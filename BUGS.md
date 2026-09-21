# CrazyPod open bugs

The list of record. Every known defect gets an entry here, and a bug that
is not here is a bug one of us forgets between test rounds.

- Status date: 2026-09-21
- Branch under test: `claude/rockbox-ipod-classic-5th-port-nhvtu1`
- Newest build on that branch: `254edd14`
- Hardware under test: iPod Classic 5.5G (`ipodvideo`, PP5022, 32 MiB),
  microSD through an iFlash-style ATA adapter
- Also under test: `claude/happy-albattani-b9woty` on an iPod Mini 2G
  (`ipodmini2g`, PP5022, 32 MiB, 138x110 at two bits per pixel)

## How to use this file

Each entry carries a stable identifier. Identifiers are never reused, and a
closed bug keeps its number so an old test report still means something.

An entry is only as good as its last line: **Closes when** says what would
let us mark it done. If that line cannot be written, the bug is not
understood well enough to fix yet, and the entry says so instead of
guessing.

Statuses:

| Status | Meaning |
| --- | --- |
| Open | Reproduced, cause not established, nothing shipped |
| Instrumented | Cause not established; a build now logs what would settle it |
| Shipped, unconfirmed | A fix is on the branch, untested on device |
| Deferred | Understood, not being fixed yet, and why |
| Watch | Reported once, not reproduced; hardening shipped |
| Closed | Fixed and confirmed on hardware |

Severity is about what the owner loses, not how hard the fix is. Critical
means losing data or a listening position; high means a feature is unusable;
medium means it works but badly; low is cosmetic.

## Open

### CP-001 — Play after a reboot resumes the same audiobook

**Critical. Instrumented.**

After a reboot, the first Play starts the same audiobook at the same
position, however long a different track was played before the reboot. The
position is identical every time, which points at a saved state that stopped
being updated rather than one that is updated wrongly.

An idle auto power-off does *not* do this: it keeps whatever was playing.
`SYS_POWEROFF` and `SYS_REBOOT` run the same code
(`apps/crazypod/ui/shell/crazypod_system_prompts.c`, `execute()`), so the
reboot that loses state is not going through it — most likely a Menu+Select
hard reset, which runs no shutdown code at all. On that path the only thing
on the card is the last periodic save, and the periodic save is the suspect.

Two candidate causes, wanting opposite fixes:

1. the newest queue never reaches the card during ordinary use, and the
   forced save at shutdown is the only one that ever works;
2. it reaches the card and is not read back at boot.

Diagnostics on the branch, in `/.crazypod/diag.log`:

```
queuesave count=N [first path]                         queue file rewritten
statesave force=F queue=N index=I elapsed=E rewrote=R  header written
statefail <stage>                                      a write did not land
shutdown type=T                                        clean path ran at all
resume queue=N/M index=I elapsed=E match=B [path]      what boot restored
coldplay index=I of N elapsed=E [path]                 what Play started
```

Code: `apps/crazypod/crazypod_state.c` (`crazypod_state_save`,
`crazypod_state_tick`, `crazypod_state_load`, `save_queue`),
`apps/crazypod/ui/app/crazypod_playback.c` (`crazypod_playback_toggle`).

Commits: `16add53f`, `0d19e548` (diagnostics only; no fix).

**Closes when** a reboot after playing a song resumes that song, three times
running, including at least once from a hard reset.

### CP-002 — The Now Playing Actions menu takes 2-3 seconds to open

**High. Instrumented.**

Pressing the centre button in Now Playing takes two to three seconds to show
the Actions menu. The Actions menu on the Home screen and the wired
headphones popup both open promptly, which exonerates the popup machinery
they share.

Ruled out: overlay glass sampling and its `lv_refr_now(NULL)`; the panel and
underlay descriptors; click-wheel hold thresholds; popup animation (there is
none — `crazypod_popup_animate` sets position and opacity outright); the
favourite lookup, which is in memory. It also happens with Reduce Effects on,
which rules out the effects path by itself.

Remaining hypothesis: opening the popup forces a redraw of the Now Playing
screen underneath it, and that screen is the expensive one.

The first round of timing measured the wrong window: it stopped when the
widgets existed, which is not when the popup appears. LVGL draws them on the
next frame, and that draw was outside the measurement. The diagnostic now
covers the whole press and reports, in milliseconds:

```
nowactions refr=A under=B meas=C panel=D build=E wait=F draw=G total=H
```

`refr` is the forced screen redraw, `under` the framebuffer copy behind the
popup, `meas` the text measuring that sizes it, `panel` the glass sample,
`build` the widgets, `wait` the gap before the next frame, and `draw` that
frame. One press names the step.

Worth knowing while reading it: the Home Actions menu forces the same screen
redraw and is fast, so that call is not the difference by itself. Home is
drawn into the framebuffer natively rather than by LVGL, so there is little
for a redraw to do there and a great deal on Now Playing.

