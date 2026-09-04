# Building and installing MotionSense

MotionSense targets the Luckfox Pico Pro Max (Rockchip RV1106, SPI NAND).
It is built as an application inside the Luckfox SDK, and lives at
`project/app/motionsense` in a fork of that SDK:

- SDK fork: <https://github.com/zhu30844/luckfox-pico-sdk>
- This repo is a submodule of it.

Two things get built: `MotionSense`, the C daemon, and `motionsense-agent`,
the Go HTTP UI/API under `agent/`. Both land in `/oem/usr/bin` and are started
by the same init script.

---

## Quick start

```bash
git clone --recurse-submodules git@github.com:zhu30844/luckfox-pico-sdk.git
cd luckfox-pico-sdk
printf '9\n15\n' | ./build.sh lunch     # hardware "custom" -> MotionSense
./build.sh                              # ~20 min cold, minutes warm
./rkflash.sh update                     # board in maskrom/loader mode
```

The image is `output/image/update.img`, with a timestamped copy under
`IMAGE/IPC_SPI_NAND_BUILDROOT_RV1106_MOTIONSENSE_<date>_RELEASE_TEST/`.

Submodules nest two deep — this app under the SDK, and media-server under this
app — so `--recurse-submodules` is not optional. On an existing checkout:
`git submodule update --init --recursive`.

A devcontainer is provided (`.devcontainer/` in the SDK fork) with the
toolchain, Go, and the `LF_*` environment variables already set. Building on
the host works too; `./build.sh` puts the cross toolchain on `PATH` itself.

---

## The build chain

`./build.sh` runs the SDK's normal pipeline. MotionSense enters it at the
app stage:

```
./build.sh
  └─ sysdrv    u-boot, kernel, buildroot rootfs        -> output/out/rootfs_uclibc_rv1106
  └─ media     Rockchip media libs (rockit/rkaiq/mpp/rga) -> output/out/media_out
  └─ app       project/app/Makefile
       └─ $(wildcard ./*/Makefile) finds motionsense/Makefile
            ├─ c-daemon: cmake configure + build + install -> install-sdk/
            │            MAROC_COPY_PKG_TO_APP_OUTPUT      -> project/app/out/{bin,share}
            └─ go-agent: go build                          -> project/app/out/bin/
  └─ project/app/out  ->  output/out/app_out
  └─ __PACKAGE_OEM    ->  output/out/oem/usr/{bin,share}   (build.sh:1379)
  └─ mkfs.ubifs       ->  output/image/oem.img
  └─ update.img
```

Nothing in the SDK had to be edited to add the app: `project/app/Makefile`
discovers any subdirectory containing a `Makefile`.

`Makefile` here is a thin wrapper. It includes `../Makefile.param` for the
cross toolchain and media paths, then drives the same CMake build the
standalone flow uses. Two details that bite if you copy it elsewhere:

- `CURRENT_DIR` must be evaluated *before* `include ../Makefile.param`, which
  pulls in `.BoardConfig.mk` and would otherwise be what
  `$(lastword $(MAKEFILE_LIST))` points at.
- `SHELL := /bin/bash` is required. `Makefile.param`'s copy and strip macros
  use `[[ ]]`, and GNU make does not pass `SHELL` down to sub-makes.

### Building only this app

```bash
cd project/app/motionsense
make            # both binaries, staged into project/app/out
make c-daemon   # C only
make go-agent   # Go only
make clean
make info       # prints the resolved toolchain / media / output paths
```

`go-agent` depends on `c-daemon` purely for ordering: `Makefile.param` sets
`MAKEFLAGS += -j`, and without the dependency the two race and
`MAROC_STRIP_DEBUG_SYMBOL` can catch a half-written agent binary.

Then `./build.sh firmware` from the SDK root to repack images.

### Standalone (no image, just the binary)

```bash
cd project/app/motionsense
cmake -S . -B build && cmake --build build -j
```

The toolchain is resolved in this order: an explicit `-DCMAKE_C_COMPILER`
(what the SDK build passes), then `$LF_TOOLCHAIN`, then
`../../../tools/linux/toolchain/...` relative to this tree's place inside the
SDK. It is no longer vendored here.

### The Go agent

```
GOOS=linux GOARCH=arm GOARM=7 CGO_ENABLED=0 go build -trimpath -ldflags="-s -w"
```

No CGO is needed: `modernc.org/sqlite` is a pure-Go implementation, so the
binary is static and does not link against uclibc. `-s -w -trimpath` takes it
from 15.7MB to 11MB, which is also why it must not go through the SDK's
`MAROC_STRIP_DEBUG_SYMBOL` step — that runs GNU `strip`, which is not
something to point at a Go binary. The Makefile builds it after the strip for
that reason.

