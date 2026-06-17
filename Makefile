# universalWasmLoader-c — build & test
#
# Requires the wasmtime C API SDK in vendor/ (fetch with `make fetch`).
# Override WASMTIME_VERSION / WASMTIME_PLATFORM to match your toolchain.

WASMTIME_VERSION  ?= 45.0.2
WASMTIME_PLATFORM ?= x86_64-mingw
WASMTIME_SDK      ?= vendor/wasmtime-v$(WASMTIME_VERSION)-$(WASMTIME_PLATFORM)-c-api

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra
INCLUDE  = -I$(WASMTIME_SDK)/include
LIBDIR   = -L$(WASMTIME_SDK)/lib

# wasmtime's static lib pulls in these system libraries on Windows/MinGW.
# On Linux/macOS only -lpthread (+ -lm -ldl) are needed; override SYSLIBS.
ifeq ($(OS),Windows_NT)
  SYSLIBS ?= -lws2_32 -lbcrypt -luserenv -lntdll -lole32 -lShlwapi -ladvapi32 \
             -lkernel32 -luuid -lpthread
  EXE      = .exe
else
  UNAME_S := $(shell uname -s)
  SYSLIBS ?= -lpthread -lm -ldl
  EXE      =
endif

# Static link by default (no DLL on PATH needed). Set WASMTIME_LINK=shared to
# link the import library and run with wasmtime.dll alongside the binary.
WASMTIME_LINK ?= static
ifeq ($(WASMTIME_LINK),shared)
  WASMTIME_LIB = -lwasmtime
else
  WASMTIME_LIB = $(WASMTIME_SDK)/lib/libwasmtime.a
endif

TEST_BIN = tests/spec_tests$(EXE)

.PHONY: all test fetch clean

all: test

fetch:
	bash scripts/fetch-wasmtime.sh $(WASMTIME_VERSION) $(WASMTIME_PLATFORM)

$(TEST_BIN): tests/spec_tests.c universal_wasm_loader.h | check-sdk
	$(CC) $(CFLAGS) $(INCLUDE) tests/spec_tests.c -o $(TEST_BIN) \
	  $(LIBDIR) $(WASMTIME_LIB) $(SYSLIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

check-sdk:
	@test -d "$(WASMTIME_SDK)/include" || { \
	  echo "wasmtime C API not found at $(WASMTIME_SDK)."; \
	  echo "Run: make fetch"; exit 1; }

clean:
	rm -f $(TEST_BIN)
