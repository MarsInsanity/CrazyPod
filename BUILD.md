# CrazyPod build guide

CrazyPod's shipping product target is iPod Classic 6G (`ipod6g`).

Two further targets are under bring-up. Both compile, link and package, but
neither has been run on hardware:

- iPod Classic 5G/5.5G "Video" (`ipodvideo`) - see "iPod Video bring-up
  target" below.
- iPod Mini 2nd generation (`ipodmini2g`) - see "iPod Mini 2G bring-up
  target" below. It draws the monochrome build of the UI for its 138x110
  four-shade panel, and ships without the Media app, the Game Boy emulator,
  Mini Apps, video playback or wallpaper.

## Prerequisites

Simulator:

- GNU make
- GCC
- Perl
- Python 3
- OpenSSL 3
- SDL2 development files (`sdl2-config` and `pkg-config`)

Hardware:

- GNU make
- Perl
- Python 3
- OpenSSL 3
- `zip`
- `arm-none-eabi-gcc`
- `arm-none-eabi-objcopy`
- `arm-none-eabi-nm`

Optional video conversion:

- FFmpeg, including `ffmpeg` and `ffprobe`

The current scripts have been verified on macOS. The inherited PowerShell
scripts are not the release path for this LVGL product revision.

## Simulator

```sh
./build-sim.sh
./build-sim.sh --incremental
```

`--target` selects the model, defaulting to `ipod6g`. The 6G builds into
`build-sim/`; every other target builds into `build-sim-<target>/`, so the
simulators coexist:

```sh
./build-sim.sh --target ipodmini2g    # build-sim-ipodmini2g/rockboxui
```

The Mini's simulator needs neither Node.js nor FFmpeg, because its firmware
builds neither the Mini App loader nor the video engine.

Run from the build directory so the simulated disk resolves correctly:

```sh
cd build-sim
./rockboxui
```

| Input | Key |
| --- | --- |
| Wheel counter-clockwise | Up |
| Wheel clockwise | Down |
| Left / Right | Left / Right |
| Select | Return |
| Menu / Back | Backspace or Escape |
| Play | Space |

Simulator-only framebuffer snapshots can open a deterministic route and write
the real 320×240 RGB565 output to `build-sim/simdisk/dump *.bmp`:

```sh
CRAZYPOD_SIM_DUMP=1 CRAZYPOD_SIM_EXIT_AFTER_DUMP=1 \
CRAZYPOD_SIM_SCREEN=clock \
  "build-sim/CrazyPod Simulator.app/Contents/MacOS/CrazyPod Simulator"
```

Supported routes are `home`, `hold-feedback`, `notification-success`,
`notification-error`,
`power`, `coverflow`, `coverflow-power`, `more`,
`more-second`,
`settings-main-menu`, `settings-language`, `settings-reduce-motion`, `notes`,
`note-compose`,
`notes-new`, `notes-draft`, `notes-item`, `notes-search`,
`notes-deleted`, `books`, `books-reading`, `book-reader`,
`book-reader-actions`, `book-reader-next`, `clock`, `stopwatch`, `workouts`,
`workout-ready`,
`workout-active`, `workout-detail`, `calendar`, `calendar-day`,
`contacts`, `contact-detail`, `game2048`, `capability-lab-controls`,
`capability-lab-data`, `capability-lab-game`, and the
`capability-lab-host-*` actions. First-level preview
variants also support `music-0` through `music-7`, `media-0` through
`media-2` (`photos-N` remains an alias), and `books-N`. Video routes use
`videos-N` for a list frame and `play-video-N` for a playback smoke test.
Numeric indices are clamped to the current route. Routes that depend on
content use files from the simulated disk.

Set `CRAZYPOD_SIM_LANGUAGE` to `en`, `zh-Hans`, `zh-Hant`, `ja`, `ko`, `de`,
`fr`, `es`, or `pt-BR` to capture the same route in a specific language.

## Hardware

```sh
./build-hw.sh
./build-hw.sh --incremental
```

`--firmware-only` builds `rockbox.ipod` alone, skipping the codecs, Mini App
payloads, AOT fonts and the packaged zip. On a device that already has a full
install only `rockbox.ipod` needs replacing, so this is the loop to use while
iterating on firmware: it produces about 2 MB rather than several hundred.