`agent/` has no `vendor/` directory, so the first build downloads modules and
needs network access. The SDK build already needs network for buildroot, so
this adds no new requirement, but it does mean a cold build cannot run
offline.

The agent listens on `:5000`, serves the web assets embedded via `go:embed`,
and reads frames from the C daemon over `/tmp/motionsense.sock`. It tolerates
starting before the C daemon: a missing database is logged and skipped, and
the socket reader retries.

### Pushing to a running board over adb

`deploy.sh` builds and pushes the binary without reflashing. It deliberately
pushes no libraries, on the assumption that everything is already on the
device — which is now only true for a device flashed with an image built
*after* the switch to buildroot-provided libraries. An older image has no
`libsqlite3.so.0` or `libyaml-0.so.2` and the daemon will fail to start.
Flash `update.img` once after that change, then `deploy.sh` works again.

---

## Where dependencies come from

| Dependency | Source | Ships in |
|---|---|---|
| `librockit`, `librkaiq`, `librockchip_mpp`, `librga` | `output/out/media_out` (SDK build) | `/oem/usr/lib` |
| `libsqlite3`, `libyaml`, `libfreetype`, `libiconv` | buildroot sysroot | `/usr/lib` (rootfs) |
| `libhls`/`libmpeg` (`3rdparty/media-server`, submodule), `libosd` (`3rdparty/osd`) | built from source, static | linked in |

The Rockchip libraries have two possible sources, selected by
`MS_RK_MEDIA_DIR`: unset uses the snapshot under `vendor/`, set (which the SDK
`Makefile` does) uses `output/out/media_out`, so the app tracks what the SDK
just built rather than a copy that drifts.

The four distro libraries come from `cmake/SystemLibs.cmake`, which imports
them from the buildroot sysroot under the same CMake target names the
vendored copies used (`sqlite3::sqlite3`, `yaml::yaml`, `freetype::freetype`,
`iconv::iconv`).

### media-server

`3rdparty/media-server` is a submodule of ireader/media-server pinned at
`ea53ac6`, kept pristine. Only `libmpeg` and `libhls` are compiled; the rest of
that project (flv, mov, mkv, dash, rtsp, ...) is unused. The build glue is ours
and lives in `cmake/MediaServer.cmake`, because a submodule cannot carry local
files.

One file is not taken as-is. `hls_media_input()` asserts DTS continuity in a
way that does not allow `duration == 0` — "cut a segment on every keyframe",
which is what `storage.c` uses and what `config.yaml` ships as
`hls_duration_s: 0`. The patched copy lives in `3rdparty/media-server-local`
and `cmake/MediaServer.cmake` compiles it in place of the submodule's, so the
submodule never shows up dirty. See the README there for how to refresh it
after a version bump. Release builds define `NDEBUG` and compile the assert
out, so this only bites a `-DCMAKE_BUILD_TYPE=Debug` build, which aborts as
soon as recording starts.

Pinning took some care and is worth repeating if the version is ever bumped:
match on the `libmpeg/source`, `libmpeg/include` and `libhls/include` **tree
hashes**, not on a single file. `hls-media.c` alone points at `8928fa5`, which
is simply the newest upstream revision of a rarely-touched file; 20 of the
other 48 files disagree with it, and building there produced a binary 8KB
smaller. The tree hashes land on `ea53ac6`, where 48 of 49 files are
byte-identical to what was vendored before and the binary reproduces exactly.

Which buildroot packages this app needs is declared in
`buildroot-packages.fragment`, here in the app rather than in an SDK file
far from the code that needs it. The SDK passes `RK_BUILDROOT_DEFCONFIG`
straight to buildroot as a make target, so buildroot needs one complete
defconfig and has no fragment mechanism wired up; the fragment is applied
with:

```bash
./scripts/gen-buildroot-defconfig.sh
```

which regenerates `sysdrv/tools/board/buildroot/motionsense_defconfig` from
`luckfox_pico_defconfig` plus the declared packages, and refreshes the copy
inside the extracted buildroot tree. Run it after editing the fragment, then
rebuild. It is a separate step rather than a build hook because the defconfig
is consumed in the SDK's sysdrv stage, before any hook this app can register.

No shared library is packaged by this app. Everything it needs is already in
the image, so `install()` of the Rockchip `.so` files is skipped whenever
`MS_RK_MEDIA_DIR` is set.

---

## On-device layout

