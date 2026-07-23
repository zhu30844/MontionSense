# MotionSense Web Interface

A modern, responsive web interface for the MotionSense surveillance system running on Luckfox Pico RV1106 platform.

## 🚀 Features

- **Live Video Stream**: Real-time MJPEG streaming with status indicators
- **Motion Heatmap**: Interactive calendar with motion intensity visualization
- **Video Playback**: HLS video segments with motion point markers
- **Responsive Design**: Modern UI that works on desktop and mobile devices
- **Single Page Application**: Smooth navigation without page reloads

## 📁 Project Structure

```
www/html/
├── index.html              # Main application entry point
├── css/
│   ├── styles.css          # Main application styles
│   ├── calendar.css        # Calendar and heatmap styles
│   └── playback.css        # Video player styles
├── js/
│   ├── app.js             # Main application controller
│   ├── stream.js          # Live stream management
│   ├── calendar.js        # Calendar and heatmap logic
│   └── playback.js        # Video playback and motion markers
└── components/            # Reusable UI components
```

## 🏗️ Architecture

The web interface is built as a **Single Page Application (SPA)** using vanilla JavaScript ES6 classes:

- **Modular Design**: Each feature is encapsulated in its own class
- **Event-Driven**: Components communicate through events
- **Responsive**: CSS Grid and Flexbox for adaptive layouts
- **Progressive Enhancement**: Works without JavaScript for basic functionality

### Core Classes

- `MotionSenseApp`: Main application controller
- `StreamApp`: Live video stream management
- `CalendarApp`: Calendar and motion heatmap
- `PlaybackApp`: Video playback with motion markers

## 🌐 API Reference

### Base URL
```
http://192.168.1.1
```

### 1. Motion Counts API

**Endpoint:** `GET /api/motion_counts`

**Description:** Retrieves daily motion counts for the heatmap calendar.

**Response Format:**
```json
{
  "motion_counts": [
    {
      "date": "2025-07-26",
      "count": 156
    },
    {
      "date": "2025-07-25", 
      "count": 89
    }
  ]
}
```

**Example Usage:**
```javascript
const response = await fetch('/api/motion_counts');
const data = await response.json();
console.log(data.motion_counts);
```

### 2. Video Segments API

**Endpoint:** `GET /api/video_segments?date={date}`

**Description:** Retrieves video segments for a specific date.

**Parameters:**
- `date` (string): Date in YYYY-MM-DD format (optional, defaults to today)

**Response Format:**
```json
{
  "segments": [
    {
      "folder": "00000",
      "start_time": "10:30:15",
      "length": 120,
      "url": "/mnt/sdcard/DCIM/2025-07-26/00000/index.m3u8"
    },
    {
      "folder": "00001", 
      "start_time": "10:32:35",
      "length": 180,
      "url": "/mnt/sdcard/DCIM/2025-07-26/00001/index.m3u8"
    }
  ]
}
```

**Example Usage:**
```javascript
const response = await fetch('/api/video_segments?date=2025-07-26');
const data = await response.json();
console.log(data.segments);
```

### 3. Motion Points API

**Endpoint:** `GET /api/motion_points?date={date}`

**Description:** Retrieves motion detection points for video segments.

**Parameters:**
- `date` (string): Date in YYYY-MM-DD format (optional, defaults to today)

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

**Example Usage:**
```javascript
const response = await fetch('/api/motion_points?date=2025-07-26');
const data = await response.json();
console.log(data.motion_points);
```

### 4. Live Stream API

**Endpoint:** `GET /api/stream`

**Description:** Provides MJPEG live video stream.

**Response:** MJPEG video stream

**Example Usage:**
```html
<img src="/api/stream" alt="Live Stream" />
```

## 🎨 UI Components

### Navigation
The main navigation provides three views:
- **Live Stream**: Real-time video feed
- **Calendar**: Motion heatmap and date selection  
- **Playback**: Video segments with motion markers

