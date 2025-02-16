# Storage System Design

## Functional requirements
1. Save video in HLS format.
2. Do not drop any frame in ts buffer.
2. Handle the changing input frame rate. 
    + Saved video must be played in a fixed frame rate.
3. Orchestrate the folders.
    + Ensure video files are saved in order after power outage.
4. Utilize databases to record events and video information.
5. Free up disk space if SD card is almost full.
6. Video deletion.
    + Do not damage the integrity of the video. 
        - If there is only one day in record, do not delete any file though free disk is riching THRESHOLD.
        - Make sure delete the **ALL** sub folders and files of **ONE DAY** if deletion is performed.

## Non Functional requirements
1. Reduce disk IO. 
    + Frequency of storage detetion.
    + Frequency of database operations.

## Directory Tree 
```
sdcard
    │
    └─ DCIM
        │
        ├─ 2025-02-09
        │       │
        │       ├─ 00000 
        │       │    ├─ 00001.ts
        │       │    ├─ 00002.ts
        │       │    ├─ 00003.ts
        │       │    └─ index.m3u8
        │       ...
        │       │
        │       ├─ 00009
        │       │    ├─ 00001.ts
        │       │    ...
        │       │    ├─ 09999.ts
        │       │    └─ index.m3u8
        │       │
        │       └─ EventLogs.db
        │
        ├─ 2025-02-10
        │
        └─ VideoMetadata.db
```

## Simplified Flowchart 

```mermaid
graph TD
    INIT[Init storage] --> CHECK_SD[Check SD card] --> CHECK_DCIM[Check DCIM folder] -->|Fail| FAILURE[Return RK_FAILURE]

    CHECK_DCIM[Prepare DCIM folder] --> |Success|CLEAN_UP_THREAD[Create clean-up thread] -->|Fail| FAILURE[Return RK_FAILURE]

    CLEAN_UP_THREAD[Create clean up thread] -->|Success|GET_DATE[Get date information] --> DATEFOLDER[Prepare date folder] --> PREPARE_DB[Prepare database] --> INIT_HLS_MUXER[Configure HLS Muxer] --> INIT_DONE[Init done]
    

    WRITE_FRAME[Recived frame] --> |No SD card| NO_SD_CARD[Return RK_FAILURE]
    CHECK_DATE[Check date] -->|Today| TODAY[Update PTS/motion counter<br> DTS/frame_number] --> PUSH_TO_MUXER[Push frame to HLS Muxer]-->FRAME_RATE_CHANGE[Check if framerate changed] -->|if frame rate<br>changed| UPDATE_EVENT[Update event details<br> to all DBs] -->IF_UPDATE_VIDEO_LENGTH[Check if need <br>updating video length] 

    FRAME_RATE_CHANGE[Check if framerate changed] -->|if frame rate<br>did not changed| IF_UPDATE_VIDEO_LENGTH[Check if need <br>updating video length] -->|If need| UPDATE_VIDEO_LENGTH[Update video length<br>to EventLogs.db]--> WRITE_FRAME_DONE[Write frame done]

    WRITE_FRAME[Recived frame] --> |With SD Card| CHECK_DATE[Check date] --> |New day| UPDATE_DATE_STRING[Get new date string] --> CREATE_DATE_FOLDER[Create date folder <br>eg.sdcard/DCIM/2025-01-01/] -->SWITCH_EVENTLOD_DB[Close yesterday's eventlog<br> Create today's eventlog] -->TS_FOLDER[Create segments folder<br> eg.2025-01-01/00000/] -->RESET_VAR[Reset PTS/motion counter<br> DTS/frame_number] --> TODAY[Update PTS/motion counter<br> DTS/frame_number]

    IF_UPDATE_VIDEO_LENGTH[Check if need <br>updating video length] -->|If no need|WRITE_FRAME_DONE[Write frame done]
```

## Media-server
[GitHub Link](https://github.com/ireader/media-server.git)
We only used two sub libraries, `hls` and `mpeg`, to encapsulate ts and m3u8 files. So we do not have to build and install dependencies of Media-server.
