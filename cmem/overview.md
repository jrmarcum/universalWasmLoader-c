# overview — universalWasmLoader-c

## What this is

`universalWasmLoader-c` is the **C / header-only** port of the Universal WASM Loader family. It is
the cross-language sibling of the reference TypeScript/JavaScript loader (`universalWasmLoader`) and
the Rust/Python ports. Its job is to let C — and, via the C ABI, **Zig / V / Julia** consumers —
load and call `.wasm` component modules (produced by `wasmtk`, e.g. from TypeScript via `wasic`)
with Canonical-ABI marshalling handled for them, the same way the JS loader does in its ecosystem.

- **Language / runtime:** C, built on the **wasmtime C API** (`wasm.h` / `wasmtime.h`) as the
  underlying WASM engine.
- **Intended consumers:** C, C++, **Zig, V, Julia** (any language that can call a C header / link a
  C library). The header-only form is specifically aimed at those header-friendly consumers.
- **Distribution:** **header-only** (single-header `#include`, plus linking against the wasmtime C
  API library). No separate compiled artifact is intended to be required by consumers.

## Current state — EARLY STUB (as of 2026-06-15)

This repo is a **stub**, not an implementation. The working tree contains only:

- `README.md` — one-line description ("Universal wasm loader for C/C++/Zig/V").
- `LICENSE`
- `.gitignore` — standard C/C++ build-artifact ignores.
- `CLAUDE.md` — project goal + explicit note that "No source tree exists yet."

There is **no source code** yet: no `.h` / `.c` files, no build system (no `Makefile` / `CMakeLists.txt`),
no examples, and **no test harness**. The git history is three commits: `14f0c5e Initial commit`,
`5f0dfd8 update docs`, `91a8a27 update docs`.

## Public API surface

**None implemented yet.** The intended (not-yet-written) surface is the idiomatic C equivalent of the
reference loader's API:

- `wasmImport(...)` → singleton/DLL-pattern load + call (cache one instance after first load).
- `createSingleton(...)` → explicit single-instance handle.
- `InstancePool` → pool of fresh instances for server/loop use (the bump allocator in wasic-produced
  modules has no `free`, so pooling is how long-running hosts avoid heap exhaustion — see the
  reference loader's `acquire` / `release` / `run` / `destroy` API).

When implementing, mirror the reference loader's names/semantics so this port conforms to the
cross-language `SPEC.md`. None of these exist in C here today.

## Canonical ABI / SPEC conformance status

- **Cross-language `SPEC.md` is at v3.0.0 (2026-06-15) — a BREAKING change.** In v3.0.0, string and
  aggregate **RETURNS** moved from the OLD caller-allocated out-parameter convention to the
  **canonical callee-allocated convention**: the export returns an **i32 pointer** to a
  callee-allocated `[ptr, len]` pair; the host reads `ptr`/`len` from that pair (e.g. via the
  module's memory) and then MUST call the paired **`cabi_post_<name>(retPtr)`** export to release the
  callee-side allocation.
- **This port's string-return handling: NONE (no source exists).** There is no out-param code, no
  `cabi_post` call, no string marshalling of any kind — nothing to align. A future session bringing
  this port up to SPEC 3.0.0 is therefore implementing string returns **fresh against the NEW
  callee-allocated + `cabi_post_<name>` convention from the start** (do not implement the deprecated
  out-param form). String PARAMS still use `cabi_realloc` to allocate + copy bytes into module memory,
  same as the reference loader.

## Tests

No test harness yet. (When one is added, record the real command here and in `INDEX.md`, replacing the
`(no test harness yet)` placeholder.)

## Build / release flow

Not yet established. Expected shape: a single-header include consumed directly by C/Zig/V/Julia
projects, linked against the wasmtime C API. No build system, packaging, or release tooling exists in
the repo at this time.
