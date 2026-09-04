# MotionSense — Luckfox SDK app integration.
#
# project/app/Makefile discovers this via $(wildcard ./*/Makefile), so no
# vendor file has to be edited to add this app to the SDK build.
#
# Makefile.param supplies the cross toolchain, the media library paths and
# RK_APP_OUTPUT, whose bin/ lib/ share/ subdirs build.sh then copies into the
# oem partition (see __PACKAGE_OEM in project/build.sh).
#
# This wrapper only drives the existing CMake build; the standalone
# `cmake -S . -B build` flow keeps working unchanged.

# Evaluate before the include: Makefile.param pulls in .BoardConfig.mk, which
# would otherwise be what $(lastword $(MAKEFILE_LIST)) points at.
CURRENT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

# Makefile.param's copy/strip macros use bash syntax ([[ ]]), and GNU make does
# not pass SHELL down to sub-makes, so it has to be set here too.
SHELL := /bin/bash

include ../Makefile.param
MS_BUILD_DIR := $(CURRENT_DIR)/build-sdk
MS_STAGE_DIR := $(CURRENT_DIR)/install-sdk

# Where the SDK's freshly built Rockchip media libraries and headers live.
MS_MEDIA_DIR := $(RK_APP_PATH_LIB_INCLUDE)

all:
	@echo -e "$(C_GREEN) [MotionSense] configure $(C_NORMAL)"
	cmake -S $(CURRENT_DIR) -B $(MS_BUILD_DIR) \
		-DCMAKE_C_COMPILER=$(RK_APP_CROSS)-gcc \
		-DCMAKE_CXX_COMPILER=$(RK_APP_CROSS)-g++ \
		-DCMAKE_INSTALL_PREFIX=$(MS_STAGE_DIR) \
		-DMS_RK_MEDIA_DIR=$(MS_MEDIA_DIR)
	@echo -e "$(C_GREEN) [MotionSense] build $(C_NORMAL)"
	cmake --build $(MS_BUILD_DIR) -j$(RK_APP_JOBS)
	cmake --install $(MS_BUILD_DIR)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT)/bin, $(MS_STAGE_DIR)/MotionSense)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT)/share/MotionSense/fonts, $(MS_STAGE_DIR)/fonts)
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT)/share/MotionSense, $(MS_STAGE_DIR)/config.yaml)
	$(call MAROC_STRIP_DEBUG_SYMBOL, $(RK_APP_OUTPUT)/bin)

clean:
	rm -rf $(MS_BUILD_DIR) $(MS_STAGE_DIR)

distclean: clean

info:
	@echo "MotionSense: cross=$(RK_APP_CROSS) media=$(MS_MEDIA_DIR) out=$(RK_APP_OUTPUT)"

.PHONY: all clean distclean info
