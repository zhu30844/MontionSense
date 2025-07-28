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
    + **video_id**: Segment number of motion event (references VideoSegments.id).
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
    + **Purpose**: Highlight video player bar with motion events
    + **Create**: when switching/opening folder 
    + **Update trigger**: every time frame rate changes
    + **Read by**: Web server via JOIN query with VideoSegments
    + **Release**: Switching folder
    + **Delete**: Delete the whole db file

2. Table: VideoSegments
    + **folder**: folder name of segments (e.g., "00000", "00001")
    + **start_time**: start time of each segment
    + **length**: total length of one segment 
    + Example Table 
      |id       |folder         |start_time          |length      |
      | :------ | :-----------: | :----------------: | ---------: |
      | 1       | 00000         | 00:00:15.272347    | 16260      |
      | 2       | 00001         | 00:50:15.562444    | 10086      |
      | 3       | 00002         | 01:00:15.565666    | 10010      |
      | ...     | ...           | ...                | ...        |

    + **Purpose**: Provides total length of one segment, helping locate the motion mark in player's bar
    + **Create trigger**: Switching folder 
    + **Update trigger**: Every 10 frames
    + **Read by**: Web server, to show the interrupt times for users

## API Data Flow

### Motion Points API (`/api/motion_points`)
The API performs a JOIN query between EventDetails and VideoSegments tables:

```sql
SELECT json_group_array(json_object(
    'video_id', ed.video_id, 
    'motion_time', ed.motion_time, 
    'folder', COALESCE(vs.folder, 'unknown')
)) 
FROM EventDetails ed 
LEFT JOIN VideoSegments vs ON ed.video_id = vs.id;
```

**Response Format:**
```json
{
  "motion_points": [
    {
      "video_id": 1,
      "motion_time": 45,
      "folder": "00000"
    },
    {
      "video_id": 1,
      "motion_time": 67,
      "folder": "00000"
    },
    {
      "video_id": 2,
      "motion_time": 12,
      "folder": "00001"
    }
  ]
}
```

### Key Changes in v2.0
1. **JOIN Query**: EventDetails now JOINs with VideoSegments to get folder information
2. **Folder Mapping**: Frontend uses `folder` field instead of `video_id` for segment matching
3. **Fallback Handling**: Uses `COALESCE` to handle missing folder data with "unknown"
4. **Data Validation**: Added checks for empty EventDetails table before querying

### Database Relationships
```
EventDetails.video_id → VideoSegments.id
EventDetails.motion_time → Frame number (30fps)
VideoSegments.folder → Physical folder name (00000, 00001, etc.)
```

### Performance Considerations
- **Indexing**: video_id should be indexed for JOIN performance
- **Memory Management**: Queries include proper mutex locking
- **Error Handling**: Graceful fallback for missing data
- **Connection Management**: Separate connections for different date databases

