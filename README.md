# MotionSense

Motion-triggered video recording for the Luckfox Pico Pro Max (Rockchip
RV1106). Records H.264 to HLS on the SD card, serves a live MJPEG stream and a
playback UI over HTTP, and logs motion events to SQLite.

Two processes:

- **`MotionSense`** — the C daemon. Drives the ISP, encoder and IVS motion
  detection through Rockchip's rockit, writes HLS segments and the metadata
  databases.
- **`motionsense-agent`** — the Go agent. HTTP UI and API on port 5000, reading
  frames from the daemon over a unix socket.

## Features

- **Adaptive frame rate.** Records at 1 fps while idle and 30 fps while motion
  is detected. Thresholds, sensitivity and night mode are configurable.
- **HLS recording.** One `.ts` per keyframe by default, organised into
  date-based directories with a per-day event database and automatic cleanup
  when free space runs low.
- **Live view and playback.** MJPEG stream at `/api/stream`, recordings browsable
  by date, plus a `/api/status` endpoint reporting uptime, memory, load, SoC
  temperature and the last day with footage.
- **OSD timestamp** burned into both the recording and the live stream.

Everything is configurable through `config.yaml` on the SD card; the compiled-in
defaults are used when it is absent.

## Requirements

- Luckfox Pico Pro Max, SPI NAND boot
- An SD card, ext4, **without the `orphan_file` feature** — the 5.10 kernel
  cannot mount it read-write otherwise, and nothing starts. See
  [BUILD.md](BUILD.md#the-sd-card).
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
./rkflash.sh update                     # board in maskrom/loader mode
```

### Development container

The devcontainer is in the **SDK fork**, not in this repository — open
`luckfox-pico-sdk` in VS Code, not `MotionSense`. VS Code will offer *Reopen in
Container*; the image carries the cross toolchain, Go, and the build
dependencies, with `LF_SDK`, `LF_TOOLCHAIN` and `LF_SYSROOT` already set.

Editing this app means editing `project/app/motionsense` inside that workspace.
Building on the host works too — `./build.sh` puts the toolchain on `PATH`
itself — provided the SDK's build dependencies are installed.

[BUILD.md](BUILD.md) covers the build chain, the board configuration, the
on-device layout and what to check when something does not come up.

## Licence

MIT — see [LICENSE](LICENSE). [THIRD-PARTY.md](THIRD-PARTY.md) lists everything
in the tree that is not covered by it.
