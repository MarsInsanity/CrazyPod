#!/bin/sh
set -eu

cd "$(dirname "$0")"

detect_jobs() {
    if [ -n "${JOBS:-}" ]; then
        printf '%s\n' "$JOBS"
    elif command -v getconf >/dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN 2>/dev/null || printf '%s\n' 4
    elif command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu 2>/dev/null || printf '%s\n' 4
    else
        printf '%s\n' 4
    fi
}

require_tools() {
    missing=0
    for tool in make perl "${CROSS_COMPILE}gcc" \
        "${CROSS_COMPILE}objcopy"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "Error: missing required tool '$tool' on PATH." >&2
            missing=1
        fi
    done
    [ "$missing" -eq 0 ] || exit 2
}

incremental=0
case "${CRAZYPOD_INCREMENTAL:-}" in
    1|yes|true|YES|TRUE) incremental=1 ;;
esac

while [ "$#" -gt 0 ]; do
    case "$1" in
        -i|--incremental)
            incremental=1
            ;;
        -h|--help)
            cat <<'EOF'
Usage: build-bootloader.sh [-i|--incremental]
                           [--target ipod6g|ipodvideo|ipodmini2g]

Builds the CrazyPod bootloader. Defaults to ipod6g.

How the result is installed differs by model, and the difference matters.
The 6G takes its bootloader over DFU with mks5lboot. The Video and the
Mini take theirs with ipodpatcher, which rewrites the firmware partition
in place; read the Rockbox manual for that model before using one.

Environment:
  CRAZYPOD_TARGET=name    same as --target
  CRAZYPOD_INCREMENTAL=1  reuse build-bootloader-<target>/
  CRAZYPOD_SKIP_DEP=1     skip make dep when make.dep exists
  CROSS_COMPILE=prefix-   default arm-none-eabi-
  JOBS=N                  parallel job count
EOF
            exit 0
            ;;
        --target)
            [ "$#" -ge 2 ] || {
                echo "Error: --target needs a value." >&2
                exit 2
            }
            CRAZYPOD_TARGET="$2"
            shift
            ;;
        --target=*)
            CRAZYPOD_TARGET="${1#--target=}"
            ;;
        *)
            echo "Error: unsupported argument '$1'." >&2
            exit 2
            ;;
    esac
    shift
done

CROSS_COMPILE="${CROSS_COMPILE:-arm-none-eabi-}"
export CROSS_COMPILE
CRAZYPOD_TARGET="${CRAZYPOD_TARGET:-ipod6g}"
case "$CRAZYPOD_TARGET" in
    ipod6g) label="iPod 6G" ;;
    ipodvideo) label="iPod Video (5G)" ;;
    ipodmini2g) label="iPod Mini 2G" ;;
    *)
        echo "Error: unknown CrazyPod target '$CRAZYPOD_TARGET'." >&2
        exit 2
        ;;
esac
require_tools

builddir="build-bootloader-$CRAZYPOD_TARGET"
stamp="crazypod bootloader $CRAZYPOD_TARGET"
output="bootloader-$CRAZYPOD_TARGET.ipod"

configure_build() {
    ../tools/configure --target="$CRAZYPOD_TARGET" --type=b
    printf '%s\n' "$stamp" > .crazypod_bootloader_configure_stamp
}

if [ "$incremental" -eq 0 ]; then
    echo "CrazyPod: clean $label bootloader build"
    rm -rf "$builddir"
    mkdir "$builddir"
    cd "$builddir"
    configure_build
else
    echo "CrazyPod: incremental $label bootloader build"
    mkdir -p "$builddir"
    cd "$builddir"
    if [ ! -f Makefile ] ||
       [ ! -f .crazypod_bootloader_configure_stamp ] ||
       [ "$(cat .crazypod_bootloader_configure_stamp)" != "$stamp" ]; then
        configure_build
    fi
fi

if [ -n "${CRAZYPOD_SKIP_DEP:-}" ] && [ -f make.dep ]; then
    echo "CrazyPod: reusing bootloader make.dep"
else
    make dep
fi

make -j"$(detect_jobs)"

if [ ! -f "$output" ]; then
    echo "Error: bootloader build did not produce $output." >&2
    exit 1
fi

echo "CrazyPod: built $(pwd)/$output"
