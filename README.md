# MotionSense

Motion-triggered video recording for the Luckfox Pico Pro Max (Rockchip
RV1106). Records H.264 to HLS on the SD card, serves a live MJPEG stream and a
playback UI over HTTP, and logs motion events to SQLite.

## Features

- **Adaptive frame rate.** Records at 1 fps while idle and 30 fps while motion
  is detected. Thresholds, sensitivity and night mode are configurable.
- **HLS recording.** One `.ts` per keyframe by default, organised into
  date-based directories with a per-day event database and automatic cleanup
  when free space runs low.
- **Live view and playback.** MJPEG stream at `/api/stream`, recordings
  browsable by date, plus a `/api/status` endpoint reporting uptime, memory,
  load, SoC temperature and the last day with footage.
- **OSD timestamp** burned into both the recording and the live stream.

## How it fits together

Two processes, split by what each language is good at:

- **`MotionSense`**, in C, owns the pipeline — ISP, encoder and IVS motion
  detection through Rockchip's rockit, writing HLS and the metadata databases
  to the card. Nothing sits between it and the hardware.
- **`motionsense-agent`**, in Go, owns the network — HTTP, the UI, the API. It
  never touches rockit, so **no cgo**: a static binary built with
  `CGO_ENABLED=0`, reading the databases through `modernc.org/sqlite` in pure
  Go. It serves on port 80 as root, 3000 otherwise.

They talk over a unix socket (`/tmp/motionsense-stream.sock`), which keeps the
split honest: the daemon can die without taking the web interface with it, and
the agent restarts without disturbing a recording.

Configuration is one `config.yaml` on the SD card, seeded from the image on
first boot and never overwritten after that. Compiled-in defaults apply when
it is missing, so a blank card still records.

`S99motionsense` starts both, waits for the card to mount, and hands the
camera over from the stock `rkipc` app. It ships in the image through the
board overlay, so a flashed board records from first boot with nothing to
install and nothing to configure.

## Requirements

- Luckfox Pico Pro Max, SPI NAND boot
- An SD card, **ext4**. It must not carry the `orphan_file` feature — the 5.10
  kernel cannot mount that read-write, and nothing starts. See
  [docs/BUILD.md](docs/BUILD.md#the-sd-card).

  FAT32 and exFAT do mount and do work — SQLite included, since both support
  the shared mappings and byte-range locks that WAL wants. They are not
  recommended because neither journals. This device is powered by whatever it
  is plugged into and gets pulled without a shutdown; ext4 replays its journal
  after that, FAT does not, and the directory holding a day of footage is the
  thing at risk. FAT32 additionally caps a file at 4 GB, and both force every
  file to uid 1000 regardless of who wrote it.

  The rule lists ntfs, ext2 and ext3 as well, but this kernel builds none of
  them, so those branches never fire.
- Enough card for the footage you want to keep. Below
  `storage.disk_free_min_mb`, 2 GB by default, the oldest day is deleted to
  make room; recording itself never stops.
- x86-64 Linux host

## Building

This is an application inside the Luckfox SDK, not a standalone project. It
lives at `project/app/motionsense` in a fork of that SDK, which carries the
board configuration, the device tree and the development environment:

**<https://github.com/zhu30844/luckfox-pico-sdk>**

```bash
git clone --recurse-submodules git@github.com:zhu30844/luckfox-pico-sdk.git
cd luckfox-pico-sdk
printf '9\n15\n' | ./build.sh lunch     # hardware "custom" -> MotionSense
./build.sh
adb reboot loader                       # put the board in maskrom/loader mode
./rkflash.sh update
```

### Development container

The devcontainer is in the **SDK fork**, not in this repository — open
`luckfox-pico-sdk` in VS Code, not `MotionSense`. VS Code will offer *Reopen in
Container*; the image carries the cross toolchain, Go, and the build
dependencies, with `LF_SDK`, `LF_TOOLCHAIN` and `LF_SYSROOT` already set.

Editing this app means editing `project/app/motionsense` inside that workspace.
Building on the host works too — `./build.sh` puts the toolchain on `PATH`
itself — provided the SDK's build dependencies are installed.

[docs/BUILD.md](docs/BUILD.md) covers the build chain, the board configuration, the
on-device layout and what to check when something does not come up.
[docs/DEVNOTES.md](docs/DEVNOTES.md) covers working on the board: how to connect to it,
what to do when a link drops, and the bugs that present as something else.

## Scope

A learning project. Not built, tested or reviewed as a security product, and
not suitable for one.

Concretely, on the image this repository produces:

- The HTTP interface has no authentication. Anyone who can reach port 80 can
  watch the live stream and every recording.
- The root password is the vendor's, shipped in the image and identical on
  every board built from it.
- Recordings and the metadata databases are unencrypted on the SD card.
- Nothing is hardened against a hostile network, and nothing here has had a
  security review.

Keep it on a network you control. Do not expose it to the internet, and do not
rely on it for anything that matters — safety, evidence, or an audit trail.

The author accepts no responsibility for what it does or fails to do.

## Licence

MIT — see [LICENSE](LICENSE). [docs/THIRD-PARTY.md](docs/THIRD-PARTY.md) lists everything
in the tree that is not covered by it.
