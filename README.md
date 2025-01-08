# Luckfox Pico MD (Motion Detection) example

+ This example is based on RKMPI_IVS 
+ This example uses RKMPI_IVS for Motion Detection.
+ Example code for video capture and streaming specifically developed for Luckfox Pico series development boards.


## Project Update Log
+ ver 0.1.2
1. MD with multi VI chns

+ ver 0.1.1
1. MD with multithreading  

+ Ver 0.1
1. a demo of calling RKMPI_IVS lib


## Implementation Results

show the result of motion detection.

## System Framework 

TBD

## Video Framework
VI 0 => bind VENC 0 and osd draw time => muxer 1080*1920

VI 1 => bind VENC 1 => rtsp 1080*1920

VI 2 => bind IVS for OD MD 1080*1920