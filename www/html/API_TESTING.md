# MotionSense API Testing Guide

This document provides comprehensive testing examples for all MotionSense web APIs.

## 🛠️ Testing Tools

### Browser DevTools
```javascript
// Test in browser console
fetch('/api/motion_counts')
  .then(response => response.json())
  .then(data => console.log(data))
  .catch(error => console.error('Error:', error));
```

### cURL Commands
```bash
# Test motion counts
curl -X GET "http://192.168.1.1/api/motion_counts"

# Test video segments for specific date
curl -X GET "http://192.168.1.1/api/video_segments?date=2025-07-26"

# Test motion points for specific date
curl -X GET "http://192.168.1.1/api/motion_points?date=2025-07-26"

# Test live stream (save to file)
curl -X GET "http://192.168.1.1/api/stream" -o stream.mjpg
```

### JavaScript Testing Script
```javascript
// Complete API test suite
class MotionSenseAPITester {
    constructor(baseURL = 'http://192.168.1.1') {
        this.baseURL = baseURL;
    }

    async testMotionCounts() {
        try {
            const response = await fetch(`${this.baseURL}/api/motion_counts`);
            const data = await response.json();
            console.log('✅ Motion Counts API:', data);
            return data;
        } catch (error) {
            console.error('❌ Motion Counts API Error:', error);
            throw error;
        }
    }

    async testVideoSegments(date = '2025-07-26') {
        try {
            const response = await fetch(`${this.baseURL}/api/video_segments?date=${date}`);
            const data = await response.json();
            console.log('✅ Video Segments API:', data);
            return data;
        } catch (error) {
            console.error('❌ Video Segments API Error:', error);
            throw error;
        }
    }

    async testMotionPoints(date = '2025-07-26') {
        try {
            const response = await fetch(`${this.baseURL}/api/motion_points?date=${date}`);
            const data = await response.json();
            console.log('✅ Motion Points API:', data);
            return data;
        } catch (error) {
            console.error('❌ Motion Points API Error:', error);
            throw error;
        }
    }

    async testLiveStream() {
        try {
            const response = await fetch(`${this.baseURL}/api/stream`);
            console.log('✅ Live Stream API Status:', response.status);
            return response.ok;
        } catch (error) {
            console.error('❌ Live Stream API Error:', error);
            throw error;
        }
    }

    async runAllTests() {
        console.log('🚀 Starting MotionSense API Tests...\n');
        
        try {
            await this.testMotionCounts();
            await this.testVideoSegments();
            await this.testMotionPoints();
            await this.testLiveStream();
            
            console.log('\n✅ All API tests completed successfully!');
        } catch (error) {
            console.error('\n❌ Some API tests failed:', error);
        }
    }
}

// Usage
const tester = new MotionSenseAPITester();
tester.runAllTests();
```

## 📊 Expected Response Examples

### 1. Motion Counts API Response
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
    },
    {
      "date": "2025-07-24",
      "count": 234
    }
  ]
}
```

### 2. Video Segments API Response
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
    },
    {
      "folder": "00002",
      "start_time": "10:35:35",
      "length": 95,
      "url": "/mnt/sdcard/DCIM/2025-07-26/00002/index.m3u8"
    }
  ]
}
```

