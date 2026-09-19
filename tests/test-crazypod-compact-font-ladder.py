#!/usr/bin/env python3
"""The compact type ladder has to land on faces the font pack builds.

On the Mini every font request is rewritten to a smaller size by
compact_size() in crazypod_runtime_font.c. Ask for a size that
tools/crazypod-runtime-font-specs.txt does not list for that family and
weight and the resolver finds no file: the screen falls back to a built-in
bitmap face, silently, for that one role. That is not something a build
failure catches, so the two are checked against each other here.
"""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
SPEC_FILE = ROOT / "tools/crazypod-runtime-font-specs.txt"
RUNTIME_SOURCE = ROOT / "apps/crazypod/crazypod_runtime_font.c"
WIDGETS_SOURCE = ROOT / "apps/crazypod/ui/presentation/crazypod_ui_widgets.c"

SPEC_PATTERN = re.compile(
    r"(system|serif|mono):(\d{3}):(\d{1,2})(?::(full|compact))?")
LADDER_ENTRY = re.compile(r"\{\s*(\d+),\s*(\d+)\s*\}")


def load_specs():
    """The pack split by canvas: what a request may name, what may answer it.

    A line without a canvas is in both sets. `full` marks a face only the
    320x240 packages carry, which is where the design's own sizes live;
    `compact` marks one only the Mini carries, which is where the rewritten
    sizes land.
    """
    requested = set()
    available = set()
    for line_number, raw_line in enumerate(
            SPEC_FILE.read_text(encoding="ascii").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        match = SPEC_PATTERN.fullmatch(line)
        if match is None:
            raise SystemExit(
                f"invalid runtime font spec at {SPEC_FILE}:{line_number}: "
                f"{line}")
        spec = (match.group(1), int(match.group(2)), int(match.group(3)))
        canvas = match.group(4)
        if canvas != "compact":
            requested.add(spec)
        if canvas != "full":
            available.add(spec)
    return requested, available


def load_ladder():
    """The table compact_size() walks, plus the size it falls through to."""
    source = RUNTIME_SOURCE.read_text(encoding="utf-8")
    start = source.index("static unsigned compact_size(")
    end = source.index("\n}", start)
    body = source[start:end]
    table_start = body.index("} ladder[] = {")
    table_end = body.index("};", table_start)
    entries = [(int(a), int(b))
               for a, b in LADDER_ENTRY.findall(body[table_start:table_end])]
    if not entries:
        raise SystemExit("compact_size() has no ladder entries")
    fallthrough = re.search(r"return\s+(\d+);\s*$", body.strip())
    if fallthrough is None:
        raise SystemExit("compact_size() has no fall-through size")
    return entries, int(fallthrough.group(1))


def compact_size(entries, fallthrough, size):
    for step, compact in entries:
        if size <= step:
            return compact
    return fallthrough


def main():
    requested, available = load_specs()
    entries, fallthrough = load_ladder()

    steps = [size for size, _ in entries]
    if steps != sorted(steps):
        raise SystemExit(
            "compact_size() walks its ladder in order and stops at the first "
            f"entry it fits, so the requested sizes must ascend: {steps}")

    produced = [compact for _, compact in entries] + [fallthrough]
    if produced != sorted(produced):
        raise SystemExit(
            "the compact ladder must not shrink a larger request below a "
            f"smaller one: {produced}")

    missing = set()
    for family, weight, size in sorted(requested):
        wanted = compact_size(entries, fallthrough, size)
        if (family, weight, wanted) not in available:
            missing.add(f"{family}:{weight}:{wanted} "
                        f"(needed by {family}:{weight}:{size})")
    if missing:
        raise SystemExit(
            "the compact ladder asks for faces the Mini package does not "
            "carry:\n  " + "\n  ".join(sorted(missing)))

    unreachable = sorted(
        f"{family}:{weight}:{size}" for family, weight, size in available
        if (family, weight, size) not in requested
        and not any(compact_size(entries, fallthrough, other) == size
                    and (family, weight) == (other_family, other_weight)
                    for other_family, other_weight, other in requested))
    if unreachable:
        raise SystemExit(
            "the Mini package carries faces nothing can ask for:\n  "
            + "\n  ".join(unreachable))

    # The widget layer rewrites the built-in Montserrat sizes into runtime
    # requests before anything else sees them; those entry points have to be
    # covered too.
    widgets = WIDGETS_SOURCE.read_text(encoding="utf-8")
    for size in {int(value) for value in
                 re.findall(r"crazypod_runtime_font_at_size\((\d+)\)",
                            widgets)}:
        wanted = compact_size(entries, fallthrough, size)
        if ("system", 400, wanted) not in available:
            raise SystemExit(
                f"widget text at {size}px maps to system:400:{wanted}, "
                "which the font pack does not build")

    print(f"CrazyPod compact font ladder: {len(entries)} steps, "
          f"{len(requested)} requestable faces, {len(available)} on the Mini, "
          "every mapping covered")
    return 0


if __name__ == "__main__":
    sys.exit(main())
