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
          rest -- rewritten in place.
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
decoder reads -- so this clears progressive covers at the same time.

It reports by default and changes nothing until you pass --shrink. Needs
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


def shrink_with_libjpeg(data, cap, quality):
    """DCT-scaled decode, which is both faster and cleaner than resampling.

    libjpeg scales by eighths, so this picks the largest eighth that still
    lands inside the cap -- the result can be smaller than the cap, never
    larger.
    """
    width, height, _ = jpeg_size(data)
    longest = max(width, height)
    numerator = 8
    while numerator > 1 and longest * (numerator - 1) // 8 >= cap:
        numerator -= 1
    if numerator == 8:
        raise CoverError("already within the cap")
    pixels = run(["djpeg", "-scale", "%d/8" % numerator, "-ppm"], data)
    return run(["cjpeg", "-quality", str(quality), "-optimize",
                "-baseline"], pixels)


def shrink_image(data, cap, quality, tool):
    kind, binary = tool
    if kind == "magick":
        return shrink_with_magick(binary, data, cap, quality)
    if kind == "libjpeg":
        if data[:2] != b"\xff\xd8":
            raise CoverError(
                "djpeg reads JPEG only; install ImageMagick for PNG covers")
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


def rewrite_epub(path, entry, image, backup):
    """Copy the archive across with one entry replaced.

    Written beside the original and renamed over it, so an interrupted run
    leaves the book as it was rather than half a file.
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
        if backup:
            shutil.copy2(path, path + ".bak")
        os.replace(temporary, path)
    except BaseException:
        if os.path.exists(temporary):
            os.remove(temporary)
        raise


# ---- Walking the library -------------------------------------------------

def report(path, note):
    print("%s: %s" % (path, note))


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
    except (zipfile.BadZipFile, OSError) as error:
        counts["unreadable"] += 1
        report(path, "cannot read: %s" % error)
        return

    size = image_size(data)
    if size is None:
        counts["unreadable"] += 1
        report(path, "%s is neither JPEG nor PNG" % entry)
        return
    width, height, baseline = size
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
    cap = options.cap_music
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
        data = bytearray(handle.read())
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
    directory = os.path.dirname(os.path.abspath(path))
    handle, temporary = tempfile.mkstemp(suffix=".m4b", dir=directory)
    try:
        with os.fdopen(handle, "wb") as out:
            out.write(data)
        if backup:
            shutil.copy2(path, path + ".bak")
        os.replace(temporary, path)
    except BaseException:
        if os.path.exists(temporary):
            os.remove(temporary)
        raise


def handle_audiobook(path, options, tool, counts):
    cap = options.cap_audiobooks
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
        "--backup", action="store_true",
        help="keep the original alongside as <name>.bak")
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
        "unreadable", "failed", "audiobook"))
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
                "audiobook", "unreadable", "failed"):
        if counts[key]:
            print("%-14s %d" % (key, counts[key]))
    if not options.shrink and counts["would shrink"]:
        print("\nNothing was changed. Pass --shrink to rewrite these.")
    return 1 if counts["failed"] else 0


if __name__ == "__main__":
    sys.exit(main())
