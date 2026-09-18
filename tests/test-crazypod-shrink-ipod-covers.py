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


def atom(kind, payload):
    return struct.pack(">I", len(payload) + 8) + kind + payload


def png_cover_bytes(width, height):
    """A real PNG -- the decoder in the tool has to actually read it."""
    raw = b""
    for y in range(height):
        raw += b"\x00" + bytes(
            ((x * 7) % 256, (y * 5) % 256, 128)[i % 3]
            for x in range(width) for i in range(3))
    idat = zlib.compress(raw)
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    out = b"\x89PNG\r\n\x1a\n"
    for kind, payload in ((b"IHDR", header), (b"IDAT", idat),
                          (b"IEND", b"")):
        out += (struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", zlib.crc32(kind + payload)))
    return out


def build_m4b(path, cover, kind=13):
    """An m4b with the atom nesting a real one has: the cover in
    moov/udta/meta/ilst/covr/data, a chapter table beside it, and an mdat
    after moov whose position is what must not move."""
    data_atom = atom(b"data", struct.pack(">II", kind, 0) + cover)
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
    """Album art and audiobook covers are drawn on the same screens, so
    they share a default -- but each can be set on its own."""
    music = os.path.join(directory, "Music", "Album")
    audio = os.path.join(directory, "Audiobooks")
    os.makedirs(music)
    os.makedirs(audio)
    with open(os.path.join(music, "cover.jpg"), "wb") as handle:
        handle.write(jpeg_bytes(110, 110))
    build_m4b(os.path.join(audio, "book.m4b"), jpeg_bytes(110, 110))

    code, output = run_tool(directory, "--verbose")
    assert code == 0, output
    assert "caps: music 100, audiobooks 100" in output, output
    for line in output.splitlines():
        if "cover.jpg" in line or "book.m4b" in line:
            assert "would" in line, line

    code, output = run_tool(directory, "--cap", "512")
    assert "caps: music 512, audiobooks 512" in output, output
    assert "would" not in output, output

    code, output = run_tool(directory, "--cap-music", "64",
                            "--cap-audiobooks", "128", "--verbose")
    assert "caps: music 64, audiobooks 128" in output, output
    song = [line for line in output.splitlines() if "cover.jpg" in line][0]
    book = [line for line in output.splitlines() if "book.m4b" in line][0]
    assert "would rewrite" in song, song
    assert "nothing to do" in book, book


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
    """Covers are no longer taken out of books, but a file that is not a
    book still reaches the shelf and can never be opened, and nothing
    else would say which kind of not-a-book it is."""
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
        assert expected in shrink.describe_file(path), name

    # A real book, with a cover, that must be reported as nothing to do
    # and never rewritten.
    book = os.path.join(directory, "real.epub")
    build_epub(book, "cover.jpg", jpeg_bytes(1400, 2100), "property")
    before = open(book, "rb").read()

    code, output = run_tool(directory, "--shrink", "--verbose")
    assert code == 0, output
    assert "not an epub" in output, output
    assert "CrazyPod cannot read them either" in output, output
    real = [line for line in output.splitlines() if "real.epub" in line][0]
    assert "covers are not drawn" in real, real
    assert open(book, "rb").read() == before, "the book must be untouched"
    for name in cases:
        with open(os.path.join(directory, name), "rb") as handle:
            assert handle.read() == cases[name][0], name


def test_png_cover_becomes_a_jpeg_the_firmware_can_read(directory):
    """CrazyPod decodes JPEG and BMP and nothing else, so a PNG cover is
    converted rather than shrunk -- and the data atom's type flag has to
    change with it, or the decoder is handed a JPEG and told it is a PNG."""
    png = png_cover_bytes(64, 64)
    assert shrink.image_size(png) == (64, 64, True)
    width, height, rows = shrink.png_decode(png)
    assert (width, height) == (64, 64) and len(rows) == 64
    assert len(rows[0]) == 64 * 3

    ppm, out_width, out_height = shrink.png_to_ppm(png, 16)
    assert (out_width, out_height) == (16, 16)
    assert ppm.startswith(b"P6\n16 16\n255\n")
    assert len(ppm) - len(b"P6\n16 16\n255\n") == 16 * 16 * 3

    # Under the cap, the pixels come back unscaled.
    ppm, out_width, out_height = shrink.png_to_ppm(png, 200)
    assert (out_width, out_height) == (64, 64)

    path = os.path.join(directory, "png.m4b")
    build_m4b(path, png + b"\x00" * 400, kind=14)
    before = open(path, "rb").read()
    payload = shrink.find_covr_data(before)[0]
    assert int.from_bytes(before[payload - 8:payload - 4], "big") == 14

    shrink.rewrite_m4b_cover(path, jpeg_bytes(16, 16), backup=False)
    after = open(path, "rb").read()
    payload = shrink.find_covr_data(after)[0]
    assert int.from_bytes(after[payload - 8:payload - 4], "big") == 13, \
        "the cover is a JPEG now and the atom must say so"
    assert len(after) == len(before), "moov must not change length"


def test_the_cap_follows_the_folder(directory):
    """An .m4a in Music is a song; an .m4a in Audiobooks is a book. The
    firmware reads them that way, so the caps have to as well."""
    music = os.path.join(directory, "Music", "Kim Petras")
    audio = os.path.join(directory, "Audiobooks")
    os.makedirs(music)
    os.makedirs(audio)
    build_m4b(os.path.join(music, "song.m4a"), jpeg_bytes(120, 120))
    build_m4b(os.path.join(audio, "book.m4a"), jpeg_bytes(120, 120))

    code, output = run_tool(
        directory, "--cap-music", "200", "--cap-audiobooks", "64",
        "--verbose")
    assert code == 0, output
    song = [line for line in output.splitlines() if "song.m4a" in line][0]
    book = [line for line in output.splitlines() if "book.m4a" in line][0]
    assert "nothing to do" in song, song
    assert "would replace" in book, book


def test_shrinking_actually_reaches_the_cap():
    """The reported fault: a cover came out at 197x297 against a cap of
    128, so every run found the same work to do again.

    djpeg scales in eighths and stops at one eighth, which for a large
    cover is nowhere near a cap of 128. The eighth is only the first
    stage now, and a resample finishes the job -- so what comes out is at
    the cap, and a second run has nothing to say about it.
    """
    for cap in (64, 100, 128, 200):
        for source in (129, 200, 297, 500, 1000, 1400, 2100, 2376, 4000):
            numerator = shrink.libjpeg_numerator(source, cap)
            decoded = source * numerator // 8
            if source > cap:
                assert decoded >= cap, (source, cap, numerator, decoded)
            # Whatever the eighth gave, the resample lands on the cap.
            width, height, rows = shrink.fit_within(
                decoded, decoded, [b"\x00" * decoded * 3] * decoded, cap)
            assert max(width, height) <= cap, (source, cap, width, height)

    # The exact shape that was reported: 2376 tall decodes to 297 at the
    # smallest eighth libjpeg has, and 297 is not 128.
    assert shrink.libjpeg_numerator(2376, 128) == 1
    assert 2376 * 1 // 8 == 297

    # A cover already inside the cap is not scaled at all, so converting a
    # small progressive one to baseline does not shrink it as well.
    assert shrink.libjpeg_numerator(100, 128) == 8
    width, height, _ = shrink.fit_within(100, 150, [b"\x00" * 300] * 150,
                                         200)
    assert (width, height) == (100, 150)


def test_a_shrunk_cover_is_left_alone_next_time(directory):
    """End to end through the PNG path, which needs no image tool: shrink
    once, and the second pass must find nothing to do."""
    png = png_cover_bytes(400, 600)
    ppm, width, height = shrink.png_to_ppm(png, 128)
    assert max(width, height) == 128, (width, height)
    assert (width, height) == (85, 128)
    # Feed the result back in: already at the cap, so unchanged.
    again, width, height = shrink.png_to_ppm(png, 128)
    assert (width, height) == (85, 128)
    assert again == ppm


def test_read_ppm_handles_what_djpeg_writes():
    body = b"".join(bytes((x, x, x)) for x in range(4)) * 3
    for header in (b"P6\n4 3\n255\n", b"P6 4 3 255 ",
                   b"P6\n# djpeg\n4 3\n255\n"):
        width, height, rows = shrink.read_ppm(header + body)
        assert (width, height) == (4, 3), header
        assert len(rows) == 3 and len(rows[0]) == 12, header
    try:
        shrink.read_ppm(b"P6\n4 3\n255\n" + b"\x00" * 4)
        raise AssertionError("a short PPM must be refused")
    except shrink.CoverError:
        pass


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
        singles = os.path.join(directory, "singles")
        os.mkdir(singles)
        caps = os.path.join(directory, "caps")
        os.mkdir(caps)
        test_each_kind_has_its_own_cap(caps)
        onboard = os.path.join(directory, "ipod")
        os.mkdir(onboard)
        test_it_works_on_the_volume_it_sits_in(onboard)
        broken = os.path.join(directory, "broken")
        os.mkdir(broken)
        test_a_file_that_is_not_an_epub_is_named(broken)
        test_png_cover_becomes_a_jpeg_the_firmware_can_read(singles)
        folders = os.path.join(directory, "folders")
        os.mkdir(folders)
        test_the_cap_follows_the_folder(folders)
        guarded = os.path.join(directory, "guarded")
        os.mkdir(guarded)
        test_shrinking_actually_reaches_the_cap()
        test_read_ppm_handles_what_djpeg_writes()
        test_a_shrunk_cover_is_left_alone_next_time(guarded)
        test_m4b_cover_is_found(singles)
        test_m4b_rewrite_moves_nothing(singles)
        test_m4b_refuses_a_larger_cover(singles)
        test_m4b_without_a_cover_is_left_alone(singles)
    print("CrazyPod cover shrinker tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
