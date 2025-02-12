#!/bin/bash

# 获取本机时间，使用适当的格式
local_time=$(date +'%Y-%m-%d %H:%M:%S')
echo "Local time: $local_time"
# 设置设备的系统时间
adb shell "date -s '$local_time'"

# 将系统时间写入RTC
adb shell "hwclock -w"
