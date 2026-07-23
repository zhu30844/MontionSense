# MotionSense Web Interface - Quick Start Guide

Get up and running with the MotionSense web interface in minutes.

## 🚀 Quick Start

### 1. Access the Interface
Open your web browser and navigate to:
```
http://192.168.1.1
```

**Note:** Replace `192.168.1.1` with your MotionSense device's actual IP address.

### 2. Main Interface Overview

The web interface has three main sections:

#### 📹 Live Stream
- **Purpose**: View real-time video feed
- **Features**: 
  - Live MJPEG stream
  - Connection status indicator
  - Auto-reconnect on connection loss

#### 📅 Calendar & Heatmap
- **Purpose**: Browse recorded video by date
- **Features**:
  - Monthly calendar view
  - Motion intensity heatmap (red = more motion)
  - Click any date to view recordings

#### ▶️ Video Playback
- **Purpose**: Play recorded video segments
- **Features**:
  - HLS video player
  - Motion point markers on progress bar
  - Segment list with timestamps
  - Click markers to jump to motion events

## 🎯 First Steps

### Step 1: Check Live Stream
1. Click the **"Live Stream"** button in the navigation
2. Verify the video feed is working
3. Check the status indicator (green = connected)

### Step 2: Browse Recordings
1. Click the **"Calendar"** button
2. Look for dates with red coloring (indicates motion activity)
3. Click on a date with activity

### Step 3: Play Video with Motion Markers
1. In the playback view, select a video segment from the list
2. Watch for red markers on the progress bar
3. Click any red marker to jump to that motion event

## 🔧 Troubleshooting

### Common Issues

**❌ Can't access the web interface**
- Check device IP address
- Ensure device is powered on
- Verify network connectivity

**❌ Live stream not working**
- Check camera connection
- Verify device is recording
- Try refreshing the page

**❌ No motion markers on video**
- Ensure video has motion events
- Check browser console for errors
- Verify API endpoints are working

**❌ Calendar not loading**
- Check motion counts API
- Verify date format
- Clear browser cache

### Quick API Test
Open browser console (F12) and run:
```javascript
// Test all APIs
fetch('/api/motion_counts').then(r => r.json()).then(console.log);
fetch('/api/video_segments').then(r => r.json()).then(console.log);
fetch('/api/motion_points').then(r => r.json()).then(console.log);
```

## 📱 Mobile Usage

The interface is fully responsive and works on mobile devices:

- **Touch-friendly**: Large buttons and touch targets
- **Responsive layout**: Adapts to screen size
- **Mobile-optimized**: Works on phones and tablets

### Mobile Tips
- Use landscape orientation for better video viewing
- Tap motion markers to jump to events
- Swipe calendar to navigate months

## 🎨 Customization

### Language Toggle
Click the language button to switch between English and Chinese.

### Video Player Controls
- **Play/Pause**: Spacebar or click
- **Seek**: Click progress bar or motion markers
- **Fullscreen**: Double-click video or use button

### Calendar Navigation
- **Previous Month**: Click left arrow
- **Next Month**: Click right arrow
- **Today**: Click today's date

## 📊 Understanding the Data

### Motion Counts
- **Low (0-50)**: Light green
- **Medium (51-150)**: Orange
- **High (151+)**: Red

### Video Segments
- **Folder**: Segment identifier (00000, 00001, etc.)
- **Start Time**: When recording began
- **Length**: Duration in seconds
- **Motion Points**: Number of motion events

### Motion Markers
- **Red lines**: Motion detection events
- **Position**: Based on frame number and video duration
- **Clickable**: Jump directly to motion event

## 🔍 Advanced Features

### Debug Mode
Enable detailed logging in browser console:
```javascript
localStorage.setItem('debug', 'true');
location.reload();
```

### API Monitoring
Monitor network requests in browser DevTools:
1. Open DevTools (F12)
2. Go to Network tab
3. Interact with interface
4. Check API calls and responses

### Performance Tips
- Use wired connection for better video quality
- Close other browser tabs for smoother playback
- Clear browser cache if experiencing issues

## 📞 Support

### Getting Help
1. Check browser console for error messages
2. Verify device is running latest firmware
3. Test API endpoints manually
4. Check network connectivity

### Useful Commands
```bash
# Test device connectivity
ping 192.168.1.1

# Test web server
curl -I http://192.168.1.1

# Check device status
ssh root@192.168.1.1
```

---

**Need more help?** See the full documentation in `README.md` and `API_TESTING.md`. 