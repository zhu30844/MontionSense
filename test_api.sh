#!/bin/bash

echo "Testing MotionSense APIs..."

# Test motion counts API
echo "1. Testing motion counts API:"
curl -s "http://192.168.1.1/api/motion_counts" | head -10

echo -e "\n2. Testing video segments API:"
curl -s "http://192.168.1.1/api/video_segments?date=2025-07-26" | head -10

echo -e "\n3. Testing motion points API:"
curl -s "http://192.168.1.1/api/motion_points?date=2025-07-26" | head -10

echo -e "\n4. Testing motion points API (today):"
curl -s "http://192.168.1.1/api/motion_points" | head -10

echo -e "\nAPI tests completed." 