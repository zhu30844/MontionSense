# Third-party components

`../LICENSE` (MIT) covers the code written for this project. This file lists
everything else in the tree and the terms it carries. Paths are relative to
the repository root, not to this directory.

## Covered by LICENSE (MIT)

| Path | |
|---|---|
| `src/`, `include/` | the C daemon |
| `agent/` | the Go agent and its web assets, except `agent/static/js/video-js-*/` |
| `cmake/`, `scripts/`, `Makefile`, `CMakeLists.txt` | build |
| `config.yaml`, `S99motionsense`, `*.sh`, `docs/` | packaging and docs |

## Distributed here, under their own terms

| Path | Owner | License |
|---|---|---|
| `3rdparty/osd/*.c`, `*.h` | Rockchip Electronics Co., Ltd. | 3-clause BSD — `3rdparty/osd/LICENSE` |
| `3rdparty/osd/DejaVuSansMono.ttf` | Bitstream, Inc.; DejaVu and Arev authors | Bitstream Vera + Arev — `3rdparty/osd/DejaVuSansMono.LICENSE` |
| `agent/static/js/video-js-8.20.0/` | Brightcove / Video.js authors | Apache-2.0 |

The OSD sources reference their license file from each header; that file is
`3rdparty/osd/LICENSE`.

The OSD font is DejaVu Sans Mono, replacing the font the SDK ships. The OSD
draws an ASCII timestamp only.

## Referenced, not distributed

| Path | Owner | How it gets there |
|---|---|---|
| `3rdparty/media-server/` | ireader | MIT. Submodule pinned at `ea53ac6`. `3rdparty/media-server-local/hls-media.c` is one file of it carrying a local fix, under the same terms. |
| `vendor/` | Rockchip Electronics Co., Ltd. | Not in this repository. Created by `scripts/sync-vendor.sh`, which extracts it from a Luckfox SDK checkout. |

## vendor/

`vendor/` holds Rockchip's prebuilt `librockit`, `librkaiq`, `librockchip_mpp`
and `librga` with 462 headers, carrying `Copyright (c) 2021 Fuzhou Rockchip
Electronics Co., Ltd`.

The headers state their own terms. `librockit` and the ISP headers open with
the SPDX line `GPL-2.0 WITH Linux-syscall-note OR Apache 2.0`; `librkaiq`,
`librockchip_mpp` and `librga` reproduce the Apache-2.0 notice in full. Either
branch permits redistribution in binary form, which is what makes a flashable
image built from this project distributable, provided the notices travel with
it.

The terms that apply are the ones shipped beside the binaries rather than
those of the public upstreams, because the SDK does not ship upstream builds:
the commit `media/mpp` records is not in `rockchip-linux/mpp`, and the one
`media/rga` records is not in `airockchip/librga`. Both are Rockchip-internal
builds whose licence travels in their headers.

It is needed only for standalone builds. The SDK app build reads the SDK's own
copies through `MS_RK_MEDIA_DIR`.

To populate it:

```bash
./scripts/sync-vendor.sh            # or: ./scripts/sync-vendor.sh <sdk-path>
```

The script reads `media/` in the SDK, where these ship as source, so a plain
SDK clone is enough and no SDK build is required.

## Disclaimer

This inventory records what the files themselves state. It is not legal advice.

---

Written by Claude (Opus 5) from the SDK sources and from testing on a Luckfox
Pico Pro Max. Reviewed by ZIXUAN ZHU.
