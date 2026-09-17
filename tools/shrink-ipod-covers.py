#!/usr/bin/env python3
"""Cap the resolution of every cover on the iPod: music, books, audiobooks.

CrazyPod draws a book cover at 72x101 and album art at 128 or 180, so a
1600x1600 cover is decoded at full size and then thrown away. On a 75 MHz
ARM7 reading from a card that is the pause you feel -- eight to ten
seconds for 500x500 album art, and a Books menu that stopped for seconds
because its preview draws a stack of three.

Meant to live in the root of the iPod so it travels with the device. It
works on the volume it is sitting in, so from there it takes no
arguments at all:

  python3 /Volumes/IPOD/shrink-ipod-covers.py
  python3 /Volumes/IPOD/shrink-ipod-covers.py --shrink

It finds Music, Books and Audiobooks beside itself and leaves Photos and
Videos alone. Name a folder or a file to do only that instead. To put it
on the iPod in the first place:

  python3 tools/shrink-ipod-covers.py --install /Volumes/IPOD

Each kind is capped at what the firmware draws it at:

  album art    100   shown at 128 in Now Playing and Coverflow, 120 in
                     the menu preview, 42 in the home capsule
  book cover   128   shown at 72x101, and never larger anywhere
  audiobook    100   the same screens as album art

--cap sets all three at once; --cap-music, --cap-books and
--cap-audiobooks set one. 128 is the largest size any cover is drawn at,
so nothing above it is ever seen -- it is decoded and thrown away.

What it touches:

  images  album art and loose covers -- cover.jpg, folder.png and the
          rest -- rewritten in place. A PNG is converted to JPEG, since
          the firmware reads no other kind.
  .epub   the cover image inside the archive is rewritten. Everything
          else in the book is copied across byte for byte, and "mimetype"
          keeps its required place as the first, uncompressed entry.
  .m4b    the cover inside the file is replaced without remuxing: the
          "covr" atom is overwritten and the space it gives up is filled
          with a "free" atom, so "moov" keeps the same size to the byte
          and every chapter mark and sample offset in the file still
          points where it did. If that cannot be done safely the file is
          reported and left alone.

Output is always baseline JPEG, which is the only kind this firmware's
decoder reads -- so this clears progressive covers at the same time, and
converts PNG ones. Needs no image library for that: a PNG is decoded
here and cjpeg encodes the result.

An epub whose cover is a PNG is reported rather than rewritten. The
firmware refuses a book cover by its file name, so that cover is not
being drawn today, and fixing it properly means a second manifest entry
rather than a JPEG smuggled in under the old name.

It reports by default and changes nothing until you pass --shrink, keeps
the original beside each file it rewrites as <name>.bak unless told not
to, and reads every rewrite back and checks it before letting it replace
anything. Needs
ImageMagick, or djpeg and cjpeg from libjpeg-turbo, only when actually
shrinking; the report reads the image headers itself and needs nothing.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
import zlib

# What CrazyPod actually draws, measured from the firmware:
#
#   album art   128 in Now Playing and Coverflow, 120 in the menu
#               preview, 42 in the home capsule
#   book cover  72x101, and never larger anywhere
#   audiobook   the same path as album art, so 128
#
# So 128 is the largest size any of it is ever shown at, and anything
# above that is decoded and then thrown away. The defaults sit at or just
# above what is drawn; 100 for music is a deliberate step below 128,
# which trades a little sharpness in Now Playing for a faster decode.
DEFAULT_CAP_MUSIC = 100
DEFAULT_CAP_BOOKS = 128
DEFAULT_CAP_AUDIOBOOKS = 100
DEFAULT_QUALITY = 88
IMAGE_SUFFIXES = (".jpg", ".jpeg", ".png")
COVER_NAMES = ("cover", "folder", "front", "artwork")
# Start-of-frame markers that carry the dimensions. SOF0 is the baseline
# one; the others are listed so a progressive cover is still measured and
# still reported rather than silently skipped.
SOF_MARKERS = set(range(0xC0, 0xD0)) - {0xC4, 0xC8, 0xCC}
STANDALONE = {0x01, 0xD8, 0xD9} | set(range(0xD0, 0xD8))


class CoverError(Exception):
    pass


class MissingToolError(CoverError):
    """Raised at the first cover that actually needs scaling, rather than
    at startup: a library of audiobooks and small covers has nothing to
    scale, and refusing to run at all would be a lie about it."""


# ---- Measuring, with nothing installed -----------------------------------

def jpeg_size(data):
    """(width, height, baseline) for a JPEG held in memory."""
    offset = 2
    if len(data) < 4 or data[0] != 0xFF or data[1] != 0xD8:
        raise CoverError("not a JPEG")
    while offset + 1 < len(data):
        if data[offset] != 0xFF:
            offset += 1
            continue
        marker = data[offset + 1]
        offset += 2
        if marker == 0xFF:
            offset -= 1
            continue
        if marker in STANDALONE:
            continue
        if offset + 2 > len(data):
            break
        length = (data[offset] << 8) | data[offset + 1]
        if length < 2:
            raise CoverError("malformed JPEG segment")
        if marker in SOF_MARKERS:
            if offset + 7 > len(data):
                break
            height = (data[offset + 3] << 8) | data[offset + 4]
            width = (data[offset + 5] << 8) | data[offset + 6]
            return width, height, marker == 0xC0
        if marker == 0xDA:      # start of scan: no more headers
            break
        offset += length
    raise CoverError("no JPEG frame header")


def png_size(data):
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise CoverError("not a PNG")
    if data[12:16] != b"IHDR":
        raise CoverError("PNG does not start with IHDR")
    width = int.from_bytes(data[16:20], "big")
    height = int.from_bytes(data[20:24], "big")
    return width, height, True


def image_size(data):
    """(width, height, baseline) for a JPEG or PNG, or None if neither."""
    for reader in (jpeg_size, png_size):
        try:
            return reader(data)
        except CoverError:
            continue
    return None


# ---- PNG, without a library ----------------------------------------------
#
# The firmware decodes JPEG and BMP and nothing else, so a PNG cover is not
# shrunk, it is converted -- otherwise it would never have been drawn at
# all. djpeg cannot read PNG and ImageMagick is one more thing to install,
# so the decode is done here: PNG is zlib over filtered scanlines, which is
# a hundred lines, and cjpeg takes the pixels from there.

PNG_CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}


def png_chunks(data):
    offset = 8
    while offset + 8 <= len(data):
        length = int.from_bytes(data[offset:offset + 4], "big")
        kind = data[offset + 4:offset + 8]
        start = offset + 8
        if start + length > len(data):
            return
        yield kind, data[start:start + length]
        offset = start + length + 4


def png_unpack_bits(row, depth, count):
    """Scanline samples for bit depths below a byte."""
    out = []
    per_byte = 8 // depth
    mask = (1 << depth) - 1
    for index in range(count):
        byte = row[index // per_byte]
        shift = 8 - depth * (index % per_byte + 1)
        out.append((byte >> shift) & mask)
    return out


def png_unfilter(data, width, height, channels, depth):
    """Undo the per-scanline filters, returning raw samples per row."""
    if depth == 16:
        stride = width * channels * 2
        step = channels * 2
    else:
        stride = (width * channels * depth + 7) // 8
        step = max(1, channels * depth // 8)
    rows = []
    previous = bytearray(stride)
    offset = 0
    for _ in range(height):
        if offset + 1 + stride > len(data):
            raise CoverError("PNG scanlines are shorter than the header says")
        kind = data[offset]
        row = bytearray(data[offset + 1:offset + 1 + stride])
        offset += 1 + stride
        if kind == 1:
            for i in range(step, stride):
                row[i] = (row[i] + row[i - step]) & 0xFF
        elif kind == 2:
            for i in range(stride):
                row[i] = (row[i] + previous[i]) & 0xFF
        elif kind == 3:
            for i in range(stride):
                left = row[i - step] if i >= step else 0
                row[i] = (row[i] + ((left + previous[i]) >> 1)) & 0xFF
        elif kind == 4:
            for i in range(stride):
                left = row[i - step] if i >= step else 0
                up = previous[i]
                corner = previous[i - step] if i >= step else 0
                estimate = left + up - corner
                da = abs(estimate - left)
                db = abs(estimate - up)
                dc = abs(estimate - corner)
                if da <= db and da <= dc:
                    row[i] = (row[i] + left) & 0xFF
                elif db <= dc:
                    row[i] = (row[i] + up) & 0xFF
                else:
                    row[i] = (row[i] + corner) & 0xFF
        elif kind != 0:
            raise CoverError("unknown PNG filter %d" % kind)
        rows.append(row)
        previous = row
    return rows


def png_decode(data):
    """(width, height, rows of RGB bytes). Alpha is composited on white,
    which is what a cover on a white page would have looked like."""
    header = None
    palette = b""
    idat = bytearray()
    for kind, payload in png_chunks(data):
        if kind == b"IHDR":
            header = payload
        elif kind == b"PLTE":
            palette = payload
        elif kind == b"IDAT":
            idat += payload
        elif kind == b"IEND":
            break
    if header is None or len(header) < 13:
        raise CoverError("PNG has no header")
    width = int.from_bytes(header[0:4], "big")
    height = int.from_bytes(header[4:8], "big")
    depth = header[8]
    colour = header[9]
    if header[12] != 0:
        raise CoverError("interlaced PNG is not supported")
    if colour not in PNG_CHANNELS:
        raise CoverError("PNG colour type %d is not supported" % colour)
    if depth not in (1, 2, 4, 8, 16):
        raise CoverError("PNG bit depth %d is not supported" % depth)
    if colour in (2, 4, 6) and depth < 8:
        raise CoverError("PNG bit depth %d is not valid here" % depth)
    channels = PNG_CHANNELS[colour]
    raw = png_unfilter(zlib.decompress(bytes(idat)), width, height,
                       channels, depth)

    rows = []
    for row in raw:
        if depth == 16:
            samples = [row[i] for i in range(0, len(row), 2)]
        elif depth == 8:
            samples = list(row)
        else:
            samples = png_unpack_bits(row, depth, width * channels)
        out = bytearray()
        maximum = (1 << depth) - 1 if depth < 8 else 255
        for x in range(width):
            base = x * channels
            if colour == 3:
                index = samples[base] * 3
                if index + 2 >= len(palette):
                    raise CoverError("PNG palette is short")
                red, green, blue = palette[index:index + 3]
                alpha = 255
            elif colour == 0:
                grey = samples[base] * 255 // maximum
                red = green = blue = grey
                alpha = 255
            elif colour == 4:
                grey = samples[base]
                red = green = blue = grey
                alpha = samples[base + 1]
            elif colour == 2:
                red, green, blue = samples[base:base + 3]
                alpha = 255
            else:
                red, green, blue, alpha = samples[base:base + 4]
            if alpha != 255:
                red = (red * alpha + 255 * (255 - alpha)) // 255
                green = (green * alpha + 255 * (255 - alpha)) // 255
                blue = (blue * alpha + 255 * (255 - alpha)) // 255
            out += bytes((red, green, blue))
        rows.append(bytes(out))
    return width, height, rows


def box_resize(width, height, rows, target_width, target_height):
    """Average each destination pixel over the source pixels it covers.

    Cheap, and the right shape of cheap: going from 500 to 100 every
    source pixel lands in exactly one box, so nothing is dropped the way
    picking nearest neighbours would drop it.
    """
    out = []
    for y in range(target_height):
        y0 = y * height // target_height
        y1 = max(y0 + 1, (y + 1) * height // target_height)
        line = bytearray()
        for x in range(target_width):
            x0 = x * width // target_width
            x1 = max(x0 + 1, (x + 1) * width // target_width)
            red = green = blue = count = 0
            for sy in range(y0, y1):
                row = rows[sy]
                for sx in range(x0, x1):
                    base = sx * 3
                    red += row[base]
                    green += row[base + 1]
                    blue += row[base + 2]
                    count += 1
            line += bytes((red // count, green // count, blue // count))
        out.append(bytes(line))
    return out


def png_to_ppm(data, cap):
    """Decode, scale to the cap, and hand back a PPM for cjpeg."""
    width, height, rows = png_decode(data)
    width, height, rows = fit_within(width, height, rows, cap)
    return write_ppm(width, height, rows), width, height

# ---- Shrinking, with one of two toolchains -------------------------------

def which_tool():
    for name in ("magick", "convert"):
        if shutil.which(name):
            return ("magick", name)
    if shutil.which("djpeg") and shutil.which("cjpeg"):
        return ("libjpeg", None)
    return (None, None)


def run(command, data):
    result = subprocess.run(
        command, input=data, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    if result.returncode != 0 or not result.stdout:
        message = result.stderr.decode("utf-8", "replace").strip()
        raise CoverError(message or "%s failed" % command[0])
    return result.stdout


def shrink_with_magick(binary, data, cap, quality):
    return run([binary, "-", "-resize", "%dx%d>" % (cap, cap),
                "-strip", "-interlace", "none", "-quality", str(quality),
                "jpg:-"], data)


def libjpeg_numerator(longest, cap):
    """The eighth to decode at: the largest reduction that still leaves
    the image at or above the cap, so the resample below has pixels to
    work from rather than having to invent them.

    libjpeg stops at one eighth. That is the whole reason the second
    stage exists: a 2376-pixel cover decoded at 1/8 is still 297, which
    is well over a cap of 128, and stopping there was why running this
    twice found work to do both times.
    """
    if longest <= cap:
        return 8
    numerator = 8
    while numerator > 1 and longest * (numerator - 1) // 8 >= cap:
        numerator -= 1
    return numerator


def read_ppm(data):
    """(width, height, rows) from the binary PPM djpeg writes."""
    if data[:2] != b"P6":
        raise CoverError("djpeg did not return a PPM")
    fields = []
    offset = 2
    while len(fields) < 3:
        while offset < len(data) and data[offset:offset + 1].isspace():
            offset += 1
        if data[offset:offset + 1] == b"#":
            while offset < len(data) and data[offset] != 0x0A:
                offset += 1
            continue
        start = offset
        while offset < len(data) and not data[offset:offset + 1].isspace():
            offset += 1
        fields.append(int(data[start:offset]))
    offset += 1                       # the single whitespace byte
    width, height, maximum = fields
    if maximum != 255:
        raise CoverError("unexpected PPM depth")
    stride = width * 3
    body = data[offset:]
    if len(body) < stride * height:
        raise CoverError("PPM is shorter than its header says")
    return width, height, [body[y * stride:(y + 1) * stride]
                           for y in range(height)]


def write_ppm(width, height, rows):
    return b"P6\n%d %d\n255\n" % (width, height) + b"".join(rows)


def fit_within(width, height, rows, cap):
    """Scale to the cap exactly, or leave it alone if it already fits."""
    longest = max(width, height)
    if longest <= cap:
        return width, height, rows
    target_width = max(1, width * cap // longest)
    target_height = max(1, height * cap // longest)
    return (target_width, target_height,
            box_resize(width, height, rows, target_width, target_height))


def shrink_with_libjpeg(data, cap, quality):
    """Decode at the coarsest eighth that overshoots the cap, then
    resample the rest of the way.

    Two stages because one is not enough: the DCT scaler is fast and
    stops at an eighth, and the box filter finishes the job at any ratio.
    """
    width, height, _ = jpeg_size(data)
    numerator = libjpeg_numerator(max(width, height), cap)
    pixels = run(["djpeg", "-scale", "%d/8" % numerator, "-ppm"], data)
    width, height, rows = read_ppm(pixels)
    width, height, rows = fit_within(width, height, rows, cap)
    return encode_jpeg(write_ppm(width, height, rows), quality)


def encode_jpeg(ppm, quality):
    return run(["cjpeg", "-quality", str(quality), "-optimize",
                "-baseline"], ppm)


def shrink_image(data, cap, quality, tool):
    kind, binary = tool
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        # Not a shrink but a conversion: the firmware decodes JPEG and BMP
        # and nothing else, so a PNG cover was never drawn in the first
        # place. ImageMagick does the whole job; otherwise the decode
        # happens here and cjpeg does the rest.
        if kind == "magick":
            return shrink_with_magick(binary, data, cap, quality)
        if kind == "libjpeg":
            ppm, _, _ = png_to_ppm(data, cap)
            return encode_jpeg(ppm, quality)
    elif kind == "magick":
        return shrink_with_magick(binary, data, cap, quality)
    elif kind == "libjpeg":
        return shrink_with_libjpeg(data, cap, quality)
    raise MissingToolError(
        "this cover needs scaling and no image tool is installed.\n"
        "Install ImageMagick (brew install imagemagick, apt install "
        "imagemagick),\nor libjpeg-turbo for djpeg and cjpeg, then run "
        "this again.")


# ---- Finding the cover inside an epub ------------------------------------

def opf_name(archive):
    try:
        container = archive.read("META-INF/container.xml").decode(
            "utf-8", "replace")
    except KeyError:
        return None
    match = re.search(r'full-path="([^"]+)"', container)
    return match.group(1) if match else None


def cover_entry(archive):
    """The archive name of the cover image, by the same three routes the
    firmware's own probe takes: the package's cover-image property, the
    older <meta name="cover"> pointer, then the obvious file names."""
    package = opf_name(archive)
    names = archive.namelist()
    if package is not None:
        try:
            opf = archive.read(package).decode("utf-8", "replace")
        except KeyError:
            opf = ""
        directory = os.path.dirname(package)
        items = dict(
            (match.group(1), match.group(2)) for match in re.finditer(
                r'<item\b[^>]*id="([^"]+)"[^>]*href="([^"]+)"', opf))
        hrefs = []
        for match in re.finditer(r'<item\b[^>]*>', opf):
            tag = match.group(0)
            if "cover-image" in tag:
                href = re.search(r'href="([^"]+)"', tag)
                if href:
                    hrefs.append(href.group(1))
        meta = re.search(r'<meta\b[^>]*name="cover"[^>]*content="([^"]+)"',
                         opf)
        if meta and meta.group(1) in items:
            hrefs.append(items[meta.group(1)])
        for href in hrefs:
            candidate = os.path.normpath(
                os.path.join(directory, href)).replace(os.sep, "/")
            if candidate in names:
                return candidate
    for name in names:
        stem, extension = os.path.splitext(os.path.basename(name).lower())
        if extension in IMAGE_SUFFIXES and stem in COVER_NAMES:
            return name
    return None


def verify_epub(original, rewritten, entry, image):
    """Read the new archive back and prove it before it replaces a book.

    A book is not something to be optimistic about. Every entry the
    original had must be present, every one of them except the cover must
    come back byte for byte, the archive must pass its own CRC check, and
    "mimetype" must still be the first entry and uncompressed -- which is
    what an epub reader looks at before anything else.
    """
    with zipfile.ZipFile(original, "r") as before:
        names = before.namelist()
        expected = dict((name, before.read(name)) for name in names)
    with zipfile.ZipFile(rewritten, "r") as after:
        damaged = after.testzip()
        if damaged is not None:
            raise CoverError("the rewritten archive fails its own "
                             "checksum at %s" % damaged)
        infos = after.infolist()
        if [info.filename for info in infos] != names:
            raise CoverError("the rewritten archive has different entries")
        if "mimetype" in names:
            if infos[0].filename != "mimetype":
                raise CoverError("mimetype is no longer the first entry")
            if infos[0].compress_type != zipfile.ZIP_STORED:
                raise CoverError("mimetype is no longer uncompressed")
        for name in names:
            data = after.read(name)
            if name == entry:
                if data != image:
                    raise CoverError("the new cover did not survive")
            elif data != expected[name]:
                raise CoverError("%s changed, and should not have" % name)


def write_out(temporary, path, backup):
    """Put the rewritten file in place, durably.

    fsync before the rename because this runs on a memory card that gets
    unplugged: without it the directory entry can reach the card while the
    data behind it has not, which is a file that looks right in a listing
    and is rubble when opened.
    """
    with open(temporary, "rb+") as handle:
        handle.flush()
        os.fsync(handle.fileno())
    if backup:
        shutil.copy2(path, path + ".bak")
    os.replace(temporary, path)
    try:
        directory = os.open(os.path.dirname(os.path.abspath(path)),
                            os.O_RDONLY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
    except (OSError, AttributeError):
        # Directory fsync is not available everywhere; the file's own
        # fsync has already done the part that matters.
        pass


def rewrite_epub(path, entry, image, backup):
    """Copy the archive across with one entry replaced.

    Written beside the original, read back and checked, and only then
    renamed over it -- so a rewrite that went wrong in any way at all
    leaves the book exactly as it was.
    """
    directory = os.path.dirname(os.path.abspath(path))
    handle, temporary = tempfile.mkstemp(suffix=".epub", dir=directory)
    os.close(handle)
    try:
        with zipfile.ZipFile(path, "r") as source:
            infos = source.infolist()
            with zipfile.ZipFile(temporary, "w") as target:
                # "mimetype" must be first and stored, or readers that
                # sniff the archive stop recognising it as an epub.
                for info in sorted(
                        infos, key=lambda i: i.filename != "mimetype"):
                    data = (image if info.filename == entry
                            else source.read(info.filename))
                    copy = zipfile.ZipInfo(info.filename, info.date_time)
                    copy.compress_type = (
                        zipfile.ZIP_STORED
                        if info.filename == "mimetype"
                        else info.compress_type)
                    copy.external_attr = info.external_attr
                    copy.internal_attr = info.internal_attr
                    copy.create_system = info.create_system
                    target.writestr(copy, data)
        verify_epub(path, temporary, entry, image)
        write_out(temporary, path, backup)
    except BaseException:
        if os.path.exists(temporary):
            os.remove(temporary)
        raise


# ---- Walking the library -------------------------------------------------

def report(path, note):
    print("%s: %s" % (path, note))


def cap_for(path, options, fallback):
    """The cap follows the folder, not the extension.

    An .m4a in Music is a song and an .m4a in Audiobooks is a book, and
    the firmware treats them that way too -- so calling every .m4a an
    audiobook would measure a song's cover against the wrong number.
    """
    parts = [part.lower()
             for part in os.path.abspath(path).split(os.sep)]
    if "audiobooks" in parts:
        kind = "audiobooks"
    elif "music" in parts or "podcasts" in parts:
        kind = "music"
    else:
        kind = fallback
    return getattr(options, "cap_" + kind)


# ---- What a file that is not an epub actually is -------------------------

def describe_file(path):
    """A .epub that will not open as a zip is nearly always something
    else wearing the extension. Say which, because what to do about it
    differs -- and CrazyPod cannot read any of them either, so a book like
    this shows on the device with no title and no cover."""
    try:
        with open(path, "rb") as handle:
            head = handle.read(132)
        size = os.path.getsize(path)
    except OSError as error:
        return "cannot be opened: %s" % error

    if size == 0:
        return "the file is empty -- the copy to the device never finished"
    if head[:4] == b"PK\x03\x04" or head[:2] == b"PK":
        return ("a zip that is damaged or was copied part-way. Copy it "
                "over again")
    if head[60:68] in (b"BOOKMOBI", b"TEXtREAd"):
        return ("a Kindle book (MOBI/AZW) named .epub. Convert it to EPUB "
                "with Calibre, or re-download it as EPUB")
    if head[:8] == b"\xeaDRMION\xee" or head[:4] == b"CONT":
        return ("a Kindle KFX book, and encrypted. It cannot be converted "
                "or read -- get the book as EPUB instead")
    if head[:4] == b"%PDF":
        return "a PDF named .epub. Convert it to EPUB with Calibre"
    if head[:4] == b"\x00\x05\x16\x07":
        return ("a macOS resource fork, not a book. Safe to delete, and "
                "'dot_clean' removes them all at once")
    if head[:5].lower() in (b"<?xml", b"<html"):
        return "HTML or XML named .epub. Convert it to EPUB with Calibre"
    return ("not a format this recognises: it starts %s" %
            " ".join("%02x" % byte for byte in head[:8]))


def handle_epub(path, options, tool, counts):
    cap = options.cap_books
    try:
        with zipfile.ZipFile(path, "r") as archive:
            entry = cover_entry(archive)
            if entry is None:
                counts["no cover"] += 1
                if options.verbose:
                    report(path, "no cover image in the archive")
                return
            data = archive.read(entry)
    except (zipfile.BadZipFile, OSError):
        counts["unreadable"] += 1
        report(path, "not an epub: %s" % describe_file(path))
        return

    size = image_size(data)
    if size is None:
        counts["unreadable"] += 1
        report(path, "%s is neither JPEG nor PNG" % entry)
        return
    width, height, baseline = size
    if data[:8] == b"\x89PNG\r\n\x1a\n":
        # The firmware decodes a book cover as JPEG or BMP and refuses
        # anything else by its file name, so this cover is not being
        # drawn at all today. Writing a JPEG into the archive under the
        # old .png name would leave it refused here and broken in every
        # other reader, and doing it properly means a second manifest
        # entry and surgery on the package file. Say so, leave it alone.
        counts["png cover"] += 1
        report(path, "%dx%d PNG cover: CrazyPod draws JPEG and BMP book "
                     "covers only, so this one never appears. Convert "
                     "the cover to JPEG in Calibre" % (width, height))
        return
    needs_shrink = max(width, height) > cap
    needs_baseline = data[:2] == b"\xff\xd8" and not baseline
    if not needs_shrink and not needs_baseline:
        counts["already small"] += 1
        if options.verbose:
            report(path, "%dx%d, nothing to do" % (width, height))
        return

    reason = "%dx%d" % (width, height)
    if needs_baseline:
        reason += ", progressive"
    if not options.shrink:
        counts["would shrink"] += 1
        report(path, "%s -> would rewrite (%s)" % (reason, entry))
        return
    try:
        shrunk = shrink_image(data, cap, options.quality, tool)
        rewrite_epub(path, entry, shrunk, options.backup)
    except MissingToolError:
        raise
    except (CoverError, OSError) as error:
        counts["failed"] += 1
        report(path, "%s -> failed: %s" % (reason, error))
        return
    new_size = image_size(shrunk)
    counts["shrunk"] += 1
    report(path, "%s -> %dx%d, %d KB -> %d KB" % (
        reason,
        new_size[0] if new_size else 0,
        new_size[1] if new_size else 0,
        len(data) // 1024, len(shrunk) // 1024))


def handle_image(path, options, tool, counts):
    cap = cap_for(path, options, "music")
    try:
        with open(path, "rb") as handle:
            data = handle.read()
    except OSError as error:
        counts["unreadable"] += 1
        report(path, "cannot read: %s" % error)
        return
    size = image_size(data)
    if size is None:
        return
    width, height, baseline = size
    needs_baseline = data[:2] == b"\xff\xd8" and not baseline
    if max(width, height) <= cap and not needs_baseline:
        counts["already small"] += 1
        if options.verbose:
            report(path, "%dx%d, nothing to do" % (width, height))
        return
    if not options.shrink:
        counts["would shrink"] += 1
        report(path, "%dx%d -> would rewrite" % (width, height))
        return
    try:
        shrunk = shrink_image(data, cap, options.quality, tool)
    except MissingToolError:
        raise
    except CoverError as error:
        counts["failed"] += 1
        report(path, "%dx%d -> failed: %s" % (width, height, error))
        return
    target = os.path.splitext(path)[0] + ".jpg"
    if options.backup:
        shutil.copy2(path, path + ".bak")
    with open(target, "wb") as handle:
        handle.write(shrunk)
    if target != path:
        os.remove(path)
    counts["shrunk"] += 1
    report(path, "%dx%d -> %d KB" % (width, height, len(shrunk) // 1024))


# ---- The cover inside an m4b --------------------------------------------
#
# Replacing it without a remux is worth the care: an audiobook's chapter
# table sits in the same moov atom, and every sample offset in stco is an
# absolute position in the file. Change moov's size by one byte and all of
# them are wrong. So the new cover is written over the old one and the
# space it gives up becomes a "free" atom -- moov's size never changes,
# nothing after it moves, and the chapter marks still point where they did.

def atom_children(data, start, end):
    """Yield (type, header_end, atom_end) for each atom in a range."""
    offset = start
    while offset + 8 <= end:
        size = int.from_bytes(data[offset:offset + 4], "big")
        kind = data[offset + 4:offset + 8]
        header = 8
        if size == 1:
            if offset + 16 > end:
                return
            size = int.from_bytes(data[offset + 8:offset + 16], "big")
            header = 16
        elif size == 0:
            size = end - offset
        if size < header or offset + size > end:
            return
        yield kind, offset, offset + header, offset + size
        offset += size


def find_atom(data, start, end, wanted):
    for kind, atom_start, header_end, atom_end in atom_children(
            data, start, end):
        if kind == wanted:
            return atom_start, header_end, atom_end
    return None


def find_covr_data(data):
    """(payload_start, payload_end, atom_start, atom_end) for the cover's
    data atom, or None. atom_* bound the covr atom itself."""
    moov = find_atom(data, 0, len(data), b"moov")
    if moov is None:
        return None
    udta = find_atom(data, moov[1], moov[2], b"udta")
    if udta is None:
        return None
    meta = find_atom(data, udta[1], udta[2], b"meta")
    if meta is None:
        return None
    # meta carries a version and flags before its children, except where
    # it does not -- so look for ilst at both offsets.
    for skip in (4, 0):
        ilst = find_atom(data, meta[1] + skip, meta[2], b"ilst")
        if ilst is not None:
            break
    if ilst is None:
        return None
    covr = find_atom(data, ilst[1], ilst[2], b"covr")
    if covr is None:
        return None
    entry = find_atom(data, covr[1], covr[2], b"data")
    if entry is None:
        return None
    # data: version+flags(4) then reserved(4), then the image.
    payload = entry[1] + 8
    if payload >= entry[2]:
        return None
    return payload, entry[2], covr[0], covr[2]


def rewrite_m4b_cover(path, image, backup):
    """Overwrite the cover in place, keeping moov exactly as long."""
    with open(path, "rb") as handle:
        original = handle.read()
    data = bytearray(original)
    found = find_covr_data(data)
    if found is None:
        raise CoverError("no cover atom to replace")
    payload, payload_end, covr_start, covr_end = found
    room = payload_end - payload
    freed = room - len(image)
    if freed < 0:
        raise CoverError("the new cover is larger than the old one")
    if freed != 0 and freed < 8:
        raise CoverError("no room for the padding a smaller cover needs")
    data[payload:payload + len(image)] = image
    # The well-known type in the data atom's flags says which format the
    # bytes are: 13 JPEG, 14 PNG. Converting a PNG cover and leaving that
    # at 14 hands the decoder a JPEG and tells it to expect a PNG, so the
    # cover simply never appears.
    if image[:2] == b"\xff\xd8":
        data[payload - 8:payload - 4] = (13).to_bytes(4, "big")
    if freed:
        # Shrink covr and its data atom by what was freed, then spend the
        # same number of bytes on a free atom directly after it.
        for atom_start in (covr_start, payload - 16):
            size = int.from_bytes(
                data[atom_start:atom_start + 4], "big") - freed
            data[atom_start:atom_start + 4] = size.to_bytes(4, "big")
        cut = covr_end - freed
        data[cut:cut + 4] = freed.to_bytes(4, "big")
        data[cut + 4:cut + 8] = b"free"
        for offset in range(cut + 8, covr_end):
            data[offset] = 0
    # Prove it before it goes anywhere near the original: the cover is the
    # only thing allowed to have changed, the file may not change length,
    # and the atom tree has to still parse from end to end.
    if len(data) != len(original):
        raise CoverError("the file changed length")
    if (data[:covr_start] != original[:covr_start] or
            data[covr_end:] != original[covr_end:]):
        raise CoverError("something outside the cover changed")
    verify = find_covr_data(data)
    if verify is None or bytes(data[verify[0]:verify[0] + len(image)]) \
            != image:
        raise CoverError("the new cover did not survive")
    for kind, start, header_end, end in atom_children(data, 0, len(data)):
        if end > len(data):
            raise CoverError("the atom tree no longer adds up")

    directory = os.path.dirname(os.path.abspath(path))
    handle, temporary = tempfile.mkstemp(suffix=".m4b", dir=directory)
    try:
        with os.fdopen(handle, "wb") as out:
            out.write(data)
        write_out(temporary, path, backup)
    except BaseException:
        if os.path.exists(temporary):
            os.remove(temporary)
        raise


def handle_audiobook(path, options, tool, counts):
    cap = cap_for(path, options, "audiobooks")
    try:
        with open(path, "rb") as handle:
            data = handle.read()
    except OSError as error:
        counts["unreadable"] += 1
        report(path, "cannot read: %s" % error)
        return
    found = find_covr_data(data)
    if found is None:
        counts["no cover"] += 1
        if options.verbose:
            report(path, "no cover inside the file")
        return
    cover = data[found[0]:found[1]]
    size = image_size(cover)
    if size is None:
        counts["no cover"] += 1
        if options.verbose:
            report(path, "cover is neither JPEG nor PNG")
        return
    width, height, baseline = size
    needs_baseline = cover[:2] == b"\xff\xd8" and not baseline
    if max(width, height) <= cap and not needs_baseline:
        counts["already small"] += 1
        if options.verbose:
            report(path, "%dx%d, nothing to do" % (width, height))
        return
    reason = "%dx%d" % (width, height)
    if needs_baseline:
        reason += ", progressive"
    if not options.shrink:
        counts["would shrink"] += 1
        report(path, "%s -> would replace the cover in place" % reason)
        return
    try:
        shrunk = shrink_image(bytes(cover), cap, options.quality, tool)
        rewrite_m4b_cover(path, shrunk, options.backup)
    except MissingToolError:
        raise
    except (CoverError, OSError) as error:
        counts["failed"] += 1
        report(path, "%s -> left alone: %s" % (reason, error))
        return
    new_size = image_size(shrunk)
    counts["shrunk"] += 1
    report(path, "%s -> %dx%d, cover %d KB -> %d KB" % (
        reason,
        new_size[0] if new_size else 0,
        new_size[1] if new_size else 0,
        len(cover) // 1024, len(shrunk) // 1024))


# The folders CrazyPod reads covers from, and the ones whose images are
# the owner's pictures rather than artwork.
MEDIA_FOLDERS = ("music", "books", "audiobooks", "podcasts")
SKIP_FOLDERS = ("photos", "videos", "dcim", ".rockbox", ".crazypod")


def roots_for(path):
    """A whole iPod, or whatever was actually pointed at.

    Given the root of the device, walking all of it would drag in the
    photo library, where a 3000x2000 image is the point rather than a
    mistake. Given anything else, take it at its word.
    """
    if os.path.isfile(path):
        return [path]
    try:
        present = dict((name.lower(), name) for name in os.listdir(path))
    except OSError:
        return [path]
    media = [present[name] for name in MEDIA_FOLDERS if name in present]
    if media:
        return [os.path.join(path, name) for name in media]
    return [path]


def walk(root, options, tool, counts):
    if os.path.isfile(root):
        entries = [(os.path.dirname(root), [os.path.basename(root)])]
    else:
        entries = []
        for directory, subdirectories, files in os.walk(root):
            subdirectories[:] = [
                name for name in subdirectories
                if name.lower() not in SKIP_FOLDERS]
            entries.append((directory, files))
    for directory, files in entries:
        for name in sorted(files):
            if name.startswith("."):
                continue
            path = os.path.join(directory, name)
            lowered = name.lower()
            extension = os.path.splitext(lowered)[1]
            if extension == ".epub":
                handle_epub(path, options, tool, counts)
            elif extension in (".m4b", ".m4a"):
                handle_audiobook(path, options, tool, counts)
            elif extension in IMAGE_SUFFIXES:
                # Album art is usually cover.jpg, but a folder with one
                # image in it is that album's art whatever it is called.
                stem = os.path.splitext(lowered)[0]
                images = [other for other in files
                          if os.path.splitext(other.lower())[1]
                          in IMAGE_SUFFIXES]
                if stem in COVER_NAMES or len(images) == 1:
                    handle_image(path, options, tool, counts)


def own_volume(script):
    """The folder this script is sitting in.

    Copied to the root of the iPod, that is the iPod -- which is the whole
    point of putting it there, so it should not also have to be told where
    it is.
    """
    return os.path.dirname(os.path.abspath(script))


def install(script, destination):
    if not os.path.isdir(destination):
        print("%s: not a folder" % destination, file=sys.stderr)
        return 2
    target = os.path.join(destination, os.path.basename(script))
    shutil.copy2(script, target)
    print("Copied to %s" % target)
    print("Run it from there with no arguments at all:\n  python3 %s"
          % target)
    return 0


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "paths", nargs="*",
        help="a folder or single files. Left out, it works on the volume "
             "this script is sitting in")
    parser.add_argument(
        "--install", metavar="IPOD",
        help="copy this script to the iPod's root folder and exit")
    parser.add_argument(
        "--cap", type=int, default=None,
        help="longest side in pixels for everything at once, overriding "
             "the per-kind defaults below")
    parser.add_argument(
        "--cap-music", type=int, default=None,
        help="album art (default %d; it is drawn at 128 and below)"
             % DEFAULT_CAP_MUSIC)
    parser.add_argument(
        "--cap-books", type=int, default=None,
        help="epub covers (default %d; they are drawn at 72x101)"
             % DEFAULT_CAP_BOOKS)
    parser.add_argument(
        "--cap-audiobooks", type=int, default=None,
        help="m4b covers (default %d; same screens as album art)"
             % DEFAULT_CAP_AUDIOBOOKS)
    parser.add_argument(
        "--quality", type=int, default=DEFAULT_QUALITY,
        help="JPEG quality (default %d)" % DEFAULT_QUALITY)
    parser.add_argument(
        "--shrink", action="store_true",
        help="actually rewrite; without it nothing is changed")
    parser.add_argument(
        "--no-backup", dest="backup", action="store_false",
        help="do not keep the original alongside as <name>.bak. These "
             "are books and music, so a backup is kept by default")
    parser.add_argument(
        "--verbose", action="store_true",
        help="mention the books that were already small enough")
    options = parser.parse_args()

    if options.install:
        return install(os.path.abspath(__file__), options.install)
    if not options.paths:
        options.paths = [own_volume(__file__)]
        print("Looking in %s, the folder this script is in."
              % options.paths[0])
    for kind, fallback in (("music", DEFAULT_CAP_MUSIC),
                           ("books", DEFAULT_CAP_BOOKS),
                           ("audiobooks", DEFAULT_CAP_AUDIOBOOKS)):
        name = "cap_" + kind
        if getattr(options, name) is None:
            setattr(options, name,
                    options.cap if options.cap is not None else fallback)
        if getattr(options, name) < 32:
            parser.error("a cap below 32 pixels would not be a cover")
    print("caps: music %d, books %d, audiobooks %d" % (
        options.cap_music, options.cap_books, options.cap_audiobooks))
    tool = which_tool()

    counts = dict((key, 0) for key in (
        "shrunk", "would shrink", "already small", "no cover",
        "unreadable", "failed", "png cover"))
    try:
        for path in options.paths:
            if not os.path.exists(path):
                print("%s: no such path" % path, file=sys.stderr)
                return 2
            for root in roots_for(path):
                walk(root, options, tool, counts)
    except MissingToolError as error:
        print("\n%s" % error, file=sys.stderr)
        return 2

    print("")
    for key in ("shrunk", "would shrink", "already small", "no cover",
                "png cover", "unreadable", "failed"):
        if counts[key]:
            print("%-14s %d" % (key, counts[key]))
    if counts["unreadable"]:
        print("\nThe unreadable ones are not epubs at all. CrazyPod "
              "cannot read them either:\nthey appear in Books with no "
              "title and no cover. Convert or remove them.")
    if not options.shrink and counts["would shrink"]:
        print("\nNothing was changed. Pass --shrink to rewrite these.")
    return 1 if counts["failed"] else 0


if __name__ == "__main__":
    sys.exit(main())
