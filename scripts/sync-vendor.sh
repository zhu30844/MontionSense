#!/bin/bash
# Populate vendor/ from a Luckfox SDK checkout.
#
# vendor/ holds Rockchip's prebuilt media libraries and their headers. They
# carry a copyright notice and no redistribution grant, so they are not kept
# in this repository; this script copies them out of an SDK checkout instead.
#
# The SDK ships them under media/ as source, so a plain clone is enough — no
# SDK build required, and nothing here depends on output/out/media_out.
#
# Only needed for a standalone build. Building through the SDK app tree passes
# -DMS_RK_MEDIA_DIR and never looks at vendor/ at all.
set -euo pipefail

HERE=$(cd "$(dirname "$(realpath "$0")")" && pwd)
APP_DIR=$(dirname "$HERE")

SDK=${1:-$(cd "$APP_DIR/../../.." && pwd)}
if [ ! -d "$SDK/media" ]; then
    echo "Not an SDK checkout: $SDK" >&2
    echo "Usage: $0 [path-to-luckfox-pico-sdk]" >&2
    exit 1
fi

RV=arm-rockchip830-linux-uclibcgnueabihf
ISP="$SDK/media/isp/release_camera_engine_rkaiq_rv1106_$RV"
MPP="$SDK/media/mpp/release_mpp_rv1106_$RV"
RGA="$SDK/media/rga/release_rga_rv1106_$RV"
ROCKIT="$SDK/media/rockit"

copy() {  # copy <src> <dst>
    [ -e "$1" ] || { echo "missing in SDK: $1" >&2; exit 1; }
    mkdir -p "$(dirname "$2")"
    cp -a "$1" "$2"
}

echo "Syncing vendor/ from $SDK"
rm -rf "$APP_DIR"/vendor/{rockit,rkaiq,mpp,rga}/{include,lib}

copy "$ROCKIT/out/include"  "$APP_DIR/vendor/rockit/include"
copy "$ROCKIT/rockit/lib/lib32/librockit.so" "$APP_DIR/vendor/rockit/lib/librockit.so"

copy "$ISP/include"         "$APP_DIR/vendor/rkaiq/include"
copy "$ISP/lib/librkaiq.so" "$APP_DIR/vendor/rkaiq/lib/librkaiq.so"

copy "$MPP/include"         "$APP_DIR/vendor/mpp/include"
for so in librockchip_mpp.so librockchip_mpp.so.0 librockchip_mpp.so.1; do
    [ -e "$MPP/lib/$so" ] && copy "$MPP/lib/$so" "$APP_DIR/vendor/mpp/lib/$so"
done

copy "$RGA/lib/librga.so"   "$APP_DIR/vendor/rga/lib/librga.so"

echo "Done. Contents:"
for d in rockit rkaiq mpp rga; do
    printf "  vendor/%-7s %3s headers, %s libs\n" "$d" \
        "$(find "$APP_DIR/vendor/$d/include" -name '*.h' 2>/dev/null | wc -l)" \
        "$(find "$APP_DIR/vendor/$d/lib" -type f 2>/dev/null | wc -l)"
done
