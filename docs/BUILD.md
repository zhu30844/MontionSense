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

adb reboot loader                       # put the board in maskrom/loader mode
./rkflash.sh update
```

`upgrade_tool` segfaults on some hosts and runs correctly inside the
devcontainer, so flash from there if `./rkflash.sh` dies. `rkflash.sh`'s
single-partition targets (`boot`, `rootfs`, ...) do not upload the loader
first and fail with "parameter is invalid"; `update` is the working path.

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

`./build.sh` runs `build_all` then `build_save`:

```
build_all
  ├─ build_sysdrv    u-boot, kernel, buildroot   -> output/out/{sysdrv_out,rootfs_uclibc_rv1106}
  ├─ build_media     Rockchip media libraries    -> output/out/media_out
  ├─ build_app       project/app/Makefile        -> output/out/app_out
  └─ build_firmware  staging, images, update.img -> output/image
build_save                                       -> IMAGE/<name>_<date>_RELEASE_TEST
```

Nothing in the SDK had to be edited to add the app: `project/app/Makefile`
discovers any subdirectory containing a `Makefile`.

```
build_app
  └─ $(wildcard ./*/Makefile) finds motionsense/Makefile
       ├─ c-daemon: cmake configure + build + install -> install-sdk/
       │            MAROC_COPY_PKG_TO_APP_OUTPUT      -> project/app/out/{bin,share}
       └─ go-agent: go build                          -> project/app/out/bin/
     project/app/out -> output/out/app_out
```

### build_firmware

This is where the partition images are assembled. The order matters, because
each step feeds the next:

```
build_env                    partition table + sys_bootargs   -> env.img
build_meta                   fastboot builds only             -> meta.img
__PACKAGE_ROOTFS             sysdrv rootfs tarball, then overlays
                             app_out/root, media_out/root, external/
                             + the generated /etc/init.d mount script
__PACKAGE_OEM                app_out and media_out {bin,lib,share,usr,etc}
                             -> oem staging, plus its init.d script
__RUN_PRE_BUILD_OEM_SCRIPT   RK_PRE_BUILD_OEM_SCRIPT
                             (preceded by __RUN_POST_CLEAN_FILES, which drops
                             the NPU/AIISP/audio models the board did not select)
build_mkimg <oem>            -> oem.img            (RK_BUILD_APP_TO_OEM_PARTITION=y)
__RUN_POST_BUILD_SCRIPT      RK_POST_BUILD_SCRIPT
post_overlay                 RK_POST_OVERLAY, rsync'd over the rootfs staging
build_mkimg <rootfs>         -> rootfs.img
__PACKAGE_USERDATA           media_out/install_to_userdata, app_out/install_to_userdata
__RUN_POST_BUILD_USERDATA_SCRIPT   reads RK_PRE_BUILD_USERDATA_SCRIPT
build_mkimg userdata         -> userdata.img
build_tftp_sd_update
build_updateimg              -> update.img
```

Four board-config hooks run in that sequence, all resolved relative to the
board config's own directory:

| Variable | When | This board |
|---|---|---|
| `RK_PRE_BUILD_OEM_SCRIPT` | after oem staging is populated, before `oem.img` | `motionsense-oem-pre.sh` |
| `RK_POST_BUILD_SCRIPT` | after `oem.img`, before overlays | unset |
| `RK_POST_OVERLAY` | after that, onto the rootfs staging | `overlay-luckfox-config`, `-buildroot-init`, `-buildroot-shadow`, `overlay-motionsense` |
| `RK_PRE_BUILD_USERDATA_SCRIPT` | after userdata staging, before `userdata.img` | `luckfox-userdata-pre.sh` |

The oem hook is the place to strip files from the image: it runs against the
staging directory after everything has been copied in and before the
filesystem is made. `motionsense-oem-pre.sh` uses it to drop the demo and test
binaries the media build installs.

Note the vendor naming: the function is `__RUN_POST_BUILD_USERDATA_SCRIPT` but
the variable it reads is `RK_PRE_BUILD_USERDATA_SCRIPT`.

`build_mkimg` picks the filesystem from `RK_PARTITION_FS_TYPE_CFG` and the
size from `RK_PARTITION_CMD_IN_ENV`. For ubifs it calls
`sysdrv/tools/pc/mtd-utils/mkfs_ubi.sh`, which builds three variants (2KB/128KB,
2KB/256KB, 4KB/256KB page/block) and symlinks the one matching the default
geometry to `<name>.img`.

### What Luckfox added on top of the Rockchip SDK

The hook and overlay machinery is Rockchip's; the scripts filling it are
Luckfox's, and they all live in `project/cfg/BoardConfig_IPC/`.

| Script | Hook | Used by |
|---|---|---|
| `luckfox-buildroot-oem-pre.sh` | `RK_PRE_BUILD_OEM_SCRIPT` | 11 board configs |
| `luckfox-buildroot-nocsi-oem-pre.sh` | same | 2 (boards with no camera) |
| `luckfox-glibc-oem-pre.sh` | same | none currently |
| `luckfox-userdata-pre.sh` | `RK_PRE_BUILD_USERDATA_SCRIPT` | 14 board configs |
| `luckfox-rv1106-tb-spi_nand-post.sh` | `RK_POST_BUILD_SCRIPT` | the SPI NAND fastboot config |
| `luckfox-rv1106-tb-emmc-post.sh` | same | the eMMC fastboot config |
| `luckfox-systemd-off-modem-post.sh` | same | none currently |

The `*-oem-pre.sh` scripts are all the same shape: an `lf_rm` helper and a
`remove_data()` that deletes from `$RK_PROJECT_PACKAGE_OEM_DIR` — the AIISP
models, `libdrm`, `libkms`, `libfreetype`, `libiconv`, `librkAVS`, `libjpeg`,
`libpng`, and the vqe data files. `motionsense-oem-pre.sh` is that file plus
this board's own removals.

`luckfox-userdata-pre.sh` deletes `*.sh` and `*.bmp` from the userdata staging.

The fastboot `*-post.sh` scripts are larger: they prune `usr/ko` down to the
modules a fast boot needs and strip the oem tree to a keep-list.

Overlays, same directory under `overlay/`:

| Overlay | Installs |
|---|---|
| `overlay-luckfox-config` | `usr/bin/luckfox-config` and two init scripts, `S99luckfoxconfigload` (runs `luckfox-config load` at boot) and `S99luckfoxcustomoverlay` |
| `overlay-luckfox-buildroot-init` | `S50sshd`, `S60micinit`, `S99hciinit`, `S99python`, `S99rtcinit`, `S99usb0config`, `etc/profile`, `usr/bin/iomux` |
| `overlay-luckfox-buildroot-shadow` | `etc/shadow`, `etc/ssh/sshd_config`, `etc/samba/{smb.conf,smbpasswd}` |
| `overlay-luckfox-wifibt-firmware` | rtl8723b firmware blobs |
| `overlay-luckfox-fastboot`, `-buildroot-tiny` | trimmed `etc/inittab`, `etc/fstab` for fast boot |
| `-buildroot-86panel`, `-rgb`, `-webbee`, `-sim7600g`, `-ppp`, `-glibc-*` | other Luckfox boards |

This board uses the first three.

`S99usb0config` is the one to know about: it sets `usb0` to `172.32.0.93` when
the USB controller is in peripheral mode, retrying up to ten times. That is
where the RNDIS address comes from.

Luckfox also added to `build.sh` itself: the `LF_*` board-config variables
(`LF_TARGET_ROOTFS`, `LF_BOOT_MEDIA`, `LF_HARDWARE`, `LF_SYSTEM`,
`LF_ORIGIN_BOARD_CONFIG`, `LF_WIFI_SSID`/`LF_WIFI_PSK`,
`LF_ENABLE_SPI_NAND_FAST_BOOT`), the `lunch` menu built from those arrays, and
`lf_blkenvpackage`, which packs `sd_update.img` for SD-card boot mediums.

`Makefile` here is a thin wrapper. It includes `../Makefile.param` for the
cross toolchain and media paths, then drives the same CMake build the
standalone flow uses. Two details that bite if you copy it elsewhere:

- `CURRENT_DIR` must be evaluated *before* `include ../Makefile.param`, which
  pulls in `.BoardConfig.mk` and would otherwise be what
  `$(lastword $(MAKEFILE_LIST))` points at.
- `SHELL := /bin/bash` is required. `Makefile.param`'s copy and strip macros
  use `[[ ]]`, and GNU make does not pass `SHELL` down to sub-makes.

### Build parallelism

Buildroot serialises itself. Its Makefile declares `.NOTPARALLEL` unless every
package gets its own target and host directory, so without that the packages
build strictly one after another however many cores the host has, and the
board config turns the isolation on:

```
BR2_PER_PACKAGE_DIRECTORIES=y
```

Switching this changes the output layout, so buildroot has to be built from an
empty tree. `./build.sh clean rootfs` is not enough — it leaves
`output/{build,host,staging,target}` in place, and the first package then dies
in rsync with `mkdir .../per-package/<pkg>/target failed`. Delete
`sysdrv/source/buildroot/buildroot-2023.02.6/output` and its `.config` by hand
before the first build after the change. It is also experimental in
buildroot's own words: a package that silently relies on something it never
declared as a dependency builds today and fails under isolation.

The job count comes from `RK_JOBS`, three quarters of the host by default.
Override it with `LF_JOBS`, not `RK_JOBS` — `unset_env_config_rk` at the top
of `build.sh` blanks every `RK_*` it finds in the environment, so an `RK_JOBS`
from the caller is already gone by the time the default is applied:

```bash
LF_JOBS=$(nproc) ./build.sh rootfs
```

Raising it is not worth much. Measured on a 24-core host, rootfs from an empty
output directory with a warm ccache:

| jobs | wall |
|---|---|
| 12 | 148 s |
| 18 (the default) | 146 s |
| 24 | 144 s |
| 32 | 144 s |

Four seconds across the range. Load peaks around 7-13 and never approaches the
core count, because what limits this build is the dependency graph and the
serial parts of each package -- configure, install, the per-package rsync --
not the number of slots. Set `LF_JOBS` when you want to leave the machine
usable during a build, not to make the build faster.

`BR2_JLEVEL` is not the knob it looks like. Buildroot omits its own `-j` when
`MAKEFLAGS` already carries one, and `sysdrv/Makefile` always passes
`-j$(SYSDRV_JOBS)`, so package sub-makes take their parallelism from that
jobserver — `.NOTPARALLEL` only serialises the make that declares it, not
recursive ones. What `BR2_JLEVEL` still governs is a bare `make` run by hand
inside the buildroot tree, which is how a single package gets debugged; the
vendor base pinned it at 4 and the fragment sets it to 0 (one job per CPU
plus one).

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

A standalone build also needs `vendor/`, which is not in the repository — see
*Rockchip libraries* below. Populate it once with:

```bash
./scripts/sync-vendor.sh            # or: ./scripts/sync-vendor.sh <sdk-path>
```

### The Go agent

```
GOOS=linux GOARCH=arm GOARM=7 CGO_ENABLED=0 go build -trimpath -ldflags="-s -w"
```

No CGO is needed: `modernc.org/sqlite` is a pure-Go implementation, so the
binary is static and does not link against uclibc. `-s -w -trimpath` takes it
from 15.7MB to 11MB.

The Makefile builds the agent after `MAROC_STRIP_DEBUG_SYMBOL`, keeping the Go
binary out of that step; it runs GNU `strip`.

`-buildvcs=false` is required. Go otherwise runs git to stamp the commit into
the binary and fails the build where the user does not own the checkout, which
covers `docker run` as root and most CI:

```
fatal: detected dubious ownership in repository at '.../project/app/motionsense'
error obtaining VCS status: exit status 128
```

`agent/` has no `vendor/` directory, so a cold build downloads modules and
needs network access.

The agent listens on `:5000`, serves the web assets embedded via `go:embed`,
and reads frames from the C daemon over `/tmp/motionsense.sock`. It tolerates
starting before the C daemon: a missing database is logged and skipped, and
the socket reader retries.

### Pushing to a running board over adb

`deploy.sh` builds and pushes the binary without reflashing. It pushes no
libraries, so the device must already carry them. Images built before the
switch to buildroot-provided libraries have no `libsqlite3.so.0` or
`libyaml-0.so.2`, and the daemon will not start. Flash `update.img` once on
such a device, after which `deploy.sh` works.

---

## Where dependencies come from

| Dependency | Source | Ships in |
|---|---|---|
| `librockit`, `librkaiq`, `librockchip_mpp`, `librga` | `output/out/media_out` (SDK build) | `/oem/usr/lib` |
| `libsqlite3`, `libyaml`, `libfreetype`, `libiconv` | buildroot sysroot | `/usr/lib` (rootfs) |
| `libhls`/`libmpeg` (`3rdparty/media-server`, submodule), `libosd` (`3rdparty/osd`) | built from source, static | linked in |

### Rockchip libraries

Selected by `MS_RK_MEDIA_DIR`: set — which the SDK `Makefile` does — the app
builds against `output/out/media_out`, tracking what the SDK just built. Unset,
it falls back to `vendor/`.

`vendor/` is not in the repository. `scripts/sync-vendor.sh` creates it,
extracting the libraries and headers from an SDK checkout. It reads `media/`,
where the SDK ships them as source, so a plain SDK clone is enough and no SDK
build is required.

Only standalone builds need it. The SDK app build reads `output/out/media_out`.

The script copies the four `.so` files and their headers. It does not copy the
`.a` archives or `librockit_full/tiny.so`; nothing links them.

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

When bumping the pin, match on the `libmpeg/source`, `libmpeg/include` and
`libhls/include` tree hashes rather than on a single file. Matching on
`hls-media.c` alone selects `8928fa5`, the newest upstream revision of a
rarely-touched file, where 20 of the other 48 files differ and the resulting
binary is 8KB smaller. At `ea53ac6`, 48 of the 49 compiled files are
byte-identical to the previously vendored copies.

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

The binary is linked with `RPATH=$ORIGIN/../lib`, resolving to `/oem/usr/lib`
from `/oem/usr/bin`. This covers the daemon's own `NEEDED` entries but not the
transitive ones: `librockit.so` needs `librockchip_mpp.so.1` and `librga.so`,
and resolving a library's dependencies does not consult the executable's
RPATH. `S99motionsense` therefore exports `LD_LIBRARY_PATH=/oem/usr/lib`.
Without it the daemon exits at exec with `can't load library
'librockchip_mpp.so.1'`.

### Timezone

The image carries no zoneinfo database, so a named zone does not resolve.
`overlay-motionsense/etc/TZ` holds a POSIX zone string, which uclibc reads
directly:

```
AWST-8
```

The sign is inverted from what the name suggests: `AWST-8` means UTC+8. It
reaches the image through `RK_POST_OVERLAY` like everything else in that
directory, so no separate step is needed at packaging time.

Without it the device runs on UTC, and both the OSD timestamp and the
recording start times — which call `localtime()` — are off by the offset.

The clock itself comes from the RTC at boot and from ntpd afterwards. A cold
start with a flat RTC battery records its first segment under whatever date
the clock held before ntpd corrects it.

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

**The filesystem must not use `orphan_file`.** e2fsprogs 1.47 and later enable
it by default. It occupies ext4 ro_compat bit 16, which this 5.10 kernel does
not support, so the card mounts read-only and the auto-mount rule — which asks
for read-write — fails. `/mnt/sdcard` never appears and `S99motionsense` times
out without starting either daemon. To clear it, on a PC:

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

`rv1106g-motionsense.dts` therefore omits the vendor's `non-removable`. With
that property set, the core scans once at probe, ignores the card-detect pin
and never retries, making a boot-time failure permanent; reseating the card
raises no event.

Bus speed is capped at SD High Speed, 50MHz — `no-1-8-v` rules out the UHS
modes, which need 1.8V signalling. Measured on a 119GiB SDXC card:

| | |
|---|---|
| sequential read | 22.1 MB/s |
| sequential write | 19.0 MB/s |
| 4K random read | 0.70 ms median |
| 4K synced write | 9.4 ms median, 15 ms p95, 32 ms worst |

Recording at a few Mbps uses a small fraction of the bandwidth. The
synced-write latency applies if the metadata database is committed with
`PRAGMA synchronous=FULL`.

### Reflashing just the app

The app is on its own partition, so iterating does not need a full reflash:

```bash
./rkflash.sh oem
```

`rkflash.sh` also accepts `boot`, `rootfs`, `userdata`, `uboot`, `loader`,
`update`, `erase`.

Pushing the binary over adb and restarting the service is faster still.

**Judge recording from a full reboot, not from a service restart.** Stopping
and starting S99motionsense leaves the ISP and VI unable to resume capture:
the daemon runs, the day and segment directories are created, the databases
are written, and no `.ts` is produced. A reboot recovers it. Confirmed with a
binary byte-identical to one that had been recording immediately before.

---

## Board configuration

Product-specific configuration lives in its own directory in the SDK fork,
leaving upstream Luckfox files untouched:

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

The symlinks are load-bearing. `post_overlay()` and the pre-build hooks
resolve names against the board config's own directory, and `post_overlay()`
guards with `[ -d ]`, skipping a missing overlay silently. Without them the
build succeeds while dropping the vendor init scripts from the image.

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

Nothing validates images against partition sizes. An oversized image builds
and flashes without complaint and fails at boot. `boot` has the least
headroom at 11%; check it after adding kernel drivers.

### Device tree

`rv1106g-motionsense.dts` includes the vendor `rv1106-luckfox-pico-pro-max-ipc.dtsi`
and overrides only what differs, so the vendor dts is never edited. Adding a
`.dts` needs no Makefile change: the kernel builds it via `make <name>.img`
through the generic `%.dtb` pattern rule, and none of the vendor luckfox dts
files are registered in `arch/arm/boot/dts/Makefile` either.

It also corrects the vendor dts, where the sdmmc node spells the property
`max-freqency`; that spelling is ignored and the controller inherits
`max-frequency = <200000000>` from `rv1106.dtsi`.

---

## Upstream deltas that cannot be moved

Product changes live in the directories above. Eight vendor files carry edits
that cannot move there, because they are where build switches are read rather
than declared. These need merging by hand when syncing upstream:

| File | Why |
|---|---|
| `media/luckfox/Makefile` | reads `RK_ENABLE_LUCKFOX_TEST` |
| `media/sysutils/Makefile` | reads `RK_ENABLE_SYSUTILS_TESTS` |
| `media/ive/ive/CMakeLists.txt`, `media/rga/Makefile`, `media/rockit/rockit/mpi/CMakeLists.txt` | read `RK_ENABLE_SAMPLE` and friends |
| `media/cfg/cfg.mk` | `CONFIG_RK_ISP_BUILD_DEMO=n`, a value not a switch |
| `sysdrv/Makefile` | passes `BR2_CCACHE_DIR`; copies `*_defconfig` so a board can ship its own |
| `sysdrv/tools/board/buildroot/luckfox_pico_defconfig` | `BR2_DL_DIR` |

Turning a package **off** takes more than listing it. A `# BR2_X is not set`
line appended after the base defconfig is read as a comment and changes
nothing, so `gen-buildroot-defconfig.sh` deletes the enabling line from the
base instead.

Buildroot also never removes what a deselected package already installed.
`output/target/` is built up incrementally, so those files stay and are packed
into `rootfs.tar` even though the package is no longer built — deselecting ntp
left `/usr/sbin/ntpd` and `/etc/init.d/S49ntp` in the image. On an existing
tree, force the target to be rebuilt:

```bash
cd sysdrv/source/buildroot/buildroot-2023.02.6 && make target-finalize && make
```

A clean checkout never sees this: the package is simply never built.

The `sysdrv/Makefile` defconfig copy runs only when buildroot has not been
extracted yet. After changing `motionsense_defconfig` on an existing tree,
copy it in manually; otherwise the build uses a stale one:

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

## Licensing

`../LICENSE` is MIT and covers the code written here. `THIRD-PARTY.md` draws the
boundary against everything else: the Rockchip OSD sources (3-clause BSD), the
DejaVu font, Video.js (Apache-2.0), media-server (MIT, a submodule), and the
Rockchip media libraries, which are referenced but not distributed.

---

Written by Claude (Opus 5) from the SDK sources and from testing on a Luckfox
Pico Pro Max. Reviewed by ZIXUAN ZHU.