```
/oem/usr/bin/MotionSense                     the C daemon
/oem/usr/bin/motionsense-agent               the Go HTTP UI/API, port 5000
/oem/usr/share/MotionSense/fonts/            OSD font
/oem/usr/share/MotionSense/config.yaml       seed copy
/oem/usr/lib/                                Rockchip media libraries
/etc/init.d/S99motionsense                   start script (from the board overlay)
/mnt/sdcard/MotionSense/                     log, live config
/mnt/sdcard/MotionSense/agent.log            agent log
/mnt/sdcard/DCIM/                            recordings
```

The binary is linked with `RPATH=$ORIGIN/../lib`, which resolves to
`/oem/usr/lib` from `/oem/usr/bin`. That covers the daemon's own `NEEDED`
entries but **not** the transitive ones: `librockit.so` itself needs
`librockchip_mpp.so.1` and `librga.so`, and resolving a library's own
dependencies does not consult the executable's RPATH. `S99motionsense`
therefore still exports `LD_LIBRARY_PATH=/oem/usr/lib`; without it the daemon
dies at exec with `can't load library 'librockchip_mpp.so.1'` even though the
file is right there.

`S99motionsense` waits for `/mnt/sdcard` to be mounted (up to 30s) before
starting, and hands the camera over from the stock `rkipc` app first. The SD
card is auto-mounted by a udev rule shipped in the rootfs
(`sysdrv/tools/board/eudev/rules/61-sd-cards-auto-mount.rules`), covering
vfat/exfat/ntfs/ext2/3/4.

The two processes talk over `CFG_SOCKET_PATH`
(`/tmp/motionsense-stream.sock`), which the C daemon creates and the agent
dials for MJPEG frames. It is defined once, in `config.h`; both sides read
that name. Recording does not go through it, so a broken socket costs the
live view and nothing else.

A healthy boot looks like this:

```
$ /etc/init.d/S99motionsense status
MotionSense: running
agent: running
$ grep sdcard /proc/mounts
/dev/mmcblk1p1 /mnt/sdcard ext4 rw,noatime 0 0
$ tail -1 /mnt/sdcard/MotionSense/agent.log
[stream] connected to /tmp/motionsense-stream.sock
$ ls /mnt/sdcard/DCIM/$(date +%F)/00001/
00000.ts  00001.ts  index.m3u8
```

### The SD card

Two things about the card are easy to lose a day to.

**The filesystem must not use `orphan_file`.** e2fsprogs 1.47 and later enable
it by default, it lands in ext4's ro_compat set as bit 16, and this 5.10
kernel does not know it, so the card mounts read-only and the auto-mount rule
— which asks for read-write — fails. `/mnt/sdcard` then never appears and
`S99motionsense` times out with the daemons never starting. On a PC:

```bash
sudo e2fsck -f /dev/sdX1
sudo tune2fs -O ^orphan_file /dev/sdX1
```

**The card may need a retry to enumerate.** On this board it does not always
answer at the first probe frequency:

```
mmc_host mmc1: Bus speed (slot 0) = 400000Hz
mmc1: error -110 whilst initialising SD card
mmc_host mmc1: Bus speed (slot 0) = 300000Hz
mmc1: new high speed SDXC card at address 59b4
```

This is why `rv1106g-motionsense.dts` leaves out the vendor's `non-removable`.
With it set, the core scans once at probe, ignores the card-detect pin, and
never retries — a boot-time failure became permanent and reseating the card
produced no event at all.

Bus speed is capped at SD High Speed, 50MHz: `no-1-8-v` rules out the UHS
modes, which need 1.8V signalling. Measured on a 119GiB SDXC card: 22.1 MB/s
sequential read, 19.0 MB/s sequential write, 0.70ms median 4K random read,
9.4ms median 4K synced write (p95 15ms, worst 32ms). Recording at a few Mbps
uses a small fraction of that, but the synced-write latency is worth knowing
if the metadata database is committed with `PRAGMA synchronous=FULL`.

### Reflashing just the app

The app is on its own partition, so iterating does not need a full reflash:

```bash
./rkflash.sh oem
```

`rkflash.sh` also accepts `boot`, `rootfs`, `userdata`, `uboot`, `loader`,
`update`, `erase`.

Pushing the binary over adb and restarting the service is faster still, but
**judge recording from a full reboot, not from a service restart.** Stopping
and starting S99motionsense leaves the ISP and VI in a state where capture
does not resume: the daemon runs, the day and segment directories appear, the
databases get written, and no `.ts` is ever produced. It looks exactly like a
regression in whatever you just changed. A reboot recovers it. This was
confirmed with a binary byte-identical to one that had just been recording
happily, so the restart alone accounts for it.

---

## Board configuration

