#!/bin/bash
# Build, push and run MotionSense on the Luckfox over adb.
#
# Deploy layout on device:
#   /oem/usr/bin/MotionSense              C core binary (+ future Go agent here too)
#   /oem/usr/lib/                         shared libs
#   /oem/usr/share/MotionSense/fonts/     OSD font
#   /etc/init.d/S99motionsense            boot autostart script (deploy mode)
#
# Runtime data lives on the SD card (see the C config): config.yaml, the
# DCIM recordings and the log file. /oem only holds code and static assets.
#
# Usage:
#   ./deploy.sh            build + push + run in foreground (ctrl-c stops it)
#   ./deploy.sh build      build only
#   ./deploy.sh deploy     build + push + install S99 + (re)start as a daemon
#   ./deploy.sh pushcfg    force-push config.yaml -> SD card (overwrites device copy)
#   ./deploy.sh stop       stop any running instance on the device
#   ./deploy.sh clean      wipe build/ and install/
#
# config.yaml lives on the SD card and is the device's tuning knob, so deploy/run
# only seed it when missing (never clobber on-device edits). Use `pushcfg` to push
# local config.yaml changes on purpose.

set -euo pipefail

RED='\033[0;31m'; GREEN='\033[32;1m'; YELLOW='\033[33;1m'; NC='\033[0m'
log()  { echo -e "${GREEN}==>${NC} $*"; }
warn() { echo -e "${YELLOW}==>${NC} $*"; }
die()  { echo -e "${RED}error:${NC} $*" >&2; exit 1; }

ROOT=$(cd "$(dirname "$0")" && pwd)
BUILD="$ROOT/build"
INSTALL="$ROOT/install/MotionSense"

BIN_DIR="/oem/usr/bin"
LIB_DIR="/oem/usr/lib"
SHARE_DIR="/oem/usr/share/MotionSense"
DAEMON="$BIN_DIR/MotionSense"
CONFIG_SRC="$ROOT/config.yaml"
CONFIG_DST="/mnt/sdcard/MotionSense/config.yaml"
# Boot-autostart location: busybox rcS runs /etc/init.d/S?? scripts in order, so
# this S99 runs after S21appinit (which starts rkipc) and hands the camera over.
INIT_SCRIPT="/etc/init.d/S99motionsense"
# Process names to stop. The device's busybox has no pkill/pgrep, so we stop by
# name with killall. Keep the "which/how many processes" knowledge here: append
# the Go agent's binary name when it lands instead of hardcoding it below.
APPS="MotionSense"

MODE="${1:-run}"

stop_remote() {
    adb shell "
        [ -f '$INIT_SCRIPT' ] && sh '$INIT_SCRIPT' stop 2>/dev/null
        for app in $APPS; do killall \"\$app\" 2>/dev/null; done
        true
    "
}

# Wait until the old instance is really gone before relaunching. A graceful stop
# drains the writer and closes sqlite, which takes a moment; restarting too soon
# races it and the new process hits "database is locked" / VI-busy.
# Match the daemon by its full path so we don't count monitoring pipelines like
# `tail -f messages | grep MotionSense` (the bracket also avoids matching grep itself).
wait_stopped() {
    local i=0
    while [ "$(adb shell "ps | grep -c '[/]oem/usr/bin/MotionSense'" | tr -d '\r')" != "0" ]; do
        i=$((i + 1))
        [ "$i" -ge 10 ] && { warn "old instance still present after ${i}s, proceeding anyway"; break; }
        sleep 1
    done
}

do_build() {
    log "building C core"
    cmake -B "$BUILD" -S "$ROOT" -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    make -C "$BUILD" -j"$(nproc)" install
    [ -x "$INSTALL/MotionSense" ] || die "$INSTALL/MotionSense not found after build"
}

push_payload() {
    log "pushing binary -> $BIN_DIR, fonts -> $SHARE_DIR"
    adb shell "mkdir -p '$BIN_DIR' '$SHARE_DIR'"
    adb push "$INSTALL/MotionSense" "$BIN_DIR/"
    adb push "$INSTALL/fonts"       "$SHARE_DIR/"   # -> /oem/usr/share/MotionSense/fonts/
    adb shell "chmod +x '$DAEMON'"
    # No libs are pushed: every NEEDED shared object already ships on the device
    # (librockit/librkaiq/librockchip_mpp/librga in /oem/usr/lib, libiconv in
    # /usr/lib), and freetype is linked statically. LD_LIBRARY_PATH=$LIB_DIR at
    # launch makes the /oem/usr/lib ones resolve. If a future build needs a lib
    # that isn't on the device, push it to $LIB_DIR here.
}

push_config() {
    [ -f "$CONFIG_SRC" ] || die "$CONFIG_SRC not found"
    adb shell "mkdir -p '$(dirname "$CONFIG_DST")'"
    adb push "$CONFIG_SRC" "$CONFIG_DST"
}

# Seed config.yaml on first provision only; never overwrite on-device tuning.
seed_config() {
    if [ "$(adb shell "[ -f '$CONFIG_DST' ] && echo yes" | tr -d '\r')" = "yes" ]; then
        warn "config exists on device, leaving it untouched (use 'pushcfg' to overwrite)"
    else
        log "seeding config.yaml -> $CONFIG_DST"
        push_config
    fi
}

ensure_modules() {
    adb shell '
        if [ ! -d /dev/mpi ]; then
            echo "[deploy] /dev/mpi missing, running insmod_ko.sh"
            cd /oem/usr/ko && sh insmod_ko.sh >/dev/null 2>&1
        fi
        [ -d /dev/mpi ] || { echo "[deploy] ERROR: /dev/mpi still missing"; exit 1; }
    '
}

# ── modes that exit early ──────────────────────────────────────────────────────
case "$MODE" in
    clean)
        rm -rf "$BUILD" "$ROOT/install"
        log "cleaned"; exit 0 ;;
    pushcfg)
        log "force-pushing config.yaml -> $CONFIG_DST"
        push_config; exit 0 ;;
    stop)
        log "stopping MotionSense on device"
        stop_remote; exit 0 ;;
    build)
        do_build
        log "built: $INSTALL"; exit 0 ;;
    run|deploy)
        ;;  # fall through to the shared build + push path below
    *)
        die "usage: $0 {<none>|build|deploy|pushcfg|stop|clean}" ;;
esac

# ── shared: build, stop whatever is running, push code, load modules ────────────
do_build

log "stopping existing instances"
adb shell '/oem/usr/bin/RkLunch-stop.sh 2>/dev/null; true'
stop_remote
wait_stopped

push_payload
seed_config
ensure_modules

# ── deploy: install init script and (re)start as a background daemon ────────────
if [ "$MODE" = "deploy" ]; then
    log "installing boot init script -> $INIT_SCRIPT + restarting as daemon"
    adb push "$ROOT/S99motionsense" "$INIT_SCRIPT"
    adb shell "chmod +x '$INIT_SCRIPT'"
    adb shell "sh '$INIT_SCRIPT' start"
    log "deployed — autostarts on boot via $INIT_SCRIPT"
    exit 0
fi

# ── run (default): foreground, ctrl-c stops the remote program(s) ──────────────
# adb does not reliably forward the interrupt to the remote process, so the
# trap stops it explicitly on the device.
trap 'echo; warn "interrupted — stopping device program"; stop_remote; exit 130' INT
log "running in foreground (ctrl-c to stop)"
adb shell "LD_LIBRARY_PATH='$LIB_DIR' '$DAEMON'"
