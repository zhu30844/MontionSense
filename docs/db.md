# Database Design

## Why we need database?
    1. File management
    2. Heatmap 
    3. Motion details tracking

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

## VideoMetadata.db
1. Table: VideoMetadata
    + **date**: Date when videos were recorded.
    + **motion_count**: Number of times motion was detected each day.
    + Example Table
      |id       |date           |motion_count   |
      | :------ | :-----------: | :-----------: |
      | 1       | 2025-01-01    | 451           |
      | 2       | 2025-02-02    | 10086         |
      | ...     | ...           | ...           |
      | 10      | 2025-02-12    | 10010         |
      | ...     | ...           | ...           |

    + Create & update: Record frame changes
    + Read by: Web server, send the whole table to browser in JSON format
    + Delete : Cleaning disk, delete one term **sort by date, NOT by id** 
    + Keeps being connected since VideoMetadata.db is loaded
    + Released when system call deinit


## EventLogs.db
1. Table: EventDetails
    + **video_id**: Segment number of motion event.
    + **motion_time**: Frame number when video frame rate is changing.
    + Example Table 
      |id       |video_id       |motion_time    |
      | :------ | :-----------: | :-----------: |
      | 1       | 0             | 1             |
      | 2       | 0             | 2             |
      | 3       | 0             | 30            |
      | ...     | ...           | ...           |
      | 45      | 0             | 2030          |
      | 46      | 1             | 1             |
      | ...     | ...           | ...           |
    + High light video player bar
    + Create: when switching/opening folder 
    + Update trigger: every time frame rate changes
    + Read by: Web server **TODO**  
    + Release: Switching folder
    + Delete : Delete the whole db file

2. Table: VideoSegments
    + **folder**: folder name of segments
    + **start_time**: start time of each segment
    + **length**: total length of one segment 
    + Example Table 
      |id       |folder         |start_time          |length      |
      | :------ | :-----------: | :----------------: | ---------: |
      | 1       | 00000         | 00:00:15.272347    | 16260      |
      | 2       | 00001         | 00:50:15.562444    | 10086      |
      | 3       | 00002         | 01:00:15.565666    | 10010      |
      | ...     | ...           | ...                | ...        |

    + Provides total length of one segment, helping locate the motion mark in player's bar
    + Create trigger: Switching folder 
    + Update trigger: Every 10 frames
    + Read by: Web server, to show the interrupt times for users

