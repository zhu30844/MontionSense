ARCHBITS = 32  # 32/64 bits

ifdef PLATFORM
	CROSS:=${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-
else
	CROSS:=${SDK_PATH}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin/arm-rockchip830-linux-uclibcgnueabihf-
	OSID := arm
	OSVERSIONID := rockchip830
	PLATFORM:=${OSID}$(OSVERSIONID)linux$(ARCHBITS)
endif

ifeq ($(RELEASE),1)
	BUILD:=release
else
	BUILD:=debug
endif

all:
	$(MAKE) -C libdash
	$(MAKE) -C libflv
	$(MAKE) -C libhls
	$(MAKE) -C libmkv
	$(MAKE) -C libmov
	$(MAKE) -C libmpeg

	
clean:
	$(MAKE) -C libdash clean
	$(MAKE) -C libflv clean
	$(MAKE) -C libhls clean
	$(MAKE) -C libmkv clean
	$(MAKE) -C libmov clean
	$(MAKE) -C libmpeg clean

