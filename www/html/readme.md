# IPC WebServer

## Connection 
1. Client and IPC connect directly
2. Cannot access Internet

## Development progress
1. SQLite avaliable 
2. HLS Video recorder done
3. Front end calendar done

## Hardware resources 
1. SOC: RV1106, single core ARMv7, 1.2GHz
2. RAM: 256MB, 150MB avaliable for webserver
3. Storage: 256MB Nand Flash, 150MB avaliable


## General requirements
1. View real time stream
2. Choose the date and review the record
3. Synchronize time from user's browser

## Requirement and Solution
1. View real time stream
    + RTMP + NGINX 
    + Possible functions: to save system recourses, auto start or turn off rtmp
2. Choose the date and review the record
    + Check the segments of videos(for the segment of power shortages,every time the IPC start, it will create a new folder to write new m3u8 file in the folder of date. eg. 2025-01-24/00001)
    + Get the start time of each segments
    + Get the movement times of video
    + Display the movements time via progress bar

## What we need?
1. Front end: 
    + A calendar 
        - User can view the canlendar and click the date to review the record
    + A video player that can 
        - Play HLS video 
        - Get the time straps
        - Customization progress bar
        - Display one or more m3u8 source within one video window

2. Back end:
    + A database 
        - Record/return the M3U8 file information(including the video is avaliable or not, the path, how many m3u8 files per day )
        - Return the motion time stramp of the video
    + A rtmp server
    + Provide video files 
3. A high efficent WebServer engine
    + Nginx

4. Components Organization 
    + Communication and interfaces 
        - How to design a back-end server by FastCGI 
        - The library/protocol to communicate 
        - One typical usage: To synchronize IPC time 
    + The ports of different components


## More things to consider
1. Safety
    + Do not allow user access the orignal ports without nginx

## Tech stacks 
1. video.js + hls.js
2. Progress Bar: Canvas
3. Delete old video: find /video -type d -mtime +7 -exec rm -rf {} \;
4. 开发FastCGI核心接口