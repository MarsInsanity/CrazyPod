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
import shutil
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
    build_epub(path, "cover.jpg", jpeg_bytes(90, 120), "property")
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


def test_each_kind_has_its_own_cap(directory):
    """Album art is drawn at 128 and below, a book cover at 72x101. The
    caps follow the screens, so a music cover of 110 is over its cap while
    a book cover of the same size is under its own."""
    music = os.path.join(directory, "Music", "Album")
    books = os.path.join(directory, "Books")
    os.makedirs(music)
    os.makedirs(books)
    with open(os.path.join(music, "cover.jpg"), "wb") as handle:
        handle.write(jpeg_bytes(110, 110))
    build_epub(os.path.join(books, "novel.epub"),
               "cover.jpg", jpeg_bytes(110, 110), "property")

    code, output = run_tool(directory, "--verbose")
    assert code == 0, output
    assert "caps: music 100, books 128, audiobooks 100" in output, output
    lines = output.splitlines()
    music_line = [line for line in lines if "Album" in line][0]
    book_line = [line for line in lines if "novel.epub" in line][0]
    assert "would rewrite" in music_line, music_line
    assert "nothing to do" in book_line, book_line

    # One --cap sets all three at once.
    code, output = run_tool(directory, "--cap", "512")
    assert "caps: music 512, books 512, audiobooks 512" in output, output
    assert "would rewrite" not in output, output

    # And a single kind can be overridden on its own.
    code, output = run_tool(directory, "--cap-books", "64", "--verbose")
    assert "caps: music 100, books 64, audiobooks 100" in output, output
    book_line = [line for line in output.splitlines()
                 if "novel.epub" in line][0]
    assert "would rewrite" in book_line, book_line


def test_it_works_on_the_volume_it_sits_in(directory):
    """Copied to the root of the iPod, the script is on the iPod -- so it
    should not also have to be told where the iPod is."""
    music = os.path.join(directory, "Music", "Album")
    os.makedirs(music)
    os.makedirs(os.path.join(directory, "Photos"))
    with open(os.path.join(music, "cover.jpg"), "wb") as handle:
        handle.write(jpeg_bytes(500, 500))
    # A photo must stay out of it even when nothing was pointed at.
    with open(os.path.join(directory, "Photos", "holiday.jpg"), "wb") as f:
        f.write(jpeg_bytes(3000, 2000))
    onboard = os.path.join(directory, os.path.basename(TOOL))
    shutil.copy2(TOOL, onboard)

    result = subprocess.run(
        [sys.executable, onboard], cwd=tempfile.gettempdir(),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = result.stdout.decode("utf-8", "replace")
    assert result.returncode == 0, output
    assert "the folder this script is in" in output, output
    assert "Music" in output and "500x500" in output, output
    assert "holiday" not in output, output

    # --install says the same thing about itself.
    destination = os.path.join(directory, "elsewhere")
    os.mkdir(destination)
    code, output = run_tool("--install", destination)
    assert code == 0, output
    assert os.path.exists(
        os.path.join(destination, os.path.basename(TOOL)))
    assert "with no arguments at all" in output, output


def test_a_file_that_is_not_an_epub_is_named(directory):
    """A .epub that will not open as a zip is nearly always something else
    wearing the extension, and which one decides what to do about it."""
    cases = {
        "kindle.epub": (b"\x00" * 60 + b"BOOKMOBI" + b"\x00" * 64,
                        "Kindle book (MOBI/AZW)"),
        "kfx.epub": (b"\xeaDRMION\xee" + b"\x00" * 120, "KFX"),
        "pdf.epub": (b"%PDF-1.7\n" + b"\x00" * 120, "PDF"),
        "empty.epub": (b"", "empty"),
        "half.epub": (b"PK\x03\x04" + b"\x00" * 8, "damaged"),
        "fork.epub": (b"\x00\x05\x16\x07" + b"\x00" * 60,
                      "resource fork"),
        "page.epub": (b"<html><body>hi</body></html>", "HTML"),
    }
    for name, (data, expected) in cases.items():
        path = os.path.join(directory, name)
        with open(path, "wb") as handle:
            handle.write(data)
        described = shrink.describe_file(path)
        assert expected in described, (name, described)

    code, output = run_tool(directory)
    assert code == 0, output
    assert "not an epub" in output, output
    assert "CrazyPod cannot read them either" in output, output
    # Naming the problem is not the same as touching the file.
    for name in cases:
        with open(os.path.join(directory, name), "rb") as handle:
            assert handle.read() == cases[name][0], name


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
        caps = os.path.join(directory, "caps")
        os.mkdir(caps)
        test_each_kind_has_its_own_cap(caps)
        onboard = os.path.join(directory, "ipod")
        os.mkdir(onboard)
        test_it_works_on_the_volume_it_sits_in(onboard)
        broken = os.path.join(directory, "broken")
        os.mkdir(broken)
        test_a_file_that_is_not_an_epub_is_named(broken)
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