### Calendar Heatmap
- **Color Intensity**: Red intensity indicates motion frequency
- **Interactive**: Click dates to view video segments
- **Responsive**: Adapts to different screen sizes

### Video Player
- **HLS Support**: Uses Video.js for HLS playback
- **Motion Markers**: Red lines on progress bar indicate motion events
- **Click Navigation**: Click markers to jump to motion events
- **Segment Table**: List of available video segments

## 🔧 Development

### Prerequisites
- Modern web browser with ES6 support
- Network access to the MotionSense device

### Local Development
1. Clone the repository
2. Serve the `www/html` directory with a web server
3. Access via `http://localhost:port`

### Browser Compatibility
- Chrome 60+
- Firefox 55+
- Safari 12+
- Edge 79+

## 📱 Responsive Design

The interface uses CSS Grid and Flexbox for responsive layouts:

```css
/* Mobile-first approach */
.container {
  display: grid;
  grid-template-columns: 1fr;
  gap: 1rem;
}

/* Tablet and desktop */
@media (min-width: 768px) {
  .container {
    grid-template-columns: 1fr 1fr;
  }
}
```

## 🎯 Motion Marker System

### How It Works
1. **Data Retrieval**: Fetches motion points from `/api/motion_points`
2. **Time Calculation**: Converts frame numbers to video time (30fps)
3. **Position Mapping**: Maps time to progress bar position
4. **Visual Rendering**: Creates clickable markers on the progress bar

### Marker Properties
- **Width**: 2px (4px on hover)
- **Color**: Red (#ff4444)
- **Position**: Calculated from motion_time and video duration
- **Interaction**: Click to seek to motion event

## 🔍 Debugging

### Console Logging
The application includes comprehensive logging:

```javascript
// Enable debug logging
console.log('Motion points loaded:', motionPoints);
console.log('Video segments:', segments);
console.log('Stream status:', streamStatus);
```

### Network Monitoring
Monitor API calls in browser DevTools:
- Motion counts: `/api/motion_counts`
- Video segments: `/api/video_segments`
- Motion points: `/api/motion_points`
- Live stream: `/api/stream`

## 🚀 Performance Optimization

### Loading Strategy
- **Lazy Loading**: Components load on demand
- **Caching**: API responses cached where appropriate
- **Debouncing**: Input events debounced for performance

### Memory Management
- **Event Cleanup**: Event listeners properly removed
- **DOM Cleanup**: Markers cleared before recreation
- **Resource Management**: Video player resources released

## 🔒 Security Considerations

- **CORS**: Configured for local network access
- **Input Validation**: API parameters validated
- **Error Handling**: Graceful degradation on API failures

## 📊 Data Flow

```
User Interaction → Event Handler → API Call → Data Processing → UI Update
     ↓
Calendar Click → Load Segments → Fetch Motion Points → Create Markers
     ↓
Video Play → Update Progress → Show Motion Indicators
```

## 🛠️ Troubleshooting

### Common Issues

**Motion markers not showing:**
- Check API response format
- Verify video duration is available
- Ensure motion points match current segment

**Video not playing:**
- Verify HLS stream is accessible
- Check network connectivity
- Ensure Video.js is loaded

**Calendar not loading:**
- Check motion counts API
- Verify date format
- Check browser console for errors

### Error Handling
```javascript
try {
  const response = await fetch('/api/motion_points');
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  const data = await response.json();
} catch (error) {
  console.error('API Error:', error);
  // Handle gracefully
}
```

## 📈 Future Enhancements

- **Real-time Updates**: WebSocket for live motion alerts
- **Advanced Filtering**: Motion intensity filtering
- **Export Features**: Video segment export
- **Mobile App**: Native mobile application
- **Cloud Integration**: Remote monitoring capabilities

---

**Version:** 2.0.0  
**Last Updated:** 2025-07-26  
**Compatibility:** MotionSense v1.3.0+
