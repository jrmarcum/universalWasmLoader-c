# overview — universalWasmLoader-c

## What this is

`universalWasmLoader-c` is the **C / header-only** port of the Universal WASM Loader family. It is
the cross-language sibling of the reference TypeScript/JavaScript loader (`universalWasmLoader`) and
the Rust/Python ports. Its job is to let C — and, via the C ABI, **Zig / V / Julia** consumers —
load and call `.wasm` reactor/library modules (produced by `wasmtk`, e.g. from TypeScript via
`wasic`/`modc`) with Canonical-ABI marshalling handled for them, the same way the JS loader does in
its ecosystem.

- **Language / runtime:** C (C11), built on the **wasmtime C API** (`wasm.h` / `wasmtime.h` /
  `wasi.h`) as the underlying WASM engine.
- **Intended consumers:** C, C++, **Zig, V, Julia** (any language that can call a C header / link a
  C library). The header-only form is specifically aimed at those header-friendly consumers.
- **Distribution:** **header-only** — a single STB-style header, `universal_wasm_loader.h`. Declarations
  are always visible; the implementation is compiled into exactly one TU that does
  `#define UWL_IMPLEMENTATION` before including. Consumers link against the wasmtime C API library.

## Current state — IMPLEMENTED v1 (2026-06-17)

The header-only loader is implemented and passes the full reference suite. Working tree:

