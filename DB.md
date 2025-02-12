# Database Design

```
                   _ooOoo_
                  o8888888o
                  88" . "88
                  (| -_- |)
                  O\  =  /O
               ____/`---'\____
             .'  \\|     |//  `.
            /  \\|||  :  |||//  \
           /  _||||| -:- |||||-  \
           |   | \\\  -  /// |   |
           | \_|  ''\---/''  |   |
           \  .-\__  `-`  ___/-. /
         ___`. .'  /--.--\  `. . __
      ."" '<  `.___\_<|>_/___.'  >'"".
     | | :  `- \`.;`\ _ /`;.`/ - ` : | |
     \  \ `-.   \_ __\ /__ _/   .-` /  /
======`-.____`-.___\_____/___.-`____.-'======
                   `=---='
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
```






## Why we need database?
    1. File management
    2. Heatmap

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
1. Dates when videos were recorded.
2. Number of times motion was detected each day.

## EventLogs.db
1. Power events.
2. Timestamps/frame nummber of motion detection.