In CI it is the `firmware_only` input on the "CrazyPod build" workflow, which
then uploads that file alone. It is an input rather than a workflow of its own
because GitHub only offers a `workflow_dispatch` workflow in the Actions UI
when its file exists on the default branch, so a workflow added on a feature
branch is never visible there.

`--target` selects the model; it defaults to `ipod6g`. The build directory,
the packaged zip and the Mini App CPK payloads all follow the target:

```sh
./build-hw.sh                        # build-hw-ipod6g/CrazyPod-6G.zip
./build-hw.sh --target ipodvideo     # build-hw-ipodvideo/CrazyPod-5G.zip
./build-hw.sh --target ipodmini2g    # build-hw-ipodmini2g/CrazyPod-Mini2G.zip
```

Set Rockbox's build version explicitly for a tagged release. V1.0 was built
with:

```sh
VERSION=V1.0 ./build-hw.sh
cp build-hw-ipod6g/CrazyPod-6G.zip \
  build-hw-ipod6g/CrazyPod-V1.0-iPod6G.zip
cp build-hw-ipod6g/rockbox.ipod \
  build-hw-ipod6g/CrazyPod-V1.0-iPod6G-rockbox.ipod
```

## iPod Video bring-up target

`ipodvideo` builds the same product UI against Rockbox's existing iPod
Classic 5G/5.5G platform layer. Both models are 320x240 RGB565 at 160 DPI
with the same click-wheel keypad, so no UI asset or layout differs.

Targets that ship the product UI are listed in two places, which must stay
in sync:

- `HAVE_CRAZYPOD_UI` in `firmware/export/config/<model>.h`, which selects the
  product code over the Rockbox app layer.
- `CRAZYPOD_MODELS` in `tools/root.make`, which selects the LVGL, Mini App
  and product build rules.

What differs from the 6G, and how:

| Area | 6G | iPod Video |
| --- | --- | --- |
| SoC | S5L8702, ARMv5, 216 MHz, single core | PP5022, ARMv4T, dual core |
| RAM | 64 MiB | 64 MiB, or 32 MiB on 30 GB units (detected in `crt0-pp.S`) |
| Coprocessor | none | `cop_main()` in `crazypod_main.c` releases the COP |
| LCD present | TE/phase-synchronised `lcd_update_rect_*_sync()` | generic `lcd_update_rect()` |
| Accessory | serial iAP plus USB iAP (`HAVE_CRAZYPOD_IAP`) | not ported; the feature is off |
| Install | DFU plus `mks5lboot` | `bootloader-ipodvideo.ipod` via `ipodpatcher` |

Build the 5G bootloader with `build-bootloader.sh`, which takes the same
`--target` as the firmware script:

```sh
./build-bootloader.sh --target ipodvideo
# build-bootloader-ipodvideo/bootloader-ipodvideo.ipod
```

The script only builds the image. How it is installed differs by model and
the difference matters: the 6G's goes on over DFU with `mks5lboot`, while
the 5G's is written into the firmware partition with `ipodpatcher`, as
below.

### Memory budget on 32 MiB units

30 GB 5G/5.5G models carry 32 MiB; 60 and 80 GB models carry 64 MiB.
`crt0-pp.S` probes which, and `core_allocator_init()` in `firmware/core_alloc.c`
lowers `audiobufend` by 32 MiB when it finds the smaller one. The shared
buflib arena that results holds both the audio buffer and every CrazyPod
runtime allocation:

| Build | Static | Shared arena |
| --- | --- | --- |
| iPod 6G | 19.6 MiB | ~40 MiB |
| iPod Video, 64 MiB | 18.5 MiB | ~41.5 MiB |
| iPod Video, 32 MiB | 18.5 MiB | ~9.5 MiB |

The address math leaves no slack on a 32 MiB unit: the codec (1 MiB) and
plugin (3 MiB) buffers alias into 28-32 MiB and land exactly on the top of
RAM, so `PLUGIN_BUFFER_SIZE` cannot grow past 3 MiB without overflowing.
Against a 4 MiB audio floor, a 32 MiB unit has roughly 5.5 MiB of working
room where the 6G has about 36 MiB. This is unmeasured on hardware and is
the most likely thing to fail there.

### Installing on Windows

