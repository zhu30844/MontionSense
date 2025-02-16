# IPC WebServer

## Connection 
1. Client and IPC connect directly
2. Cannot access Internet

## Development progress
1. SQLite avaliable 
2. HLS Video recorder done
3. Front end calendar done
4. Heat map done
5. Playback done
6. Video progress bar TODO

## Hardware resources 
1. SOC: RV1106, single core ARMv7, 1.2GHz
2. RAM: 256MB, 150MB avaliable for webserver
3. Storage: 256MB Nand Flash, 150MB avaliable

## Requirements and Solution
1. View real time stream
    + mjpg stream
2. Choose the date and review the record
    + Check the segments of videos(for the segment of power shortages,every time the IPC start, it will create a new folder to write new m3u8 file in the folder of date. eg. 2025-01-24/00001)
    + Get the start time of each segments
    + Get the movement times of video
    + Display the movements time via progress bar

## What we need?
1. Front end: 
    + A calendar 
        - User can view the canlendar and click the date to review the record
    + A video player (video.js) that can 
        - Play HLS video 
        - Get the time straps
        - Customization progress bar

2. Back end:
    + A database (sqlite)
        - Record/return the M3U8 file information(including the video is avaliable or not, the path, how many m3u8 files per day )
        - Return the motion time stramp of the video
    + A mjpg streamer
    + Provide video files 
3. A portable WebServer
    + Mongoose

4. Components Organization 
    + Communication and interfaces 
        - RESTful


## More things to consider
1. Safety
    + Do not allow user download DB files
