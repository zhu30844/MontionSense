#!/bin/bash
RED='\033[0;31m'
GREEN='\e[32;1m'
NC='\033[0m' # No Color
#switch to the script's directory
ROOT_PWD=$(cd "$(dirname $0)" && pwd)
# Function to handle signals function 
handle_signal() { 
	echo -e "\n${RED}Signal caught, rebooting device...${NC}" 
	adb shell reboot 
	exit 
} 
# Trap signals 
trap handle_signal SIGINT
#clean build folder
if [ "$1" = "clean" ]; then
	if [ -d "${ROOT_PWD}/build" ]; then
		rm -rf "${ROOT_PWD}/build"
		echo -e "${GREEN}${ROOT_PWD}/build has been deleted!${NC}"
	fi

	if [ -d "${ROOT_PWD}/install" ]; then
		rm -rf "${ROOT_PWD}/install"
		echo -e "${GREEN}${ROOT_PWD}/install has been deleted!${NC}"
	fi
	echo -e "${GREEN}clean done!${NC}"
	exit
fi

#clean build folder
if [ -d ${ROOT_PWD}/build ]; then
	rm -rf ${ROOT_PWD}/build
fi
mkdir ${ROOT_PWD}/build
cd ${ROOT_PWD}/build
cmake .. 
make install
if [ $? -eq 0 ]; then 
	echo -e "${GREEN}Make executed successfully${NC}"
	echo "Pushing the install folder to the RV1106 board" 
	#copy the install folder to the RV1106 board
	#adb shell ./oem/usr/bin/RkLunch-stop.sh
	adb shell rm -rf /mnt/sdcard/MyMD_demo
	adb push ${ROOT_PWD}/install/MyMD_demo /mnt/sdcard/
	adb shell chmod +x /mnt/sdcard/MyMD_demo/MyMD
	echo "executing the program on the RV1106 board"
	adb shell ./mnt/sdcard/MyMD_demo/MyMD
else 
	echo -e "${RED}Make failed to execute${NC}"
fi