The 5G writes its bootloader into the firmware partition with `ipodpatcher`,
not through the 6G's DFU/`mks5lboot` path. CI builds `ipodpatcher.exe` and
attaches it to the `crazypod-ipodvideo-firmware` artifact; to cross-build it
locally:

```sh
make -C utils/ipodpatcher \
  CROSS=i686-w64-mingw32- WINDRES=windres ipodpatcher.exe
```

Run it from a Command Prompt started with "Run as administrator". The
executable carries a `requireAdministrator` manifest, and launching it from an
ordinary prompt makes Windows relaunch it elevated in a **new console that
closes as it exits**, so the command appears to return instantly with no
output at all - not even the version banner `main()` prints before it parses
arguments. An already-elevated console keeps the output.

On Windows the device argument is the `PhysicalDrive` **number** alone, not a
path (`main.c` expands it):

```bat
ipodpatcher.exe --scan
ipodpatcher.exe 2 -r ipod-firmware-backup.bin
ipodpatcher.exe 2 -a bootloader-ipodvideo.ipod
```

Take the backup before writing anything; `-w ipod-firmware-backup.bin`
restores it and `-d` removes the bootloader. Then extract `CrazyPod-5G.zip`
to the root of the iPod's data volume, which produces `\.rockbox\`
including `\.rockbox\rockbox.ipod`, the path `BOOTDIR`/`BOOTFILE` in
`ipodvideo.h` point the bootloader at. Eject, then reset with MENU+SELECT.

Recovery, should it not boot: hold MENU+SELECT for about six seconds to
reset; to reach disk mode, reset and immediately hold SELECT+PLAY. Disk mode
works with broken firmware installed, which is what makes the bootloader
reversible.

### Performance log

PortalPlayer builds append one line every ten seconds to
`/.crazypod/perf.log` (`apps/crazypod/crazypod_perf_log.c`). Each line
records the audio state, CPU boost count, whether the library scan is
running, the fullest and emptiest PCM buffer level and the lowest
file-buffer fill seen in the window, how many times the LVGL timer handler
ran and its worst and total time, the same for LVGL renders, the number of
flushed strips and pixels, and the presenter's frame counts and deadline
misses, the render time split by LVGL draw task type, the four most
frequent invalidated screen areas and layer renders with the object class
and caller behind them, the live object count of the active screen,
the wall time of the last audiobook chapter seek, and `step=`, the
input-to-pixels latency of a wheel step split into the three phases it
can wait in: the frame-clock gate, the LVGL render, and the panel write
(`count/average/gate/render/present/worst+dropped`, milliseconds; a
dropped step is one that never reached the panel before the next
input arrived), and `art=`, why tracks do or do not show a cover
(`external/embedded/none/decoded/failed/unsupported`; unsupported
counts embedded pictures in a format no decoder is built for, which
means PNG — only JPEG and BMP are decodable), and `pre=`, the time
spent in the two heaviest things the UI loop does before LVGL runs:
the runtime services tick and the deferred route/preview render. Lines are held in RAM
and written only when the disk is already awake or the buffer fills, since
waking a sleeping drive from the UI thread costs most of a second. The log
stops itself at 512 KiB; delete the file to start over. Play music for a
few minutes, then read the file from the iPod in disk mode to see where the
time goes.

The first log from a 30 GB unit showed why playback stuttered: Rockbox
threads are cooperative, and one LVGL refresh took 200-600 ms at the idle
30 MHz clock, during which the codec thread could not run. PortalPlayer
builds therefore yield between render strips, hold the 80 MHz clock while
the backlight is on, render in 120-row strips, and compile LVGL at `-O2`
(about 80 KiB more code than the `-Os` the rest of the firmware uses).

Not yet done: no 32 MiB memory-budget check under load and no accessory or
inline-remote support. Boot, library scanning and playback have run on a
30 GB unit; playback still stutters and the UI is slow, which is what the
performance log is for.

## iPod Mini 2G bring-up target

`ipodmini2g` builds the product UI against Rockbox's iPod Mini 2nd
generation platform layer. It is the first CrazyPod target without a colour
panel, and almost everything below differs from the 6G because of that one
fact.

