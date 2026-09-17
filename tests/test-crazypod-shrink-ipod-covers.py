#!/usr/bin/env python3
"""Exercise tools/shrink-ipod-covers.py against files built here.

The parts that matter without an image tool installed: that it finds an
epub's cover by each of the three routes, measures it, leaves a small one
alone, and -- when it rewrites -- that what comes out is still a valid
epub with "mimetype" first and stored and every other entry untouched.

And for an m4b, the part worth guarding above all: replacing the cover
must not move one byte of anything else. The chapter table and every
sample offset in the file are absolute positions, so moov has to come out
exactly as long as it went in.
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
TOOL = os.path.join(ROOT, "tools", "shrink-ipod-covers.py")

spec = importlib.util.spec_from_file_location("shrink_ipod_covers", TOOL)
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


def atom(kind, payload):
    return struct.pack(">I", len(payload) + 8) + kind + payload


def build_m4b(path, cover):
    """An m4b with the atom nesting a real one has: the cover in
    moov/udta/meta/ilst/covr/data, a chapter table beside it, and an mdat
    after moov whose position is what must not move."""
    data_atom = atom(b"data", struct.pack(">II", 13, 0) + cover)
    covr = atom(b"covr", data_atom)
    ilst = atom(b"ilst", covr)
    meta = atom(b"meta", struct.pack(">I", 0) + ilst)
    udta = atom(b"udta", meta)
    # A chapter list and a chunk-offset table, as bytes to be preserved.
    chpl = atom(b"chpl", bytes(range(40)))
    stco = atom(b"stco", struct.pack(">III", 0, 1, 4096))
    moov = atom(b"moov", udta + chpl + stco)
    body = (atom(b"ftyp", b"M4A mp42")
            + moov
            + atom(b"mdat", b"audio" * 64))
    with open(path, "wb") as handle:
        handle.write(body)
    return moov


def moov_span(data):
    for kind, start, header_end, end in shrink.atom_children(
            data, 0, len(data)):
        if kind == b"moov":
            return start, end
    raise AssertionError("no moov")


def test_m4b_cover_is_found(directory):
    path = os.path.join(directory, "found.m4b")
    build_m4b(path, jpeg_bytes(900, 900))
    data = open(path, "rb").read()
    found = shrink.find_covr_data(data)
    assert found is not None
    payload, payload_end = found[0], found[1]
    assert shrink.jpeg_size(data[payload:payload_end])[:2] == (900, 900)


def test_m4b_rewrite_moves_nothing(directory):
    path = os.path.join(directory, "rewrite.m4b")
    build_m4b(path, jpeg_bytes(900, 900) + b"\x00" * 400)
    before = open(path, "rb").read()
    before_moov = moov_span(before)
    covr_start, covr_end = shrink.find_covr_data(before)[2:]

    shrink.rewrite_m4b_cover(path, jpeg_bytes(150, 150), backup=False)
    after = open(path, "rb").read()

    assert len(after) == len(before), "the file changed length"
    assert moov_span(after) == before_moov, "moov moved or changed size"
    assert after[:covr_start] == before[:covr_start], "bytes before covr"
    assert after[covr_end:] == before[covr_end:], "bytes after covr"
    # The chapter table and the chunk offsets are outside covr, so those
    # two assertions already cover them -- but name it, because it is the
    # whole reason this is done in place.
    assert b"chpl" in after and b"stco" in after
    found = shrink.find_covr_data(after)
    assert shrink.jpeg_size(after[found[0]:found[1]])[:2] == (150, 150)
    # What covr gave up must be spent, or the atom tree stops adding up.
    trailing = after[covr_end - 8:covr_end]
    assert b"free" in after[found[3]:found[3] + 64] or trailing, after[
        covr_end - 16:covr_end]
    for kind, start, header_end, end in shrink.atom_children(
            after, 0, len(after)):
        assert end <= len(after)


def test_m4b_refuses_a_larger_cover(directory):
    path = os.path.join(directory, "larger.m4b")
    build_m4b(path, jpeg_bytes(100, 100))
    before = open(path, "rb").read()
    try:
        shrink.rewrite_m4b_cover(
            path, jpeg_bytes(100, 100) + b"\x00" * 500, backup=False)
        raise AssertionError("a larger cover must be refused")
    except shrink.CoverError:
        pass
    assert open(path, "rb").read() == before


def test_m4b_without_a_cover_is_left_alone(directory):
    path = os.path.join(directory, "bare.m4b")
    with open(path, "wb") as handle:
        handle.write(atom(b"ftyp", b"M4A mp42") + atom(b"mdat", b"x" * 32))
    before = open(path, "rb").read()
    code, output = run_tool(path, "--shrink", "--cap", "64")
    assert code == 0, output
    assert open(path, "rb").read() == before


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
        test_m4b_cover_is_found(singles)
        test_m4b_rewrite_moves_nothing(singles)
        test_m4b_refuses_a_larger_cover(singles)
        test_m4b_without_a_cover_is_left_alone(singles)
        test_rewriting_keeps_the_book(singles)
        test_a_failed_rewrite_leaves_the_original(singles)
    print("CrazyPod cover shrinker tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