Everything product-specific lives in its own directory in the SDK fork, so
syncing upstream Luckfox changes does not conflict:

```
project/cfg/BoardConfig_MotionSense/
├── BoardConfig-SPI_NAND-Buildroot-RV1106_MotionSense-IPC.mk
├── motionsense-oem-pre.sh                    oem trimming for this board only
├── luckfox-userdata-pre.sh          -> symlink into BoardConfig_IPC/
└── overlay/
    ├── overlay-luckfox-config       -> symlink
    ├── overlay-luckfox-buildroot-init   -> symlink
    ├── overlay-luckfox-buildroot-shadow -> symlink
    └── overlay-motionsense/etc/init.d/S99motionsense

sysdrv/source/kernel/arch/arm/boot/dts/rv1106g-motionsense.dts
sysdrv/tools/board/buildroot/motionsense_defconfig
```

**The symlinks are load-bearing.** `post_overlay()` and the pre-build hooks
resolve names against the board config's *own* directory, and
`post_overlay()` guards with `[ -d ]` and skips a missing overlay silently.
Without them the build still succeeds while quietly dropping the vendor init
scripts from the image.

The board config is not in `./build.sh lunch`'s numbered menu — that list is
hardcoded in `build.sh`. Pick it through the last entry, `custom`, which lists
everything matching `BoardConfig_*/BoardConfig*.mk`. The index (15 above) will
shift if board configs are added or removed.

### Partitions

```
256K(env) 256K(idblock) 512K(uboot) 4M(boot) 60M(oem) 10M(userdata) 180M(rootfs)
```

255MB of the 256MB SPI NAND. Recent occupancy:

| Partition | Size | Used |
|---|---|---|
| boot | 4 MB | 3.55 MB |
| oem | 60 MB | 9.75 MB |
| userdata | 10 MB | 1.88 MB |
| rootfs | 180 MB | 52.00 MB |

**Nothing validates images against partition sizes.** An oversized image
builds and flashes without complaint and fails at boot. `boot` is the tight
one at 11% headroom; adding kernel drivers needs a check afterwards.

### Device tree

`rv1106g-motionsense.dts` includes the vendor `rv1106-luckfox-pico-pro-max-ipc.dtsi`
and overrides only what differs, so the vendor dts is never edited. Adding a
`.dts` needs no Makefile change: the kernel builds it via `make <name>.img`
through the generic `%.dtb` pattern rule, and none of the vendor luckfox dts
files are registered in `arch/arm/boot/dts/Makefile` either.

It also carries a fix the vendor dts has wrong: the sdmmc node there spells
the property `max-freqency`, so it is ignored and the controller inherits
`max-frequency = <200000000>` from `rv1106.dtsi`.

---

## Upstream deltas that cannot be moved

Most product changes live in the directories above. Eight vendor files still
carry edits, because they are where build switches are *read* rather than
declared. These need merging by hand when syncing upstream:

| File | Why |
|---|---|
| `media/luckfox/Makefile` | reads `RK_ENABLE_LUCKFOX_TEST` |
| `media/sysutils/Makefile` | reads `RK_ENABLE_SYSUTILS_TESTS` |
| `media/ive/ive/CMakeLists.txt`, `media/rga/Makefile`, `media/rockit/rockit/mpi/CMakeLists.txt` | read `RK_ENABLE_SAMPLE` and friends |
| `media/cfg/cfg.mk` | `CONFIG_RK_ISP_BUILD_DEMO=n`, a value not a switch |
| `sysdrv/Makefile` | passes `BR2_CCACHE_DIR`; copies `*_defconfig` so a board can ship its own |
| `sysdrv/tools/board/buildroot/luckfox_pico_defconfig` | `BR2_DL_DIR` |

Note the `sysdrv/Makefile` defconfig copy only runs when buildroot has not
been extracted yet. After changing `motionsense_defconfig` on an existing
tree, copy it in by hand or the build will use a stale one:

```bash
cp sysdrv/tools/board/buildroot/*_defconfig \
   sysdrv/source/buildroot/buildroot-2023.02.6/configs/
```

---

## Verified

A full image built from this tree has been flashed to a Pico Pro Max and run:
the board comes up on `rv1106g-motionsense.dts`, the SD card auto-mounts
read-write, both daemons autostart, the ISP initialises the sensor at
2304x1296, HLS segments and the per-day `EventLogs.db` land in `DCIM`, the
agent connects to the frame socket, and `/api/stream` delivers 15.0 fps at
44.7KB per frame.

## Not covered yet

- **No LICENSE file.** The repo is public without one.
- `statusResponse.LastDeletion` is declared but never assigned, so `/status`
  always reports it as `""`.