The panel is 138x110 at `LCD_DEPTH 2`: four shades of grey, packed four
pixels to a byte (`LCD_PIXELFORMAT HORIZONTAL_PACKING`). The SoC is a
PP5022 as on the Video, so the dual-core, performance-log and 32 MiB notes
in the Video section apply here too; the Mini always has 32 MiB.

| Area | 6G | iPod Mini 2G |
| --- | --- | --- |
| Panel | 320x240 RGB565 | 138x110, 4 greys, 2bpp packed |
| SoC | S5L8702, ARMv5, single core | PP5022, ARMv4T, dual core |
| RAM | 64 MiB | 32 MiB |
| Keypad | `IPOD_4G_PAD` | `IPOD_4G_PAD` (unchanged) |
| Display setting | backlight brightness | LCD contrast and invert |
| Install | DFU plus `mks5lboot` | `bootloader-ipodmini2g.ipod` via `ipodpatcher` |

### How colour becomes ink

The UI is composed in RGB565 exactly as on a colour panel; nothing in the
feature code knows the panel is monochrome. Two maps, both in
`apps/crazypod/crazypod_mono.c`, turn that into four greys, and they are
deliberately different:

- The **design map** inverts. CrazyPod's palette is a dark UI, and a dark
  UI on a reflective grey LCD is unreadable, so `crazypod_ui_color()` sends
  design colours through a map that makes the near-black page white and the
  near-white type black. Everything drawn goes through that one call;
  `crazypod_ui_shade()` is the escape hatch for a value already chosen as a
  grey.
- The **flush quantiser** does not invert. Photographs, album art and the
  boot logo must come back as themselves, so `crazypod_mono_blit_row()`
  quantises luma faithfully at thresholds 42/127/212.

`tests/crazypod_mono_host_test.c` pins the packing convention (four pixels
per byte, leftmost in the high bits, the stored value an inverted
brightness) and checks that the four design greys survive a round trip
through the quantiser unchanged.

### What is not built

Five things the 6G ships are compiled out, not hidden at runtime. The flags
are derived from the panel in `firmware/export/config.h`, so no target
header lists them by hand:

| Flag | Off on the Mini because |
| --- | --- |
| `HAVE_CRAZYPOD_MEDIA_LIBRARY` | the Media app cannot show a photograph on 138x110 in four shades |
| `HAVE_CRAZYPOD_GAMEBOY` | a 160x144 Game Boy frame does not fit, and it would be unplayable in four greys if it did |
| `HAVE_CRAZYPOD_MINIAPPS` | Mini App scenes and Now Playing themes are authored against a 320x240 colour canvas |
| `HAVE_CRAZYPOD_VIDEO` | the decoder stack targets a colour panel, and this SoC would not keep up regardless |
| `HAVE_CRAZYPOD_WALLPAPER` | a photograph behind type takes the type with it when both are drawn in four greys |

The application catalog shrinks with them: `CRAZYPOD_APP_COUNT` is 14 here
against 17 on a 6G. A menu order written by a 6G and carried across on the
same disk still restores -- the applications this build does not have are
dropped and the rest keep their arrangement.
`tests/crazypod_apps_catalog_host_test.c` is compiled twice, once against
each panel's configuration, to check both halves of that.

`CrazyPod-Mini2G.zip` matches: no `Pictures`, `Videos` or `MiniApps`
folders, no `default-home.bmp`, and no CPK payloads.

### Layout and type

`HAVE_CRAZYPOD_COMPACT_UI` selects a second set of layout constants in
`apps/crazypod/ui/presentation/crazypod_ui_metrics.h` -- a 12px status bar
rather than 32, six 14px rows rather than six 28px ones, and so on.

Type is resolved a size down through the ladder in
`crazypod_runtime_font.c`, and the AOT font pack is built to match:
`tools/crazypod-runtime-font-specs.txt` marks each tuple `full`, `compact`
or both, and `build-crazypod-runtime-fonts.sh --canvas compact` bakes only
the small end. Neither package carries the other's sizes, which matters
because a CJK face costs about a megabyte per size.
`tests/test-crazypod-compact-font-ladder.py` checks that every size the
ladder can ask for is in the pack.

### Installing

The Mini takes its bootloader through `ipodpatcher`, exactly as the Video
does; follow "Installing on Windows" above, substituting
`bootloader-ipodmini2g.ipod` and `CrazyPod-Mini2G.zip`. CI attaches both,
plus `ipodpatcher.exe`, to the `crazypod-ipodmini2g-firmware` artifact.

