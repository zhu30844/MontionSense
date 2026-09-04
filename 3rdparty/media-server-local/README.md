# Local changes to ireader/media-server

`3rdparty/media-server` is a submodule pinned at upstream `ea53ac6`, kept
pristine. This directory holds the one source file we cannot take unmodified,
so the change stays visible instead of hiding as a dirty submodule.

`cmake/MediaServer.cmake` drops the submodule's copy of `hls-media.c` from the
`hls` target and compiles the one here instead.

## hls-media.c

`hls_media_input()` asserts that DTS advances by less than one segment
duration. The cut logic below it supports `duration == 0`, meaning "start a new
segment on every keyframe", but the assert does not allow for it and fires on
the second frame. `storage.c` uses exactly that mode — `config.yaml` ships
`hls_duration_s: 0` so segment length follows the GOP.

Release builds define `NDEBUG`, so the assert is compiled out and the patch
changes nothing there. A `-DCMAKE_BUILD_TYPE=Debug` build without it aborts as
soon as recording starts.

`hls-media.patch` is the same change as a diff against the pinned commit, for
review. It is not applied by the build; the file next to it is what compiles.

## Refreshing after a submodule bump

    cd 3rdparty/media-server && git show <new-ref>:libhls/source/hls-media.c \
        > ../media-server-local/hls-media.c
    cd ../media-server-local && patch hls-media.c < hls-media.patch

Then rebuild and check the patch still applies cleanly; if upstream has fixed
the assert, drop this directory and the exclusion in cmake/MediaServer.cmake.
