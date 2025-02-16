#!/bin/bash

local_time=$(date +'%Y-%m-%d %H:%M:%S')
echo "Local time: $local_time"
adb shell "date -s '$local_time'"
adb shell "hwclock -w"
