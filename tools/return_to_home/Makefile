# SPDX-License-Identifier: GPL-2.0-only
include $(DEVKITPRO)/libnx/switch_rules

TARGET := return-to-home
ARCH := -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE

all: $(TARGET).nro

$(TARGET).elf: main.c $(LIBNX)/lib/libnx.a
	$(CC) $(ARCH) -O2 -Wall -Wextra -Werror -D__SWITCH__ -ffunction-sections -fdata-sections -I$(LIBNX)/include -specs=$(LIBNX)/switch.specs -Wl,--gc-sections -L$(LIBNX)/lib $< -lnx -o $@

# An SDK archive is an input, not a target for switch_rules' generic %.a rule.
$(LIBNX)/lib/libnx.a:
	@test -f "$@"

$(TARGET).nacp:
	nacptool --create "Return to HOME" "Eiffel2018" "0.1.0" $@

$(TARGET).nro: $(TARGET).elf $(TARGET).nacp
	elf2nro $< $@ --nacp=$(TARGET).nacp

.PHONY: all
