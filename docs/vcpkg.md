# Using universal-wasm-loader-c with vcpkg

This repository doubles as a **vcpkg git registry**. The port
`universal-wasm-loader-c` installs the loader header **and** the matching
wasmtime C API SDK for your triplet, then exposes one CMake target that links
everything:

```cmake
find_package(universal-wasm-loader-c CONFIG REQUIRED)
target_link_libraries(main PRIVATE unofficial::universal-wasm-loader-c)
```

There is no separate `wasmtime` vcpkg port — wasmtime ships as prebuilt GitHub
release artifacts, so this port downloads the right one (per triplet) and
installs its headers/libs alongside the loader.

---

## Prerequisites

- **git**, **CMake ≥ 3.15**, and a C/C++ toolchain (MSVC or MinGW on Windows;
  gcc/clang on Linux/macOS).
- **Network access** at install time — the port downloads the wasmtime C API
  archive (~30 MB) and the loader header from this repo's release tag.
- Supported triplets: `x64-windows`, `x64-windows-static`, `x64-mingw-dynamic`,
  `x64-mingw-static`, `x64-linux`, `arm64-linux`, `x64-osx`, `arm64-osx`.

## 0. One-time publish prerequisite (maintainer)

The portfile fetches the loader header and LICENSE from
`raw.githubusercontent.com/jrmarcum/universalWasmLoader-c/v<version>/…`, so the
tag must exist on GitHub:

```sh
git push origin main
git tag v1.1.0 && git push origin v1.1.0
```

(If the header content changes, bump `UWL_VERSION` + `UWL_HEADER_SHA512` in
`ports/universal-wasm-loader-c/portfile.cmake`, re-tag, and run
`vcpkg x-add-version --all` — see §4.)

## 1. Get vcpkg

```sh
git clone https://github.com/microsoft/vcpkg
./vcpkg/bootstrap-vcpkg.sh      # Windows: .\vcpkg\bootstrap-vcpkg.bat
export VCPKG_ROOT="$PWD/vcpkg"  # Windows (PowerShell): $env:VCPKG_ROOT = "$PWD\vcpkg"
```

## 2a. Consume as a git registry (recommended)

In your **consumer project**, add two files:

`vcpkg.json`
```json
{
  "name": "my-app",
  "version": "0.1.0",
  "dependencies": ["universal-wasm-loader-c"]
}
```

`vcpkg-configuration.json`
```json
{
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "<a recent microsoft/vcpkg commit sha>"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/jrmarcum/universalWasmLoader-c",
      "baseline": "<commit sha of this repo's main after versions/ is committed>",
      "packages": ["universal-wasm-loader-c"]
    }
  ]
}
```

Get the registry baseline with `git rev-parse HEAD` in this repo (after pushing
the `versions/` files). Then configure with the vcpkg toolchain:

```sh
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build
```

vcpkg installs the port in manifest mode automatically.

## 2b. Quick local try (overlay port)

No registry/baseline needed — point vcpkg straight at this repo's `ports/`:

```sh
# classic mode
"$VCPKG_ROOT/vcpkg" install universal-wasm-loader-c \
  --overlay-ports=/path/to/universalWasmLoader-c/ports \
  --triplet x64-windows

# or manifest mode: set the env var, then configure with the toolchain
export VCPKG_OVERLAY_PORTS=/path/to/universalWasmLoader-c/ports
```

(The header/LICENSE downloads still require the matching `v<version>` tag from §0.)

## 3. Build the example

`examples/cmake-consumer/` is a complete consumer. From a checkout:

```sh
cd examples/cmake-consumer
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_OVERLAY_PORTS=../../ports
cmake --build build
./build/consumer ../../tests/math_50.wasm   # prints add(3,4)=7
```

## 4. Maintaining the registry

After editing the port, the `git-tree` recorded in
`versions/u-/universal-wasm-loader-c.json` must be updated to the new port tree,
or registry installs fail the git-tree check.

`vcpkg x-add-version` is the usual tool, but it expects a full vcpkg-root layout
(`scripts/vcpkg-tools.json`) that this minimal registry doesn't have, so we
maintain it by hand — the value is just `git rev-parse HEAD:ports/<port>`:

```sh
git add ports/universal-wasm-loader-c && git commit -m "port: <change>"
TREE=$(git rev-parse HEAD:ports/universal-wasm-loader-c)
# set versions/u-/universal-wasm-loader-c.json "git-tree" to $TREE
# (bump "version" + "baseline" too when releasing a new version)
git add versions && git commit -m "registry: bump git-tree"
```

The consumer's `vcpkg-configuration.json` baseline (§2a) must point at a commit
where `versions/` already reflects the port you want — i.e. run `git rev-parse
HEAD` *after* committing the `versions/` update.

## Troubleshooting

- **SHA512 mismatch on the header/LICENSE download** — the `v<version>` tag
  doesn't exist yet or points at different bytes. Complete §0.
- **`undefined reference to __imp_wasi_config_*` (MinGW)** — only affects the
  manual `make` build; the header pre-defines `WASI_API_EXTERN` for MinGW/static
  so vcpkg consumers are unaffected.
- **Runtime can't find `wasmtime.dll` / `libwasmtime.so`** (dynamic triplets) —
  vcpkg's `applocal` deployment copies it next to your binary on Windows; on
  Linux/macOS ensure the vcpkg `…/lib` (or installed `bin`) is on the loader
  path, or use a `*-static` triplet.
