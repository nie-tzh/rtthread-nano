ELF_FILE := $(BUILD_DIR)/$(TARGET).elf
BIN_FILE := $(BUILD_DIR)/$(TARGET).bin
MAP_FILE := $(BUILD_DIR)/$(TARGET).map
LST_FILE := $(BUILD_DIR)/$(TARGET).lst

OBJECTS := $(addprefix $(BUILD_DIR)/obj/,$(SRCS:.c=.o))
OBJECTS := $(OBJECTS:.S=.o)
OBJECTS := $(OBJECTS:.s=.o)
DEPS    := $(OBJECTS:.o=.d)
BUILD_CONFIG := $(BUILD_DIR)/.build-config
BUILD_INPUTS := Makefile mk/toolchain.mk mk/rules.mk \
                $(BOARD_MK) $(SOC_MK) $(CPU_MK) $(APP_MK) $(DRIVER_MKS) \
                $(RTTHREAD_MK) $(RTTHREAD_CONFIG)

ifeq ($(V),1)
Q :=
else
Q := @
endif

.PHONY: all clean info size help FORCE

all: $(ELF_FILE) $(BIN_FILE) $(LST_FILE) size

FORCE:

$(BUILD_CONFIG): FORCE
	@mkdir -p $(@D)
	@{ \
		printf '%s\n' "CC=$(CC)"; \
		printf '%s\n' "CPPFLAGS=$(CPPFLAGS)"; \
		printf '%s\n' "CFLAGS=$(CFLAGS)"; \
		printf '%s\n' "AS=$(AS)"; \
		printf '%s\n' "ASFLAGS=$(ASFLAGS)"; \
		printf '%s\n' "LDFLAGS=$(LDFLAGS)"; \
		printf '%s\n' "LDLIBS=$(LDLIBS)"; \
		printf '%s\n' "SRCS=$(SRCS)"; \
	} > $@.tmp
	@if ! cmp -s $@.tmp $@; then \
		mv $@.tmp $@; \
	else \
		rm -f $@.tmp; \
	fi

$(OBJECTS): $(BUILD_CONFIG) $(BUILD_INPUTS)

$(BUILD_DIR)/obj/%.o: %.c
	@printf "  %-8s %s\n" "CC" "$<"
	@mkdir -p $(@D)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/obj/%.o: %.S
	@printf "  %-8s %s\n" "AS" "$<"
	@mkdir -p $(@D)
	$(Q)$(AS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/obj/%.o: %.s
	@printf "  %-8s %s\n" "AS" "$<"
	@mkdir -p $(@D)
	$(Q)$(AS) $(ASFLAGS) -c $< -o $@

$(ELF_FILE): $(OBJECTS) $(LINKER_SCRIPT) $(BUILD_CONFIG) $(BUILD_INPUTS)
	@printf "  %-8s %s\n" "LD" "$@"
	@mkdir -p $(@D)
	$(Q)$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(BIN_FILE): $(ELF_FILE)
	@printf "  %-8s %s\n" "OBJCOPY" "$@"
	$(Q)$(OBJCOPY) -O binary $< $@

$(LST_FILE): $(ELF_FILE)
	@printf "  %-8s %s\n" "OBJDUMP" "$@"
	$(Q)$(OBJDUMP) -d -S $< > $@

size: $(ELF_FILE)
	$(Q)$(SIZE) $<

info:
	@printf "APP=%s\n" "$(APP)"
	@printf "BOARD=%s\n" "$(BOARD)"
	@printf "SOC=%s\n" "$(SOC)"
	@printf "CHIP=%s\n" "$(CHIP)"
	@printf "CPU=%s\n" "$(CPU)"
	@printf "BUILD=%s\n" "$(BUILD)"
	@printf "BUILD_DIR=%s\n" "$(BUILD_DIR)"
	@printf "CC=%s\n" "$(CC)"
	@printf "CPU_FLAGS=%s\n" "$(CPU_FLAGS)"
	@printf "LINKER_SCRIPT=%s\n" "$(LINKER_SCRIPT)"

clean:
	@printf "  %-8s %s\n" "CLEAN" "$(BUILD_DIR)"
	$(Q)rm -rf $(BUILD_DIR)

help:
	@printf "make [APP=demo] [BOARD=t22-deserializer-evb] [BUILD=debug|release] [O=build-dir] [-j]\n"
	@printf "E902 exception test: make APP=e902-exception-test BOARD=t22-deserializer-evb BUILD=debug O=build/t22-deserializer-evb/e902-exception-test/debug\n"
	@printf "E902 CLIC test: make APP=e902-clic-test BOARD=t22-deserializer-evb BUILD=debug O=build/t22-deserializer-evb/e902-clic-test/debug\n"
	@printf "E902 DW Timer test: make APP=e902-timer-test BOARD=t22-deserializer-evb BUILD=debug O=build/t22-deserializer-evb/e902-timer-test/debug\n"
	@printf "E902 context switch test: make APP=e902-context-switch-test BOARD=t22-deserializer-evb BUILD=debug O=build/t22-deserializer-evb/e902-context-switch-test/debug\n"

-include $(DEPS)