Code: `apps/crazypod/ui/features/now_playing/crazypod_now_playing_overlay.c`.

Commits: `750b389b` (removed real waste under Reduce Effects; did not fix
this), `de8bac82` (stopped two failing lyrics file opens per menu open),
`f8113812` (measure the whole press, including the draw).

**Closes when** the menu opens in under half a second with Reduce Effects
off.

### CP-003 — Album Flow drops frames

**Medium. Deferred.**

Album Flow scrolling stutters. It decodes a cover per tile, with no reuse
between frames, so the decode cost lands on the scroll. Reported FAIL in the
last test round and untouched since.

Likely shape of the fix: a small ring of decoded tiles plus prefetch in the
direction of travel. Not started; it is real work, not a constant change.

Code: `apps/crazypod/crazypod_coverflow.c`,
`apps/crazypod/ui/app/crazypod_playback.c`
(`crazypod_playback_warm_album_flow`, `crazypod_playback_sync_album_flow`).

**Closes when** a flick through twenty albums holds its frame rate in
`perf.log`.

### CP-004 — The Podcasts route reads the whole track list for every row

**Medium. Deferred, awaiting a design decision.**

The same defect that made the Albums list lag, and worse: the Albums fix
walked sorted neighbours, but the podcast lookup scans every track in the
catalog, per row, per frame. It is implemented twice, so both copies have to
go.

Two ways out, and the choice is the owner's:

- a podcast group built into the catalog — clean, costs RAM and scan time;
- a memo cache with a memory budget — cheaper, more fragile.

Deliberately not fixed: picking either one without deciding first would be
guessing with somebody else's RAM on a 32 MiB device.

Code: `apps/crazypod/ui/features/music/crazypod_music_feature.c` (static
`podcast_track_index`),
`apps/crazypod/ui/features/music/crazypod_music_activation.c`
(`crazypod_music_podcast_track_index`).

**Closes when** the Podcasts list scrolls like the Artists list and neither
copy of the scan remains.

### CP-005 — A missing queue file can desynchronise the saved snapshot

**Medium. Open, found by reading, not reproduced.**

At boot, `crazypod_state_load` skips queue lines whose files no longer exist,
then records the hash and count of the *filtered* set as the snapshot of what
is on the card. The file on the card still holds the full set. A later save
that finds the queue generation unchanged reuses that snapshot instead of
rewriting the file, so the header can end up describing something the queue
file does not contain.

Not confirmed to happen, and not yet shown to be the cause of CP-001 — listed
because it was found while reading that path and would otherwise be lost.

Code: `apps/crazypod/crazypod_state.c`, end of `crazypod_state_load`.

**Closes when** either a host test reproduces the desynchronisation and the
fix makes it pass, or the path is shown to be safe and this entry is closed
with the reasoning written down.

## Shipped, not yet confirmed on hardware

### CP-006 — Audiobook listening position saves inconsistently

**Critical. Shipped, unconfirmed.**

The position sometimes survives a reboot and sometimes does not. The saved
position was gated on `storage_disk_is_active()`, so it only really happened
when something else had already woken the disk — in practice, on pause — and
nothing wrote it on a clean shutdown at all.

Fix: a forced interval that does not depend on the disk, plus a flush on the
way out. May share a root cause with CP-001.

Code: `apps/crazypod/crazypod_audiobooks.c` (`save_due`,
`crazypod_audiobooks_flush`),
`apps/crazypod/ui/shell/crazypod_system_prompts.c`.

Commit: `27c69cbb`.

**Closes when** a position survives three reboots in a row, including a hard
reset.

### CP-007 — No Home widget cover for an audiobook played after a reboot

**Medium. Shipped, unconfirmed.**

Title and author appear, the cover does not. The artwork request identity did
not include the embedded-picture fields, so a "this track has no cover"
answer given before the tag probe ran stuck to the slot for good. The Lock
Screen escaped it only because it reuses Now Playing's already decoded slot.

Code: `apps/crazypod/crazypod_artwork.c`.

Commit: `4a37d26e`.

**Closes when** the cover appears on the Home widget after a reboot, from a
cold start with no other screen opened first.

### CP-014 — Overlays and cards on the iPod Mini were drawn at 320x240

**High. Shipped, unconfirmed.**

Reported from the device with photographs: the Now Playing Actions card
showed a single row with a black blob where its icon should be and the
label wrapped to "Vie w", and nothing below it.

The cause is the same everywhere it appeared. These screens carry the
offsets a 320x240 panel uses, and the Mini is 138x110, so anything past
those bounds is simply not on the glass. The Actions card puts its detail
line 137 pixels down a card that cannot be taller than the screen; the
Play Mode card declares four 31px rows at 15, so each label was clipped to
a sliver and the fourth row fell off the bottom; the choice overlay printed
its "<value> n/m" caption five pixels into the first row; the headphone
card drew a 184x158 panel, so the Mini showed two giant earbuds and no
words; Search was a two-column layout whose right-hand column began at
x=170; EQ Studio put its graph, footer and hints past both edges; the note
sheet, the book reader, its Stats and Info cards, the book loading screen,
the workout and calendar detail cards and the contact card were all the
same error.

