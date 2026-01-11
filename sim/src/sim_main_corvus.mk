YQ_DIR ?= $(shell realpath `pwd`/../../..)
include $(YQ_DIR)/sim/src/corvus_vars.mk
CORVUSITOR_ROOT ?= $(shell realpath $(YQ_DIR)/../..)
LIB_DIR = $(YQ_DIR)/difftest/difftest/build
CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -g

VERILATOR_ROOT ?= $(shell verilator --getenv VERILATOR_ROOT)
_CORVUS_USER_INCLUDE_FLAGS = -I$(YQ_DIR)/sim/include \
                             -I$(CORVUSITOR_OUTPUT_DIR) \
                             -I$(CORVUSITOR_ROOT) \
                             -I$(CORVUSITOR_ROOT)/boilerplate/common \
                             -I$(CORVUSITOR_ROOT)/boilerplate/corvus_cmodel \
                             -I$(CORVUSITOR_ROOT)/boilerplate/corvus \
                             -I$(VERILATOR_ROOT)/include \
                             -I$(VERILATOR_ROOT)/include/vltstd
_CORVUS_USER_MACRO_FLAGS = -DDIFFTEST
_CORVUS_USER_LIB_FLAGS = -L$(LIB_DIR) -lrv64spike -pthread
VERILATOR_LIBS := $(wildcard $(YUQUAN_SIM_DIR)/verilator-compile-*/libV*.a)
VERILATOR_LIBS += $(firstword $(wildcard $(YUQUAN_SIM_DIR)/verilator-compile-*/libverilated.a))
_CORVUS_USER_SRC_FILES = $(YQ_DIR)/sim/src/peripheral/uart/uart.cpp \
				         $(YQ_DIR)/sim/src/peripheral/sdcard/sdcard.cpp \
				         $(YQ_DIR)/sim/src/peripheral/ram/ram.cpp \
				         $(YQ_DIR)/sim/src/peripheral/spiFlash/spiFlash.cpp
_CORVUS_MAIN_SRC = $(YQ_DIR)/sim/src/sim_main_corvus.cpp
_CORVUS_TARGET = sim_main_corvus
CORVUSITOR_GEN_SRC_GLOB = $(CORVUSITOR_OUTPUT_DIR)/C$(CORVUSITOR_CMODEL_OUTPUT_NAME)*.cpp
CORVUSITOR_RUNTIME_SRCS := $(wildcard $(CORVUSITOR_ROOT)/boilerplate/corvus_cmodel/*.cpp) \
                           $(wildcard $(CORVUSITOR_ROOT)/boilerplate/corvus/*.cpp)

export LD_LIBRARY_PATH := $(LIB_DIR):$(LD_LIBRARY_PATH)

.PHONY: yuquan_cmodel_gen
yuquan_cmodel_gen: $(CORVUSITOR_CMODEL_SENTINEL)

$(YUQUAN_SENTINEL):
	$(MAKE) -C $(YQ_DIR) CORVUS=1 YUQUAN_SIM_DIR=$(YUQUAN_SIM_DIR) verilate-archive

$(CORVUSITOR_CMODEL_SENTINEL): $(YUQUAN_SENTINEL)
	$(MAKE) -C $(YQ_DIR) yuquan_cmodel_gen \
	  CORVUSITOR_BIN=$(CORVUSITOR_BIN) \
	  CORVUSITOR_MBUS_COUNT=$(CORVUSITOR_MBUS_COUNT) \
	  CORVUSITOR_SBUS_COUNT=$(CORVUSITOR_SBUS_COUNT) \
	  YUQUAN_SIM_DIR=$(YUQUAN_SIM_DIR) \
	  CORVUSITOR_OUTPUT_DIR=$(CORVUSITOR_OUTPUT_DIR) \
	  CORVUSITOR_CMODEL_OUTPUT_NAME=$(CORVUSITOR_CMODEL_OUTPUT_NAME)

ISA := riscv64
ifneq ($(BIN),)
binFile = $(YQ_DIR)/sim/bin/$(BIN)-$(ISA)-nemu.bin
flashBinFile = $(YQ_DIR)/sim/bin/$(BIN)~flash-$(ISA)-nemu.bin
endif
$(_CORVUS_TARGET): yuquan_cmodel_gen $(CORVUSITOR_RUNTIME_SRCS) $(_CORVUS_USER_SRC_FILES) $(_CORVUS_MAIN_SRC)
	@gen_srcs="$(wildcard $(CORVUSITOR_GEN_SRC_GLOB))"; \
	 if [ -z "$$gen_srcs" ]; then echo "No Corvusitor-generated sources found in $(CORVUSITOR_OUTPUT_DIR)"; exit 1; fi; \
	$(CXX) $(CXXFLAGS) $(_CORVUS_USER_MACRO_FLAGS) $(_CORVUS_USER_INCLUDE_FLAGS) \
	  $$gen_srcs $(CORVUSITOR_RUNTIME_SRCS) $(_CORVUS_USER_SRC_FILES) $(_CORVUS_MAIN_SRC) \
	  $(_CORVUS_USER_LIB_FLAGS) $(VERILATOR_LIBS) -o $(_CORVUS_TARGET)

sim: yuquan_cmodel_gen $(_CORVUS_TARGET)
ifeq ($(BIN),)
	$(error $(nobin))
endif
	@./$(_CORVUS_TARGET) $(binFile) $(flashBinFile)
