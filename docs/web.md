# Webserver System Design

## Functional requirements
1. Display the live stream.
2. Display heatmap of motion in calendar page(To be optimized).
3. Display video segments and start time. 
4. Video playback.
5. Highligt event spot in video progress bar (TODO).

## API Endpoints
The server offers several APIs for retrieving data:

1. Streamer: 

    Streamer API

    Endpoint: /api/stream

    Method: GET

    Functionality: Streams live JPEG images.

    Usage:
    + Clients connect to this endpoint to receive a live MJPEG feed.
    + For embedding live video streams in web pages via <img> tags.

2. Motion Counts: Fetches motion detection counts.
    Motion Counts API
    Endpoint: /api/motion_counts

    Method: GET

    Functionality: Returns motion detection counts in JSON format.

    Response Structure:
    ```json
    { 
        "motion_counts": [
        {"date":"2025-02-11","motion_count":13255},
        {"date":"2025-02-12","motion_count":1276},
        {"date":"2025-02-13","motion_count":126},
        {"date":"2025-02-14","motion_count":78},
        {"date":"2025-02-15","motion_count":16602}] 
    }
    ```
3. Video Segments: Retrieves lists of recorded video segments for a given date.

    Video Segments API

    Endpoint: /api/video_segments

    Method: GET

    Query Parameter:

    date: Required. Specifies the date for which to retrieve video segments (format: YYYY-MM-DD).

    Functionality: Provides a list of video segments recorded on a specific date.

    Response Structure:

    ```json
    { 
        "video_segments": [
        {"folder":"00000","start_time":"12:41:02.387656","length":810},
        {"folder":"00001","start_time":"12:47:32.371263","length":493050}] 
    }
    ```

4. Motion Points: Intended to provide detailed motion spots in frame number (TODO).
    Motion Points API

    Endpoint: /api/motion_points

    Method: GET

    Functionality: Intended to return detailed motion spots in frame number (TODO).

    Current Response: 
    ```
        HTTP/1.1 200 OK
        Content-Type: application/jsonContent-Length: 21  
    ```
## Static Webserver
1. Create softlink `hls` in www/html folder. This can be found in the storage module when the system attempts to create the DCIM folder.