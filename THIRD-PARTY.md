# Third-party components

`LICENSE` (MIT) covers the code written for this project. This file draws the
boundary: what else is in the tree, who owns it, and under what terms.

## Covered by LICENSE (MIT)

| Path | |
|---|---|
| `src/`, `include/` | the C daemon |
| `agent/` | the Go agent and its web assets, except `agent/static/js/video-js-*/` |
| `cmake/`, `scripts/`, `Makefile`, `CMakeLists.txt` | build |
| `config.yaml`, `S99motionsense`, `*.sh`, `BUILD.md` | packaging and docs |

## Distributed here, under their own terms

| Path | Owner | License |
|---|---|---|
| `3rdparty/osd/*.c`, `*.h` | Rockchip Electronics Co., Ltd. | 3-clause BSD — see `3rdparty/osd/LICENSE` |
| `3rdparty/osd/DejaVuSansMono.ttf` | Bitstream, Inc.; DejaVu and Arev authors | Bitstream Vera + Arev — see `3rdparty/osd/DejaVuSansMono.LICENSE` |
| `agent/static/js/video-js-8.20.0/` | Brightcove / Video.js authors | Apache-2.0 |

The BSD terms on the Rockchip OSD sources require the copyright notice and
conditions to travel with any redistribution. `3rdparty/osd/LICENSE` is that
text; the file headers already point at it.

## Referenced, not distributed

| Path | Owner | How it gets there |
|---|---|---|
| `3rdparty/media-server/` | ireader | MIT. A submodule pinned at `ea53ac6` — a pointer, not a copy. `3rdparty/media-server-local/hls-media.c` is one file of it carrying a local fix, and is redistributed under the same MIT terms. |
| `vendor/` | Rockchip Electronics Co., Ltd. | **Not in this repository.** `scripts/sync-vendor.sh` copies it out of a Luckfox SDK checkout. |

## Why vendor/ is not here

`vendor/` holds Rockchip's prebuilt `librockit`, `librkaiq`, `librockchip_mpp`
and `librga` plus 462 headers. They carry `Copyright (c) 2021 Fuzhou Rockchip
Electronics Co., Ltd` and, unlike the OSD sources, no redistribution grant:
none of the rockit or mpp headers state any terms, and the trees ship no
LICENSE file. Whatever rights Luckfox has to ship them inside their SDK do not
obviously extend to a third-party repository re-publishing them, so this
project does not.

Nothing is lost by that. The SDK app build never reads `vendor/` — it points
at the SDK's own copies through `MS_RK_MEDIA_DIR`. A standalone build runs
`scripts/sync-vendor.sh`, which copies them from an SDK checkout; the SDK
ships them under `media/` as source, so a plain clone suffices and no SDK
build is needed.

## Note on the font

Until this was written the OSD font was `simsun_en.ttf`, carrying
`(c) Copyright ZHONGYI Electronic Co. 1995`. SimSun is a commercial typeface
distributed with Windows, and shipping it from here was not defensible. It is
replaced by DejaVu Sans Mono, already present in the tree for the OLED code.
The OSD only ever draws an ASCII timestamp (`%H:%M:%S  %Y-%m-%d`), so nothing
is lost.

## Disclaimer

This is an inventory of what the files themselves state, assembled by reading
their headers — not legal advice. If the licensing matters to you, check it
yourself.