Three of them were palette rather than geometry: the home Actions cells
drew a disc behind each icon at a twelfth opacity, which has no shade on a
four-shade panel and lands on a solid blob; the organizer cards are
near-black at nine tenths, which the design map turns into the page they
sit on; the calendar day sheet is white with near-black type, which is
exactly the pair the map inverts, so it came out white on black.

Code: `apps/crazypod/ui/features/now_playing/crazypod_now_playing_overlay.c`,
`apps/crazypod/ui/presentation/crazypod_choice_overlay.c`,
`crazypod_popup_layout.c`, `crazypod_search_screen.c`,
`crazypod_empty_state.c`, `apps/crazypod/ui/shell/crazypod_home_actions.c`,
`crazypod_headphone_popup.c`,
`apps/crazypod/ui/features/settings/crazypod_eq_studio_screen.c`,
`apps/crazypod/ui/features/notes/crazypod_notes_screen.c`,
`apps/crazypod/ui/features/books/`, `apps/crazypod/ui/features/organizer/`.

Commits: `14dd2be0`, `21ca35bf`, `c4c815f0`, `cb297334`.

**Closes when** each of these opens on a Mini with everything inside the
panel and readable: the Now Playing Actions and Play Mode cards, a choice
list, the home Actions menu, the headphone card, Search, EQ Studio, a note,
a book and its Stats and Info screens, a workout, a calendar day and event,
and a contact.

### CP-015 — Now Playing drew its album row and badges over the waveform

**Medium. Shipped, unconfirmed.**

On the Mini the metadata column ran into the waveform below it. The rows
are 14 pixels apart for a face whose line box is fifteen, and the album row
and the favourite and play-mode badges want another twenty-nine pixels
before a waveform that starts at sixty-eight.

The simulator never showed it: its "No Track" path builds neither the album
row nor the badges, so the collision only existed with a real track loaded.
The compact screen now shows title and artist only.

Code: `apps/crazypod/ui/features/now_playing/crazypod_now_screen.c`.

Commit: `14dd2be0`.

**Closes when** a playing track on a Mini shows its title and artist clear
of the waveform.

### CP-016 — Album Flow on the Mini opened a route with nothing to draw

**Medium. Shipped, unconfirmed.**

The monochrome build does not compile the cover flow, because it writes
past the end of a packed 2bpp framebuffer (the data abort behind the first
CrazyPod Panic). The Music menu still offered the row, so opening it gave a
route whose only contents were three labels placed off the panel.

Code: `firmware/export/config.h`,
`apps/crazypod/ui/features/music/crazypod_music_feature.c`,
`crazypod_music_activation.c`.

Commit: `fd3bcc6a`.

**Closes when** the Music menu on a Mini lists Now Playing, All Music,
Playlists, Artists, Albums, Songs and Search, and Search still opens.

## Watch

### CP-008 — An epub was reported corrupted after running the cover tool

**High. Watch.**

Reported once. Not reproduced: a stress run over realistic epub structures
passed. Rather than declare it fine, the tool now verifies a rewrite before
letting it replace the original, fsyncs before the rename, and keeps backups
by default.

Leading suspect, stated for the record: no fsync before rename on removable
media, which loses the write if the card is pulled or the machine sleeps.

Code: `tools/shrink-ipod-covers.py`,
`tests/test-crazypod-shrink-ipod-covers.py`.

Commit: `5e681a3b`.

**Closes when** a full library run completes with every book still opening,
or a reproduction is found and fixed.

### CP-009 — The cover tool has never been run against real m4b files

**High. Watch. Not a bug yet — an untested claim.**

Chapter preservation in the m4b rewrite is tested only against files the test
suite builds itself. The in-place replacement is designed to keep `moov`
byte-identical, because `stco` offsets are absolute and any size change would
break every chapter in the book. That design has never met a real audiobook.

Code: `tools/shrink-ipod-covers.py`.

**Closes when** one copied audiobook survives the tool with its chapters
intact on the device.

## Recently closed

- **CP-010** — The Albums list scrolled badly: a duplicate-title check
  read the whole catalog per row, per frame. Fixed in `54a7ca3d` by
  walking sorted neighbours instead. Confirmed on device.
- **CP-011** — The cover tool shrank covers to a size above its own
  threshold, so every run shrank the same files again. Fixed in
  `494e5f4a`.
- **CP-012** — Converted PNG covers kept the PNG type flag in the `covr`
  atom, so the firmware would not have drawn them. Fixed in `b4b7807e`.
- **CP-013** — The release package created every content folder except
  `/Audiobooks`. Fixed in `254edd14`.
