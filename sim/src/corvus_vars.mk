# Shared Corvus/Corvusitor configuration for YuQuan builds.

# Root of the YuQuan repo; callers can override before include.
YQ_DIR ?= $(CURDIR)

# Bus topology defaults (corvusitor-facing).
CORVUSITOR_MBUS_COUNT ?= 8
CORVUSITOR_SBUS_COUNT ?= 3

# Verilator output tree.
YUQUAN_SIM_DIR ?= $(YQ_DIR)/build/sim
YUQUAN_SENTINEL ?= $(YUQUAN_SIM_DIR)/verilator-compile-corvus_external/Vcorvus_external.h

# Corvusitor output location and naming.
CORVUSITOR_OUTPUT_DIR ?= $(YQ_DIR)/build
CORVUSITOR_CMODEL_OUTPUT_NAME ?= YuQuan

CORVUSITOR_CMODEL_SENTINEL ?= $(CORVUSITOR_OUTPUT_DIR)/$(CORVUSITOR_CMODEL_OUTPUT_NAME)_corvus.json

# Corvusitor binary override.
CORVUSITOR_BIN ?= $(if $(CORVUSITOR_PATH),$(CORVUSITOR_PATH),corvusitor)