- `universal_wasm_loader.h` — the single-header library (public API + `UWL_IMPLEMENTATION` body).
- `tests/spec_tests.c` — the SPEC §8 reference suite (29 assertions).
- `tests/*.wasm` + `tests/*.wit` — the four reference fixtures (`math_50`, `booleans_50`,
  `strings_50`, `imports_50`), copied verbatim from the `-rs` port (unmodified per §7-#7).
- `Makefile` — `make fetch` (download the wasmtime C API SDK into `vendor/`), `make test` (build +
  run), `make clean`. Override `WASMTIME_VERSION` / `WASMTIME_PLATFORM` / `WASMTIME_LINK`.
- `scripts/fetch-wasmtime.sh` — fetches + unzips the wasmtime C API release artifact into `vendor/`.
- `vendor/` — the wasmtime C API SDK (headers + `libwasmtime.a` / import lib). **gitignored**;
  fetched on demand, not committed.

Build environment used for v1: MinGW-w64 gcc + wasmtime C API **v45.0.2** `x86_64-mingw`, static link.

## Public API surface (implemented)

Idiomatic C equivalents of the reference loader's API. All in `universal_wasm_loader.h`.

- **`uwl_val_t`** — tagged-union value (`UWL_VOID/I32/I64/F32/F64/BOOL/STR`) with constructors
  (`uwl_i32`, `uwl_f64`, `uwl_bool`, `uwl_str`, `uwl_strn`, …) and accessors (`uwl_as_i32`,
  `uwl_as_str`, …). Owned string values/results are freed with `uwl_val_free`.
- **`uwl_import(path, callbacks, ncallbacks, &err)`** → `uwl_module_t*` — the core entry point
  (the synchronous C analog of `wasmImport`; C has no promises). Reads the `.wasm`, auto-detects the
  companion `.wit`, applies the Canonical ABI, instantiates, calls `_initialize` if present, and
  enforces `@N` version pinning. On failure returns `NULL` with a heap `*err` (free with
  `uwl_string_free`).
- **`uwl_call(m, name, args, nargs, &out, &err)`** → 0/-1 — invoke an export by its camelCase WIT
  name (or raw WASM name when no `.wit`).
- **`uwl_free(m)`**.
- **Singleton (DLL pattern):** `uwl_singleton_new` / `uwl_singleton_get` (loads once, caches) /
  `uwl_singleton_free`.
- **`InstancePool` (server/loop pattern):** `uwl_pool_new(size)` / `uwl_pool_acquire` /
  `uwl_pool_release` / `uwl_pool_size` / `uwl_pool_available` / `uwl_pool_free`. The C pool is **not
  internally synchronized** (single-threaded model; caller guards concurrency) — documented in the
  header; sufficient for the SPEC scenario (size=2, two acquires distinct, release restores).

### Host import callbacks

`uwl_host_callback_t { const char *name; uwl_host_fn_t fn; void *userdata; }` keyed by camelCase WIT
import name (e.g. `"envMul"`). Imports are defined into a `wasmtime_linker_t` under the `env`
namespace using the underscore form of the WIT name (`env-mul` → `env_mul`). A single trampoline
(`uwl__import_trampoline`) decodes args (strings read from caller memory via
`wasmtime_caller_export_get("memory")`), calls the user fn, and encodes the result. Remaining
unsatisfied imports are bound to traps (`wasmtime_linker_define_unknown_imports_as_traps`) so
pure-compute modules and partial-import modules still instantiate.

## Canonical ABI / SPEC conformance status — CONFORMANT to SPEC 3.0.0

- **String PARAMS:** `cabi_realloc(0,0,1,len)` → write UTF-8 bytes into linear memory → pass
  `(ptr,len)` as two i32 args. (Implemented fresh; no deprecated form.)
- **String RETURNS (callee-allocated, the v3.0.0 breaking change):** the export returns an **i32
  pointer** to a callee-allocated `[ptr,len]` pair; the loader reads both little-endian i32s from
  memory, decodes (copies) the bytes, then calls the paired **`cabi_post_<camelName>(retArea)`**
  export to release the buffer (no-op under wasic's bump allocator, but the contract is honored).
- **Numerics/bool:** direct; bool encoded `?1:0`, decoded `!=0`.
- **§10 reactor + WASI:** calls `_initialize` once after instantiation if present; configures a
  wasmtime WASI context (`wasmtime_context_set_wasi`, stdout/stderr inherited) and
  `wasmtime_linker_define_wasi` so I/O-using libraries instantiate. Pure-compute modules ignore it.

## Tests

`make test` → builds `tests/spec_tests.c` and runs it: **29 passed, 0 failed.** Covers all four
fixtures plus singleton, pool, and version-pin-rejected scenarios. The fixtures are byte-identical to
the `-rs`/`-js` ones (SPEC §7-#7).

## Build / release flow

- **Local:** `make fetch` then `make test` (needs `curl`, `unzip`, a C compiler, `make`).
- **Toolchain note (MinGW static link):** `wasi.h` decorates every `_WIN32` symbol
  `__declspec(dllimport)` unless `WASI_API_EXTERN` is pre-defined (unlike `wasm.h`, which already
  exempts MinGW). The header pre-defines `WASI_API_EXTERN` to plain `extern` for `__MINGW32__` /
  `LIBWASM_STATIC` builds so the `wasi_config_*` symbols resolve against `libwasmtime.a`. Without it,
  static MinGW links fail with `undefined reference to __imp_wasi_config_*`.
- **Distribution — vcpkg (owner decision 2026-06-15), AUTHORED 2026-06-17:** the repo doubles as a
  vcpkg **git registry**. `ports/universal-wasm-loader-c/` holds `vcpkg.json` (v1.0.0, MIT) +
  `portfile.cmake` + `universal-wasm-loader-c-config.cmake.in` + `usage`. The portfile downloads the
  loader header (raw github at tag `v<version>`, pinned by sha512) AND the matching **wasmtime C API
  artifact per triplet** (no `wasmtime` port exists in vcpkg — it's prebuilt GitHub releases), installs
  both headers + the wasmtime lib (release+debug copies), and emits a CMake config exposing INTERFACE
  target `unofficial::universal-wasm-loader-c` wired with the wasmtime lib + platform syslibs.
  Supported triplets: x64-windows(+static), x64-mingw-(dynamic|static), x64/arm64-linux, x64/arm64-osx
  (real sha512 captured for each wasmtime artifact, v45.0.2). Consumer example in
  `examples/cmake-consumer/`; setup guide in `docs/vcpkg.md`. **VALIDATED 2026-06-17** end-to-end on
  `x64-mingw-dynamic`: `vcpkg install` (overlay port; `downloads/` pre-seeded with the exact header /
  LICENSE / wasmtime files so it validates offline before the tag exists) passed post-build validation
  (MIT detected); the consumer example built via `find_package` + `unofficial::universal-wasm-loader-c`
  and ran `add(3,4)=7`. Two portfile bugs fixed during validation: (1) `;`-separated commands on one
  line are a CMake parse error → split to separate lines; (2) dropped the unused `vcpkg-cmake-config`
  host dep (config is hand-written via `configure_file`), which had forced an MSVC host build. **Real
  (non-seeded) installs still require pushing + tagging `v1.0.0`** so the raw-github header/LICENSE
  downloads resolve. `versions/` regenerated to the validated port tree via `vcpkg x-add-version`. See
  the ecosystem publishing matrix in wasmtk `cmem/vision.md`.

## Known gaps / not yet done

- **Host import callbacks returning `string`** are not supported (would require allocating into WASM
  memory from the trampoline; no fixture needs it). Numeric/bool import returns work.
- **URL loading** (the JS/`-rs` `http(s)://` path) is not implemented — file paths only.
- **Thread-safe pool** (blocking acquire) — the C pool is single-threaded; a future revision could
  add mutex/condvar gating behind a compile flag.
- **vcpkg port** validated on `x64-mingw-dynamic` (2026-06-17); the other triplets (MSVC x64-windows,
  linux, macos) are authored with real sha512 but not yet build-tested. CI not set up.
