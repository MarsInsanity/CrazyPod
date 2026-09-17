#!/usr/bin/env python3
"""Exercise tools/shrink-book-covers.py against epubs built here.

The parts that matter without an image tool installed: that it finds the
cover by each of the three routes, measures it, leaves a small one alone,
and -- when it does rewrite -- that what comes out is still a valid epub
with "mimetype" first and stored, and every other entry untouched.
"""
import importlib.util
import os
import struct
import subprocess
import sys
import tempfile
import zipfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, "tools", "shrink-book-covers.py")

spec = importlib.util.spec_from_file_location("shrink_book_covers", TOOL)
shrink = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shrink)


def jpeg_bytes(width, height, progressive=False):
    """A JPEG header with real dimensions. Nothing decodes it; the tool
    only ever reads as far as the frame header."""
    sof = 0xC2 if progressive else 0xC0
    frame = struct.pack(">BHHB", 8, height, width, 3) + b"\x01\x11\x00" * 3
    return (b"\xff\xd8"
            + b"\xff" + bytes([sof])
            + struct.pack(">H", len(frame) + 2) + frame
            + b"\xff\xda\x00\x08\x01\x01\x00\x00\x3f\x00"
            + b"\xff\xd9")


def png_bytes(width, height):
    header = struct.pack(">II", width, height) + b"\x08\x06\x00\x00\x00"
    chunk = b"IHDR" + header
    return (b"\x89PNG\r\n\x1a\n"
            + struct.pack(">I", len(header)) + chunk
            + struct.pack(">I", zlib.crc32(chunk)))


def build_epub(path, cover_name, cover, style, extra=None):
    """style: 'property', 'meta' or 'name' -- the three ways a cover is
    pointed at, all of which turn up in real books."""
    if style == "property":
        item = ('<item id="c" href="%s" media-type="image/jpeg" '
                'properties="cover-image"/>' % cover_name)
        meta = ""
    elif style == "meta":
        item = ('<item id="c" href="%s" media-type="image/jpeg"/>'
                % cover_name)
        meta = '<meta name="cover" content="c"/>'
    else:
        item = ""
        meta = ""
    opf = ('<?xml version="1.0"?><package><metadata>%s</metadata>'
           '<manifest>%s</manifest></package>' % (meta, item))
    container = ('<?xml version="1.0"?><container><rootfiles><rootfile '
                 'full-path="OEBPS/content.opf"/></rootfiles></container>')
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr(
            zipfile.ZipInfo("mimetype"), "application/epub+zip",
            zipfile.ZIP_STORED)
        archive.writestr("META-INF/container.xml", container)
        archive.writestr("OEBPS/content.opf", opf)
        archive.writestr("OEBPS/" + cover_name, cover)
        archive.writestr("OEBPS/chapter1.xhtml", "<html>text</html>")
        for name, data in (extra or {}).items():
            archive.writestr(name, data)


