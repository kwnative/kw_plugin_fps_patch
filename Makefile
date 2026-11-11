# SPDX-FileCopyrightText: 2026 N.Leiten
# SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
REPO_ROOT := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
KWMOD_SDK_ROOT ?= $(abspath $(REPO_ROOT)/../kw_modloader)
include $(KWMOD_SDK_ROOT)/mk/plugin.mk
.DEFAULT_GOAL := all

# Bundle builds discover optional provider headers; explicit values still win.
# Standalone builds require only the loader SDK.
PACING_SDK_ROOT ?= $(abspath $(REPO_ROOT)/../kw_plugin_pacing)
PACING_INTEGRATION_HEADER := $(PACING_SDK_ROOT)/include/kwpacing/integration/pacing_policy.h
WITH_PACING_INTEGRATION ?= $(call kwmod_bundle_integration_default,$(PACING_INTEGRATION_HEADER),kw_plugin_pacing)
ifneq ($(WITH_PACING_INTEGRATION),0)
ifneq ($(WITH_PACING_INTEGRATION),1)
$(error WITH_PACING_INTEGRATION must be 0 or 1)
endif
endif
CPPFLAGS += -DWITH_PACING_INTEGRATION=$(WITH_PACING_INTEGRATION)
ifeq ($(WITH_PACING_INTEGRATION),1)
ifeq ($(wildcard $(PACING_INTEGRATION_HEADER)),)
$(error Set PACING_SDK_ROOT to the optional provider repository)
endif
CPPFLAGS += -I"$(PACING_SDK_ROOT)/include"
endif
INTEGRATION_SETTINGS := PACING=$(WITH_PACING_INTEGRATION):$(PACING_SDK_ROOT)

CFLAGS := $(filter-out -Wpedantic,$(CFLAGS))
FPS_SOURCES := \
    src/adapter.c \
    src/memory_patch.c \
    src/runtime/config.c \
    src/runtime/game_patches.c \
    src/runtime/log.c
FPS_OBJECTS := $(call objects,$(FPS_SOURCES))
ALL_OBJECTS := $(sort $(FPS_OBJECTS))
BIN_TARGETS := $(BIN_DIR)/kw_fps_patch.dll
CONFIG_INPUTS := $(wildcard $(REPO_ROOT)/config/*)
CONFIG_TARGETS := $(patsubst $(REPO_ROOT)/config/%,$(CONFIG_DIR)/%,$(CONFIG_INPUTS))
.PHONY: all config developer clean test-render-clock-build
all: $(BIN_DIR)/kw_fps_patch.dll config
developer: $(BIN_TARGETS) config
config: $(CONFIG_TARGETS)
RENDER_CLOCK_TEST := $(OUT_DIR)/tests/test_render_clock.exe
test-render-clock-build: $(RENDER_CLOCK_TEST)
$(RENDER_CLOCK_TEST): $(REPO_ROOT)/tests/test_render_clock.c $(REPO_ROOT)/src/runtime/game_patches.c \
    $(wildcard $(REPO_ROOT)/src/runtime/*.h) $(KWMOD_SDK_ROOT)/include/kwengine/timing/game_layout.h
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(CPPFLAGS) $(CFLAGS) -static -ffunction-sections -fdata-sections -Wl,--gc-sections \
	    "$(REPO_ROOT)/tests/test_render_clock.c" -lwinmm -lm -o "$@"
$(CONFIG_DIR)/%: $(REPO_ROOT)/config/%
	@mkdir -p "$(@D)"
	cp "$<" "$@"
ENGINE_ARTIFACTS := $(KWMOD_TIMING_LIB)
include $(KWMOD_SDK_ROOT)/mk/sdk_dependencies.mk
$(ENGINE_ARTIFACTS): | $(SDK_ARTIFACT_STAMP)
	@test -f "$@"
$(BIN_DIR)/kw_fps_patch.dll: $(FPS_OBJECTS) $(KWMOD_TIMING_LIB)
	@mkdir -p "$(@D)"
	$(WINDOWS_CC) $(DLL_FLAGS) $(FPS_OBJECTS) $(KWMOD_TIMING_LIB) -lwinmm -lm -o "$@"
clean:
	$(RM) $(BIN_TARGETS) $(ALL_OBJECTS) $(ALL_OBJECTS:.o=.d) $(CONFIG_TARGETS) $(RENDER_CLOCK_TEST)
-include $(ALL_OBJECTS:.o=.d)

BUNDLE_FILES := $(BIN_DIR)/kw_fps_patch.dll=plugins/kw_fps_patch.dll \
    $(REPO_ROOT)/config/kw_fps_patch.kwplugin=plugins/kw_fps_patch.kwplugin \
    $(REPO_ROOT)/config/kw_fps_patch.ini=plugins/kw_fps_patch.ini
include $(KWMOD_SDK_ROOT)/mk/bundle.mk

include $(KWMOD_SDK_ROOT)/mk/integration.mk
