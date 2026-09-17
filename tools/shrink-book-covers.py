#!/usr/bin/env python3
"""Cap the resolution of the cover images inside books on the device.

The firmware draws a book cover at 72x101 and never larger, so a 1600x1600
cover is decoded at full size and thrown away -- on a 75 MHz ARM7 reading
from a card, that is the pause you feel when the selection settles on a
book. Shrinking the cover in the file is the same trick that worked for
the music library, where 500x500 art took eight to ten seconds and 115x115
was immediate.

What it does:

  .epub   rewrites the cover image inside the archive. Everything else in
          the book is copied across byte for byte, and "mimetype" keeps
          its required place as the first, uncompressed entry.
  images  a loose cover.jpg / folder.png beside a book is rewritten in
          place.
  .m4b    reported, never touched. An audiobook's cover lives in a "covr"
          atom beside its chapter table, and every safe way to rewrite
          that needs a remux this script will not do behind your back --
          it would be your chapter marks at risk, not a cover.

Output is always baseline JPEG, which is the only kind this firmware's
decoder reads.

  python3 tools/shrink-book-covers.py /Volumes/IPOD/Books
  python3 tools/shrink-book-covers.py /Volumes/IPOD/Books --shrink
  python3 tools/shrink-book-covers.py /Volumes/IPOD/Books --shrink --cap 160

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

DEFAULT_CAP = 200
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
    needs_shrink = max(width, height) > options.cap
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
        shrunk = shrink_image(data, options.cap, options.quality, tool)
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
    if max(width, height) <= options.cap and not needs_baseline:
        counts["already small"] += 1
        if options.verbose:
            report(path, "%dx%d, nothing to do" % (width, height))
        return
    if not options.shrink:
        counts["would shrink"] += 1
        report(path, "%dx%d -> would rewrite" % (width, height))
        return
    try:
        shrunk = shrink_image(data, options.cap, options.quality, tool)
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


def handle_audiobook(path, counts):
    counts["audiobook"] += 1
    report(path, "audiobook: cover is inside the file, left alone")


def walk(root, options, tool, counts):
    if os.path.isfile(root):
        entries = [(os.path.dirname(root), [os.path.basename(root)])]
    else:
        entries = [(directory, files)
                   for directory, _, files in os.walk(root)]
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
                handle_audiobook(path, counts)
            elif extension in IMAGE_SUFFIXES:
                stem = os.path.splitext(lowered)[0]
                if stem in COVER_NAMES:
                    handle_image(path, options, tool, counts)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "paths", nargs="+",
        help="a Books folder, or single books")
    parser.add_argument(
        "--cap", type=int, default=DEFAULT_CAP,
        help="longest side in pixels (default %d; the firmware draws a "
             "cover at 72x101)" % DEFAULT_CAP)
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

    if options.cap < 32:
        parser.error("a cap below 32 pixels would not be a cover")
    tool = which_tool()

    counts = dict((key, 0) for key in (
        "shrunk", "would shrink", "already small", "no cover",
        "unreadable", "failed", "audiobook"))
    try:
        for path in options.paths:
            if not os.path.exists(path):
                print("%s: no such path" % path, file=sys.stderr)
                return 2
            walk(path, options, tool, counts)
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
