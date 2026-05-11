# Project Name
TARGET = circular_quadraphonic

# Sources
CPP_SOURCES = src/main.cpp

# Library Locations
LIBDAISY_DIR ?= ../libDaisy
DAISYSP_DIR  ?= ../DaisySP

# Compiler flags — constitution mandates zero warnings
CFLAGS   += -Wall -Wextra
CPPFLAGS += -Wall -Wextra

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
