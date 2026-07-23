#!/bin/bash

adb shell "tail -f /var/log/messages | grep MotionSense"