def run_tool(*arguments):
    result = subprocess.run(
        [sys.executable, TOOL] + list(arguments),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    return result.returncode, result.stdout.decode("utf-8", "replace")


def test_measuring():
    width, height, baseline = shrink.jpeg_size(jpeg_bytes(1600, 1200))
    assert (width, height, baseline) == (1600, 1200, True)
    _, _, baseline = shrink.jpeg_size(jpeg_bytes(64, 64, progressive=True))
    assert not baseline, "a progressive cover must be recognised as such"
    assert shrink.png_size(png_bytes(300, 400)) == (300, 400, True)
    assert shrink.image_size(b"not an image at all") is None


def test_finding_the_cover(directory):
    for style, name in (("property", "big.jpg"),
                        ("meta", "big.jpg"),
                        ("name", "cover.jpg")):
        path = os.path.join(directory, "%s.epub" % style)
        build_epub(path, name, jpeg_bytes(1600, 1600), style)
        with zipfile.ZipFile(path) as archive:
            found = shrink.cover_entry(archive)
        assert found == "OEBPS/" + name, (style, found)


def test_reporting(directory):
    code, output = run_tool(directory)
    assert code == 0, output
    assert "1600x1600 -> would rewrite" in output, output
    assert "would shrink   3" in output, output
    assert "Nothing was changed" in output, output


def test_small_cover_is_left_alone(directory):
    path = os.path.join(directory, "small.epub")
    build_epub(path, "cover.jpg", jpeg_bytes(120, 160), "property")
    code, output = run_tool(path, "--verbose")
    assert code == 0, output
    assert "nothing to do" in output, output
    assert "would shrink" not in output, output


def test_audiobooks_are_only_reported(directory):
    path = os.path.join(directory, "book.m4b")
    with open(path, "wb") as handle:
        handle.write(b"\x00\x00\x00\x18ftypM4A ")
    before = open(path, "rb").read()
    code, output = run_tool(path, "--shrink", "--cap", "64")
    assert "left alone" in output, output
    assert open(path, "rb").read() == before, "the m4b must not be touched"
    assert code == 0, output


def test_rewriting_keeps_the_book(directory):
    """The rewrite path, with the image step stubbed out: what is being
    checked is the archive that comes back, not the scaler."""
    path = os.path.join(directory, "rewrite.epub")
    build_epub(path, "cover.jpg", jpeg_bytes(1600, 1600), "property",
               extra={"OEBPS/notes.txt": "keep me"})
    with zipfile.ZipFile(path) as archive:
        original = dict((name, archive.read(name))
                        for name in archive.namelist())
    shrink.rewrite_epub(path, "OEBPS/cover.jpg",
                        jpeg_bytes(200, 200), backup=True)
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        assert infos[0].filename == "mimetype", [i.filename for i in infos]
        assert infos[0].compress_type == zipfile.ZIP_STORED
        assert archive.read("mimetype") == b"application/epub+zip"
        assert sorted(i.filename for i in infos) == sorted(original)
        for name, data in original.items():
            if name != "OEBPS/cover.jpg":
                assert archive.read(name) == data, name
        assert shrink.jpeg_size(archive.read("OEBPS/cover.jpg"))[:2] == \
            (200, 200)
    assert os.path.exists(path + ".bak"), "--backup must keep the original"
    with zipfile.ZipFile(path + ".bak") as archive:
        assert shrink.jpeg_size(archive.read("OEBPS/cover.jpg"))[:2] == \
            (1600, 1600)


def test_a_failed_rewrite_leaves_the_original(directory):
    path = os.path.join(directory, "intact.epub")
    build_epub(path, "cover.jpg", jpeg_bytes(1600, 1600), "property")
    before = open(path, "rb").read()
    try:
        shrink.rewrite_epub(path, "OEBPS/missing.jpg", b"", backup=False)
    except KeyError:
        pass
    assert open(path, "rb").read() == before
    leftovers = [name for name in os.listdir(directory)
                 if name.endswith(".epub") and name.startswith("tmp")]
    assert not leftovers, leftovers


def test_scale_choice():
    """libjpeg scales by eighths: the result must land under the cap, and
    never above it."""
    for source, cap in ((1600, 200), (1200, 200), (900, 200), (410, 400)):
        numerator = 8
        while numerator > 1 and source * (numerator - 1) // 8 >= cap:
            numerator -= 1
        assert source * numerator // 8 >= cap or numerator == 1
        assert source * (numerator - 1) // 8 < cap or numerator == 1


def main():
    with tempfile.TemporaryDirectory() as directory:
        test_measuring()
        test_scale_choice()
        covers = os.path.join(directory, "covers")
        os.mkdir(covers)
        test_finding_the_cover(covers)
        test_reporting(covers)
        singles = os.path.join(directory, "singles")
        os.mkdir(singles)
        test_small_cover_is_left_alone(singles)
        test_audiobooks_are_only_reported(singles)
        test_rewriting_keeps_the_book(singles)
        test_a_failed_rewrite_leaves_the_original(singles)
    print("CrazyPod book cover shrinker tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
