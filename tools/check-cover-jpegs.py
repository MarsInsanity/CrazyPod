#!/usr/bin/env python3
"""Report which cover JPEGs this firmware's decoder can actually read.

Rockbox decodes baseline JPEG only. Every other start-of-frame -- most
often progressive, which is what the Cover Art Archive usually serves and
what Picard therefore writes -- is refused outright, and no retry at a
smaller size changes that. The artwork simply never appears.

Reads the marker bytes directly, so it needs nothing installed. With
--convert it calls jpegtran, which rewrites progressive to baseline
losslessly: the pixels are identical, only the coefficient ordering
changes, so nothing is re-compressed and no quality is lost.

  python3 tools/check-cover-jpegs.py /Volumes/IPOD/Music
  python3 tools/check-cover-jpegs.py /Volumes/IPOD/Music --convert
"""
import argparse
import os
import shutil
import subprocess
import sys

# Start-of-frame markers. Only SOF0 is baseline; the rest are what the
# decoder returns -4 for.
SOF_NAMES = {
    0xC0: ("baseline", True),
    0xC1: ("extended sequential", False),
    0xC2: ("progressive", False),
    0xC3: ("lossless", False),
    0xC5: ("differential sequential", False),
    0xC6: ("differential progressive", False),
    0xC7: ("differential lossless", False),
    0xC9: ("arithmetic sequential", False),
    0xCA: ("arithmetic progressive", False),
    0xCB: ("arithmetic lossless", False),
    0xCD: ("arithmetic differential sequential", False),
    0xCE: ("arithmetic differential progressive", False),
    0xCF: ("arithmetic differential lossless", False),
}
# Markers that carry no length field and so cannot be skipped by one.
STANDALONE = {0x01, 0xD8, 0xD9} | set(range(0xD0, 0xD8))


def frame_kind(path):
    """The file's SOF marker as (description, readable), or an error."""
    try:
        with open(path, "rb") as handle:
            data = handle.read(1 << 16)
    except OSError as error:
        return ("unreadable: %s" % error.strerror, False)
    if len(data) < 4 or data[0] != 0xFF or data[1] != 0xD8:
        return ("not a JPEG", False)
    offset = 2
    while offset + 3 < len(data):
        if data[offset] != 0xFF:
            offset += 1           # padding between segments is legal
            continue
        marker = data[offset + 1]
        if marker == 0xFF:
            offset += 1
            continue
        if marker in STANDALONE:
            offset += 2
            continue
        if marker in SOF_NAMES:
            return SOF_NAMES[marker]
        if marker == 0xDA:        # start of scan: no frame header found
            break
        offset += 2 + int.from_bytes(data[offset + 2:offset + 4], "big")
    return ("no frame header", False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", help="folder to scan, searched recursively")
    parser.add_argument("--convert", action="store_true",
                        help="rewrite unreadable files as baseline with "
                             "jpegtran, losslessly and in place")
    options = parser.parse_args()

    jpegtran = shutil.which("jpegtran")
    if options.convert and jpegtran is None:
        sys.exit("jpegtran not found. It ships with libjpeg-turbo:\n"
                 "  macOS    brew install jpeg-turbo\n"
                 "  Debian   sudo apt install libjpeg-turbo-progs\n"
                 "  Windows  included in the libjpeg-turbo installer")

    readable = 0
    problems = []
    for folder, _, names in os.walk(options.root):
        for name in sorted(names):
            if not name.lower().endswith((".jpg", ".jpeg")):
                continue
            path = os.path.join(folder, name)
            kind, ok = frame_kind(path)
            if ok:
                readable += 1
                continue
            problems.append((path, kind))

    for path, kind in problems:
        print("%-22s %s" % (kind, path))
    print("\n%d readable, %d the decoder will refuse" %
          (readable, len(problems)))

    if not problems or not options.convert:
        if problems:
            print("Re-run with --convert to rewrite them as baseline.")
        return

    converted = 0
    for path, _ in problems:
        temporary = path + ".baseline"
        try:
            subprocess.run([jpegtran, "-copy", "all", "-outfile",
                            temporary, path], check=True)
            os.replace(temporary, path)
            converted += 1
        except (subprocess.CalledProcessError, OSError) as error:
            print("could not convert %s: %s" % (path, error))
            if os.path.exists(temporary):
                os.remove(temporary)
    print("converted %d" % converted)


if __name__ == "__main__":
    main()