Recovery is the same as on the Video: MENU+SELECT for about six seconds
resets, and reset plus SELECT+PLAY reaches disk mode.

## Cover art must be baseline JPEG

The JPEG decoder is baseline only. A progressive JPEG -- which is what
the Cover Art Archive usually serves, and therefore what Picard writes --
is refused outright, so the artwork never appears and no smaller size
helps. `tools/check-cover-jpegs.py` reports which files in a music tree
the decoder will refuse, and with `--convert` rewrites them through
jpegtran, which changes only the coefficient ordering and so loses
nothing.

```sh
python3 tools/check-cover-jpegs.py /Volumes/IPOD/Music
python3 tools/check-cover-jpegs.py /Volumes/IPOD/Music --convert
```

## Verification

Run the structural and host tests from the repository root:

```sh
sh tests/check-crazypod-ui-architecture.sh
sh tests/run-crazypod-ui-host-tests.sh
sh tests/run-miniapp-host-tests.sh
sh tests/run-gameboy-host-tests.sh
sh tests/run-crazypod-metadata-host-tests.sh
sh tests/run-epub-host-tests.sh
sh tests/run-crazypod-font-tests.sh
python3 tools/check-crazypod-l10n.py --strict-bare
git diff --check
```

The UI host test covers collation, A-Z wheel-jump state, route dispatch,
navigation commands, menu layout, book-reader input, and text helpers. The
architecture gate requires `crazypod_ui.c` to remain between 400 and 1500
lines and rejects feature-private includes outside their owner. The
metadata host test builds the firmware's own MP4 parser against rbcodec's
Unix platform shim and runs it over a synthetic file, so a change to the
atom dispatch cannot silently lose the tag list or the duration.

For a user-visible change, also run the simulator and exercise the affected
route. For LCD, storage, USB, power, audio, or native Mini App changes, an ARM
build proves compilation only; record physical iPod results separately.

## Localization

English source keys and locale resources live under `localization/crazypod`.
After changing them, regenerate both firmware and Mini App lookup tables, then
run the strict audit:

```sh
python3 tools/generate-crazypod-l10n.py
python3 tools/check-crazypod-l10n.py --strict-bare
```

The generator rejects missing keys and mismatched format placeholders. The
audit also rejects untagged user-facing strings in common UI sinks.

Localized fonts are committed build inputs at 8, 10, 12, 14, and 16px. Font
regeneration requires `lv_font_conv` 1.5.3 plus a distributable CJK source
font. Follow [tools/CRAZYPOD_FONTS.md](tools/CRAZYPOD_FONTS.md) and include
`apps/crazypod/crazypod_l10n.c` when collecting characters; it contains native
language names that are not present as translation values.

## Bootloader

The normal hardware archive does not replace the installed bootloader. Build
the CrazyPod bootloader separately when changing the power-on screen:

```sh
./build-bootloader.sh
```

Incremental build:

```sh
./build-bootloader.sh --incremental
```

`--target` selects the model here too, and the output follows it:

```text
build-bootloader-ipod6g/bootloader-ipod6g.ipod
build-bootloader-ipodvideo/bootloader-ipodvideo.ipod
build-bootloader-ipodmini2g/bootloader-ipodmini2g.ipod
```

The script builds the image for any of the three; it does not install one,
and the install path is not the same for all three. See the per-target
sections above.

Installing a bootloader is a separate, device-writing operation. The build
script deliberately does not flash it and `CrazyPod-6G.zip` deliberately does
not include it. The current bootloader artifact is build-verified but has not
completed physical boot regression.

Artifacts:

- `build-hw-ipod6g/rockbox.ipod`
- `build-hw-ipod6g/CrazyPod-6G.zip`
- `dist/miniapps/game2048-5.0.1.cpk`
- `dist/miniapps/capability-lab-5.0.1.cpk`
- `dist/miniapps/native-reference-1.0.0.cpk`
- `dist/miniapps/now-playing-neon-1.4.6.cpk`
- `dist/miniapps/now-playing-signal-1.0.7.cpk`

The zip deliberately contains only the firmware and the runtime resources
required by the independent product:

