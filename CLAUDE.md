# universalWasmLoader-c

Universal WebAssembly loader for C/C++/Zig/V.

## Project goal

Provide a single, reusable C library (or set of headers) that can load and execute `.wasm` modules from code written in C, C++, Zig, or V — without requiring each language's ecosystem-specific WASM runtime boilerplate.

## Repository state

Early-stage project. As of the initial commit the repo contains only `README.md`, `.gitignore`, and `LICENSE`. No source tree exists yet.

## Toolchain & build

- Target languages / consumers: C, C++, Zig, V
- Compiled output types: static library (`.a`/`.lib`), shared library (`.so`/`.dll`/`.dylib`), or header-only — TBD
- `.gitignore` covers standard C/C++ artefacts: object files, libraries, executables, debug files, and kernel-module build artefacts

## Conventions

- No conventions established yet — update this file as the project grows.

## All project context lives here

All Claude Code project context, conventions, and decisions are kept in this file.
No machine-local memory is needed.
