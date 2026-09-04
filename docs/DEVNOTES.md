# Development notes

Working practices for this board, recorded because each one cost time to
establish. Build and configuration details are in [BUILD.md](BUILD.md); this
file is about the loop you work in.

## Connections

Use all three at once. They fail independently, which is what makes them worth
having.

| Link | Use |
|---|---|
| Ethernet | Preview and API. Fast, and unaffected by anything that disturbs USB. |
| USB (adb) | Flashing, `adb reboot loader`, pushing binaries, shell. |
| Serial (CP210x, 115200) | Kernel log and a shell that survives the other two. |

The serial console is not only for logs. It is the only channel left when the
rootfs cannot be read: with the SPI NAND controller timing out, `/bin/login`
itself failed to execute, so both ssh and adb were unavailable while the
console still printed.

Ethernet also serves as a second opinion on whether the device is alive. adb
reporting `offline` does not mean the board is down — see below.

## adb goes offline after the daemon is killed

Observed repeatedly on adb 1.0.41: after `killall -9 MotionSense`, USB
re-enumerates and the host's adb server keeps a stale handle. `adb devices`
shows `offline` while the board answers HTTP normally.

Recovery is `adb kill-server`, or waiting — it often returns on its own.

The same re-enumeration changes the RNDIS interface's MAC, so the host creates
a *new* netdev (`enxAAAA` → `enxBBBB`) and any IPv4 address configured on the
old one is gone. If USB networking stops working, check `ip -br addr` for a
renamed interface before assuming the device is at fault.

## Stop services with the init script

Use `/etc/init.d/S99motionsense stop`, not `killall -9`.

`SIGKILL` gives the daemon no chance to release the ISP or close its files,
and it is what precedes every adb drop above. During one session it also
interrupted an `adb push` mid-write, leaving a corrupt binary on the device
that segfaulted at start — the push had reported success.

## Verify a push by checksum

`adb push` reporting success is not sufficient.

```bash
adb push project/app/out/bin/motionsense-agent /oem/usr/bin/motionsense-agent
adb shell 'sync; md5sum /oem/usr/bin/motionsense-agent'
md5sum project/app/out/bin/motionsense-agent
```

## Judge recording from a reboot, not a service restart

Stopping and starting the service leaves the ISP and VI unable to resume
capture: the daemon runs, the day and segment directories are created, the
databases are written, and no `.ts` appears. It is indistinguishable from
having broken the recording path.

Confirmed with a binary byte-identical to one that had been recording minutes
earlier, so the restart alone accounts for it. Reboot before concluding
anything about recording.

## Containers as a toolbox

The devcontainer, or a throwaway image, gets you tools without installing them
on the host.

```bash
# JS syntax check with no node on the host
docker run --rm -v "$PWD/static/js":/js node:20-alpine node --check /js/app.js

# remove build artefacts owned by root
docker run --rm -v "$PWD":/x alpine:latest rm -rf /x/build-output
```

Access to `/dev` and the serial port needs `--privileged -v /dev:/dev`, which
is why the SDK devcontainer declares them. Adding your user to `dialout` is the
better answer where it is an option — the container flag hands the container
every device on the host.

Containers are sometimes the only path that works: `upgrade_tool` segfaults on
this host and runs correctly in the container. Userspace difference, same
binary.

## Bugs worth remembering

Each of these presents as something other than its cause.

| Symptom | Cause | Fix |
|---|---|---|
| Build fails in the app stage after ~20 min, `output/image` half populated | Go stamps VCS info by running git, which fails under `safe.directory` when the build user does not own the checkout | `-buildvcs=false` — `70d750e` |
| `/mnt/sdcard` never mounts, no daemon starts, nothing explains why | Card's ext4 carries `orphan_file` (e2fsprogs 1.47+); the 5.10 kernel cannot mount it read-write, and the auto-mount rule asks for read-write | `tune2fs -O ^orphan_file` — see BUILD.md |
| Card enumerates on reseat but not at boot | Vendor dts sets `non-removable`, so the core scans once and never uses the card-detect pin; this card does not answer at the first probe frequency | dts omits it — `c5aaf49be2` (SDK) |
| First `.ts` of a session fails with `MEDIA_ERR_DECODE`, later ones play | Encoder does not necessarily return an IDR first, and libhls opens a segment on whatever it is given, producing a first segment with no SPS/PPS | Drop frames until the first keyframe — `ab8af80` |
| Daemon exits at start with `can't load library 'librockchip_mpp.so.1'` while the file is present | RPATH resolves the daemon's own NEEDED entries but not `librockit.so`'s | `LD_LIBRARY_PATH` in the init script — `aaf319bb67` (SDK) |
| Clicking a past date returns 500 and a JSON parse error | Day with no `EventLogs.db`; `mode=ro` fails on a missing file | Empty day is 200 with no segments — `dc48835` |
| Overlay reads "Streaming" but the picture is black | `<img>` still holds the connection that died with the daemon; `src` unchanged, so the browser considers it loaded | Re-request with a fresh query string — `74beb6d` |
| Clock runs eight hours ahead a minute or two into every boot, RTC correct throughout | rkipc ships its own NTP client (`[network.ntp] enable = 1`, 60 s), adds the timezone to the UTC it receives, and has busybox write that back as UTC. `RkLunch-stop.sh` at S99 cannot stop it: RkLunch starts rkipc from a backgrounded `post_chk` that waits on /userdata first, so the stop call runs before the process exists | Drop rkipc and its inis in the oem hook — `529f3537f6` (SDK) |

## Unexplained

One session ended with the SPI NAND and SD controllers both timing out:

```
rockchip-sfc ffac0000.spi: wait sfc idle timeout
ubi0 error: ubi_io_read: error -5 while reading 188 bytes from PEB 413
dwmmc_rockchip ffaa0000.mmc: DTO timeout when already completed
mmc1: tried to HW reset card, got error -110
```

The agent kept answering, being already resident. A power cycle cleared it and
the next boot showed no storage errors. It followed a session with repeated
`killall -9` against a daemon that was writing to the card, but that is a
correlation, not a diagnosis. Recorded in case it recurs.

---

Written by Claude (Opus 5) while working on this board. Reviewed by ZIXUAN ZHU.