```text
.rockbox/rockbox.ipod
.rockbox/rockbox-info.txt
.rockbox/codecs/*.codec
.rockbox/codepages/936.cp
.rockbox/fonts/crazypod-aot/*.fnt
.rockbox/crazypod/default-home.bmp
.rockbox/crazypod/icons/<theme>/*.bmp
.rockbox/crazypod/miniapps/packages/game2048-5.0.1.cpk
.rockbox/crazypod/miniapps/packages/capability-lab-5.0.1.cpk
.rockbox/crazypod/miniapps/packages/native-reference-1.0.0.cpk
.rockbox/crazypod/miniapps/packages/now-playing-neon-1.4.6.cpk
.rockbox/crazypod/miniapps/packages/now-playing-signal-1.0.7.cpk
```

`CrazyPod-Mini2G.zip` is the same list without `default-home.bmp` and
without `miniapps/packages/`, because that firmware builds neither the
wallpaper nor the Mini App loader. Its `fonts/crazypod-aot/` holds the
compact end of the pack rather than the full one.

There are no Rockbox WPS files, themes, skin fonts, plugins, or recording
encoder codecs in the product package.

### Build-time checks

The hardware script stops before packaging if any of the main, IRQ, or FIQ
stack boundary symbols is missing or not 8-byte aligned. This is required by
the ARM EABI for values such as `double` and prevents variadic number
formatting from reading the wrong stack data on the iPod 6G.

The Mini App package builder requires Node.js 22 and npm. It AOT-compiles the
supported React Profile TypeScript/TSX subset to C. The Rockbox build compiles
that same C to `app.arm`; the simulator compiles it to `app.dylib`. CPK5 is a
deterministic ZIP STORE package containing the target-native binary, ABI
profile, converted resources and icon. It uses ZIP CRC and strict structure
validation but has no signature.

Check the generated archive and record its hashes before copying it:

```sh
unzip -tq build-hw-ipod6g/CrazyPod-6G.zip
shasum -a 256 \
  build-hw-ipod6g/rockbox.ipod \
  build-hw-ipod6g/CrazyPod-6G.zip \
  dist/miniapps/*.cpk
```

Portable DIY appearances use fixed USB-visible paths:

- Import: copy one file to `/.crazypod/import.upodtheme`, then choose
  Customize → Presets → Import.
- Export: choose Export on a saved appearance; CrazyPod writes it under
  `/.crazypod/export/`.

## Installation warning

Development builds have been installed on an iPod Classic 6G and checked
against their local artifacts with SHA-256 and per-file comparisons. That
proves the copy completed; it does not prove every current behavior is safe or
regression-free.

Before device testing:

1. Confirm the mounted volume belongs to the intended iPod.
2. Back up its existing `.rockbox` and `.crazypod` directories outside the
   repository.
3. Keep a known-good firmware and bootloader recovery procedure.
4. Check the data volume before writing. If directory entries repeat, files
   disappear, or file sizes change between reads, stop and repair the
   filesystem only after backing up every readable file.
5. Copy only the generated `.rockbox` package. Do not use a delete-mirroring
   operation, erase user content, or rewrite the boot partition.
6. Write `rockbox.ipod` last, sync, and compare the installed firmware and
   `.cpk` files against their local hashes.
7. Check the data volume again and safely eject the whole device.

CrazyPod V1.0 has a manual installation and recovery procedure for Windows,
macOS, and Linux in [README.md](README.md#install-crazypod-v10). It does not
provide a one-click CrazyPod installer. See [PROJECT_STATUS.md](PROJECT_STATUS.md)
for the current validation record.

## Environment variables

| Variable | Purpose |
| --- | --- |
| `JOBS=N` | Parallel compile jobs |
| `CRAZYPOD_INCREMENTAL=1` | Reuse the hardware build directory |
| `CRAZYPOD_SKIP_DEP=1` | Reuse an existing hardware `make.dep` |
| `ROCKPOD_INCREMENTAL=1` | Reuse the simulator build directory |
| `ROCKPOD_SKIP_DEP=1` | Reuse an existing simulator `make.dep` |
| `CROSS_COMPILE=prefix-` | Override `arm-none-eabi-` |
| `CRAZYPOD_SIM_EXIT_AFTER_DUMP=1` | Exit after a simulator framebuffer dump |
