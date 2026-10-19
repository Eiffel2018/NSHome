# SPDX-License-Identifier: GPL-2.0-only
# Uses the pinned Atmosphere-libs build configuration.
ATMOSPHERE_LIBRARIES_DIR := $(abspath $(AMS_LIBRARIES))
ATMOSPHERE_BOARD := nx-hac-001
ATMOSPHERE_CPU := arm-cortex-a57
ATMOSPHERE_BUILD_NAME := release
include $(ATMOSPHERE_LIBRARIES_DIR)/config/templates/stratosphere.mk

TARGET := build/am-observer
SOURCES := source/main.cpp source/runtime.cpp source/forward.cpp source/home_bridge.cpp source/launch_bridge.cpp source/launch_ecs.cpp source/log.cpp source/remote.cpp source/boot.cpp source/boot_screen.cpp source/boot_jpeg.cpp $(wildcard source/remote_nro_*.cpp)
OBJECTS := $(SOURCES:source/%.cpp=build/%.o)
INCLUDE := -Isource -I$(ATMOSPHERE_LIBRARIES_DIR)/libstratosphere/include -I$(ATMOSPHERE_LIBRARIES_DIR)/libvapours/include -I$(DEVKITPRO)/libnx/include
LIBPATHS := -L$(ATMOSPHERE_LIBRARIES_DIR)/libstratosphere/lib/nintendo_nx_arm64_armv8a/release -L$(DEVKITPRO)/libnx/lib
FORWARD_ABI := _ZNK3ams2sf4hipc13ServerSession14ForwardRequestERKNS0_4cmif22ServiceDispatchContextE
CXXFLAGS += -fno-lto
OBJDUMP := aarch64-none-elf-objdump
LDFLAGS += -Wl,--wrap,$(FORWARD_ABI)

.PHONY: all
all: build/exefs.nsp
build/%.o: source/%.cpp source/trace.hpp source/codec.hpp Makefile
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
$(TARGET).elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBPATHS) $(LIBS) -o $@
	$(NM) $@ > build/symbols.txt
	$(OBJDUMP) -d $@ > build/disassembly.txt
build/main: $(TARGET).elf
	elf2nso $< $@
build/main.npdm: res/sysmodule.json
	@mkdir -p build
	npdmtool res/sysmodule.json build/main.npdm
build/exefs.nsp: build/main build/main.npdm Makefile
	@mkdir -p build/exefs
	cp build/main build/main.npdm build/exefs/
	rm -f build/exefs/mitm.lst
	build_pfs0 build/exefs build/exefs.nsp

-include $(OBJECTS:.o=.d)
