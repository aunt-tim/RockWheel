# RockWheel

Firmware for the iPod Video (5G/5.5G, `ipodvideo` target), forked from Rockbox at
commit `f3ffe789ef`. It keeps Rockbox's engine and adds: Finder/iTunes sync (the
iPod presents itself as an iPod and reads Finder's iTunesDB), an iPod-style UI and
theme, Cover Flow, synced lyrics, album-colour Now Playing, dark theme and Night
Mode, plus heavy performance work for a 32 MB, 80 MHz dual-ARM7 device.
C (gnu11), GPL v2+. See `README.md` for features and measured timings.

Most of the tree is upstream Rockbox for ~100 targets. RockWheel only cares about
`ipodvideo` and the SDL simulator; don't touch other targets unless asked.

## Where RockWheel's own work lives

See it all with `git diff --stat f3ffe789ef HEAD`. The main pieces:

- `apps/itunesdb.c/.h` — reads Finder's iTunesDB, feeds the library, writes Play
  Counts back. `apps/tagcache.c`, `apps/tagtree.c`, `apps/tagnavi.config` — database
  held in RAM, journaled library updates, the iPod-style Music menus.
- `apps/root_menu.c`, `apps/menu.c`, `apps/menus/*` (main, settings, eq, display,
  sound, time, extras), `apps/onplay.c`, `apps/settings_list.c` — iPod-style menus.
- `apps/gui/wps.c`, `apps/gui/skin_engine/*` (`cover_palette.c` = album colours),
  `apps/gui/lyrics.c`, `apps/gui/usb_screen.c` — Now Playing, lyrics, USB screen.
- `apps/night_mode.c`, `apps/photos_panel.c`, `apps/plugins/photos.c`,
  `apps/plugins/pictureflow/pictureflow.c` (Cover Flow).
- `apps/playback.c`, `apps/pcmbuf.c`, `lib/rbcodec/codecs/{mpa,aac}.c`, libmad,
  `lib/rbcodec/dsp/*` — decoding on the second core (COP), codec and DSP speedups.
- `firmware/usbstack/usb_storage.c`, `ipod_sysinfo_5g.h` — USB write-behind and the
  device description Finder asks for. `firmware/common/fat.c`, `firmware/font.c`.
- `themes/` — the only theme (`iClassic-Square` and its generated `-Dark`), Inter
  fonts, and `themes/tools/*.py` that generate the bitmaps (see `themes/README.md`).
- `apps/lang/*.lang` — UI strings; new strings go in `english.lang` first
  (Russian is maintained with care; others get translations as needed).
- `install.sh` — the macOS installer (`--dry-run`, `--from DIR`, `--uninstall`).
- `tools/make_photo_database.py` — builds a test Photo Database on the simulator disk.

New source files must be added to the matching `SOURCES` file (e.g. `apps/SOURCES`).

## Build

Needs the Rockbox ARM toolchain (`tools/rockboxdev.sh --target=a`) for the device,
and SDL2 plus Homebrew's `gcc-16` for the simulator (`tools/configure` insists on it
on macOS; `brew install sdl2 gcc`). Here the ARM toolchain is in `~/rockbox-toolchain`
(put its `bin` on PATH). The
simulator takes scripted input: `RBSIM_SCRIPT=<file> ./rockboxui`, one key name per
line (`Return`, `Escape`, `Down`...), `wait <ms>`, `dump` or `quit`. Run it with
`SDL_VIDEODRIVER=dummy`: a background window's screen updates stall for up to a
second on macOS, which turns presses into holds. Dumps (BMP) still work. The
first boot asks for a language, so the first `Return` picks one. For a library
with covers on the simulator disk: `tools/make_test_library.py build-sim/simdisk`.
`make install` and an `rsync` of `themes/` set up the disk. Build directories
go at the repo root as `build*` (gitignored); `build-sim` is the simulator one.

```sh
# Device firmware -> rockbox.zip
mkdir build && cd build && ../tools/configure --target=ipodvideo --type=n
make -j8 && make zip

# Simulator (how the screenshots are made)
mkdir build-sim && cd build-sim && ../tools/configure --target=ipodvideo --type=s
make -j8 && make install && ./rockboxui
```

Theme changes can be copied into the simulator disk without a rebuild:
`rsync -a themes/*/.rockbox/ build-sim/simdisk/.rockbox/`.

## Testing

There is no unit test suite. Changes are verified by building cleanly for both the
device and the simulator, then exercising them in the simulator (`rockboxui`), and
on the real iPod for anything timing-, disk- or USB-related. Performance claims in
commits and the README are measured on the test iPod (5.5G, 32 MB RAM, iFlash).
`lib/rbcodec/test/warble` is Rockbox's codec test harness.

## Conventions

- Rockbox code style: 4-space indent, no tabs, braces on their own line for
  functions, C comments, the GPL header on new files.
- Commit subjects are plain sentences describing the user-visible behaviour, with
  no prefix (e.g. "The volume follows the wheel, also as a track starts"). Bodies
  explain why, with measured numbers where performance is involved. Only
  `README:` commits use a prefix.
- Memory is tight (32 MB model is the target): avoid new large static buffers,
  prefer aligned buffers for disk I/O (DMA needs them), and keep work off the UI
  thread.
- `.gitreview` and other Gerrit/upstream files are Rockbox leftovers; RockWheel
  bugs are tracked on GitHub, not upstream.