### 3. Motion Points API Response
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
      "video_id": 1,
      "motion_time": 89,
      "folder": "00000"
    },
    {
      "video_id": 2,
      "motion_time": 12,
      "folder": "00001"
    },
    {
      "video_id": 2,
      "motion_time": 34,
      "folder": "00001"
    }
  ]
}
```

## 🔍 Response Validation

### Motion Counts Validation
```javascript
function validateMotionCounts(data) {
    // Check structure
    if (!data.motion_counts || !Array.isArray(data.motion_counts)) {
        throw new Error('Invalid motion_counts structure');
    }
    
    // Validate each entry
    data.motion_counts.forEach(entry => {
        if (!entry.date || !entry.count) {
            throw new Error('Invalid motion count entry');
        }
        
        // Validate date format
        if (!/^\d{4}-\d{2}-\d{2}$/.test(entry.date)) {
            throw new Error('Invalid date format');
        }
        
        // Validate count is number
        if (typeof entry.count !== 'number' || entry.count < 0) {
            throw new Error('Invalid count value');
        }
    });
    
    return true;
}
```

### Video Segments Validation
```javascript
function validateVideoSegments(data) {
    // Check structure
    if (!data.segments || !Array.isArray(data.segments)) {
        throw new Error('Invalid segments structure');
    }
    
    // Validate each segment
    data.segments.forEach(segment => {
        if (!segment.folder || !segment.start_time || !segment.length) {
            throw new Error('Invalid segment structure');
        }
        
        // Validate folder format
        if (!/^\d{5}$/.test(segment.folder)) {
            throw new Error('Invalid folder format');
        }
        
        // Validate time format
        if (!/^\d{2}:\d{2}:\d{2}$/.test(segment.start_time)) {
            throw new Error('Invalid time format');
        }
        
        // Validate length
        if (typeof segment.length !== 'number' || segment.length <= 0) {
            throw new Error('Invalid length value');
        }
    });
    
    return true;
}
```

### Motion Points Validation
```javascript
function validateMotionPoints(data) {
    // Check structure
    if (!data.motion_points || !Array.isArray(data.motion_points)) {
        throw new Error('Invalid motion_points structure');
    }
    
    // Validate each point
    data.motion_points.forEach(point => {
        if (!point.video_id || !point.motion_time || !point.folder) {
            throw new Error('Invalid motion point structure');
        }
        
        // Validate video_id
        if (typeof point.video_id !== 'number' || point.video_id < 0) {
            throw new Error('Invalid video_id');
        }
        
        // Validate motion_time
        if (typeof point.motion_time !== 'number' || point.motion_time < 0) {
            throw new Error('Invalid motion_time');
        }
        
        // Validate folder
        if (typeof point.folder !== 'string') {
            throw new Error('Invalid folder');
        }
    });
    
    return true;
}
```

## 🚨 Error Handling Examples

### Network Errors
```javascript
async function handleNetworkError(url) {
    try {
        const response = await fetch(url);
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        return await response.json();
    } catch (error) {
        if (error.name === 'TypeError') {
            console.error('Network error - check device connectivity');
        } else {
            console.error('API error:', error.message);
        }
        throw error;
    }
}
```

### Timeout Handling
```javascript
async function fetchWithTimeout(url, timeout = 5000) {
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), timeout);
    
    try {
        const response = await fetch(url, {
            signal: controller.signal
        });
        clearTimeout(timeoutId);
        return response;
    } catch (error) {
        clearTimeout(timeoutId);
        if (error.name === 'AbortError') {
            throw new Error('Request timeout');
        }
        throw error;
    }
}
```

## 📈 Performance Testing

### Load Testing Script
```javascript
async function loadTestAPI(endpoint, iterations = 100) {
    const startTime = Date.now();
    const results = [];
    
    for (let i = 0; i < iterations; i++) {
        const requestStart = Date.now();
        try {
            const response = await fetch(endpoint);
            const data = await response.json();
            const requestTime = Date.now() - requestStart;
            results.push({ success: true, time: requestTime });
        } catch (error) {
            const requestTime = Date.now() - requestStart;
            results.push({ success: false, time: requestTime, error: error.message });
        }
    }
    
    const totalTime = Date.now() - startTime;
    const successful = results.filter(r => r.success).length;
    const avgTime = results.reduce((sum, r) => sum + r.time, 0) / results.length;
    
    console.log(`Load Test Results for ${endpoint}:`);
    console.log(`Total requests: ${iterations}`);
    console.log(`Successful: ${successful}`);
    console.log(`Failed: ${iterations - successful}`);
    console.log(`Average response time: ${avgTime.toFixed(2)}ms`);
    console.log(`Total time: ${totalTime}ms`);
    
    return { results, totalTime, successful, avgTime };
}
```

## 🔧 Debugging Tips

### Enable Detailed Logging
```javascript
// Add to your test script
const DEBUG = true;

function log(message, data = null) {
    if (DEBUG) {
        console.log(`[DEBUG] ${message}`, data || '');
    }
}

// Usage
log('Fetching motion counts...');
const response = await fetch('/api/motion_counts');
log('Response received:', response.status);
```

### Monitor Network Activity
```javascript
// Intercept fetch calls for debugging
const originalFetch = window.fetch;
window.fetch = function(...args) {
    console.log('🌐 Fetch request:', args[0]);
    return originalFetch.apply(this, args)
        .then(response => {
            console.log('📥 Fetch response:', response.status, response.url);
            return response;
        });
};
```

---

**Note:** Replace `192.168.1.1` with your actual MotionSense device IP address when testing. 