# universalWasmLoader-c

A single-header **C** loader for `.wasm` reactor/library modules produced by
[`wasmtk`](https://github.com/jrmarcum/wasmtk) (e.g. TypeScript compiled via `wasic`/`modc`). It
auto-detects the companion `.wit` file and applies the Canonical ABI, so you call WASM exports with
native C values — the C sibling of the reference JS loader
[`@jrmarcum/universal-wasm-loader`](https://jsr.io/@jrmarcum/universal-wasm-loader). Conforms to the
cross-language **SPEC v3.0.0** (callee-allocated string returns + `cabi_post_<name>`).

Built on the [wasmtime C API](https://docs.wasmtime.dev/c-api/). Usable from C, C++, and — via the C
ABI — Zig, V, and Julia.

## Quick start

```c
#define UWL_IMPLEMENTATION          // in exactly ONE .c file
#include "universal_wasm_loader.h"

int main(void) {
    uwl_module_t *m = uwl_import("math_50.wasm", NULL, 0, NULL);
    if (!m) { fprintf(stderr, "%s\n", uwl_last_error()); return 1; }

    int r = uwl_call_i32(m, "add", 3, 4);          // 7
    printf("add(3, 4) = %d\n", r);

    uwl_free(m);
    return 0;
}
```

Every other translation unit just `#include "universal_wasm_loader.h"` (no macro). Link against the
wasmtime C API library.

The typed convenience calls (`uwl_call_i32` / `_i64` / `_f32` / `_f64` / `_bool` / `_str` / `_void`)
read native C arguments and return a native C result — the C analog of the reference loader's
`m.add(3, 4)`. Each argument is read with the correct type from the export's WIT signature, so you
pass plain C values. On failure they return `0`/`NULL` and set a thread-local error you read with
`uwl_last_error()`. They require a companion `.wit`; for raw modules use the lower-level
`uwl_call(m, name, args, nargs, &out, &err)` with explicit `uwl_val_t` values (see
[API reference](#api-reference)).

## Strings

```c
char *g = uwl_call_str(m, "greet", "World");   // "Hello, World!"
printf("%s\n", g);
uwl_string_free(g);                            // free the owned result
```

String parameters are UTF-8-encoded and copied into WASM memory via `cabi_realloc`. String results
follow the SPEC v3.0.0 callee-allocated convention: the loader reads the returned `[ptr, len]` pair,
copies the bytes out, and calls `cabi_post_<name>` to release the module's buffer. `uwl_call_str`
hands you an owned heap string — free it with `uwl_string_free`.

## Host imports

Provide host callbacks (keyed by the **camelCase** WIT import name) for modules that import functions:

```c
static uwl_val_t env_mul(const uwl_val_t *a, size_t n, void *ud) {
    return uwl_f64(uwl_as_f64(a[0]) * uwl_as_f64(a[1]));
}

uwl_host_callback_t cbs[] = { { "envMul", env_mul, NULL } };
uwl_module_t *m = uwl_import("imports.wasm", cbs, 1, NULL);

double s = uwl_call_f64(m, "scale", 3.0, 4.0);   // module calls back into envMul
```

Host callbacks still receive and return `uwl_val_t` (the callee side inspects arguments
dynamically); only the export-call side gains the typed convenience wrappers.

## Version pinning

Append `@N` to assert the module's exported `version` i32 global equals `N` (the C shared-library
SONAME convention):

```c
uwl_module_t *m = uwl_import("mod.wasm@2", NULL, 0, NULL);  // fails unless version == 2
```

## Lifecycle helpers

**Singleton** (load once, cache — for CLI/bounded use):

```c
uwl_singleton_t *s = uwl_singleton_new("mod.wasm", NULL, 0);
uwl_module_t *m = uwl_singleton_get(s, &err);   // same instance every call
int r = uwl_call_i32(m, "add", 3, 4);
uwl_singleton_free(s);
```

**InstancePool** (independent instances — for servers/loops; extends longevity under wasic's bump
allocator):

```c
uwl_pool_t *p = uwl_pool_new("mod.wasm", NULL, 0, 4, &err);
uwl_module_t *m = uwl_pool_acquire(p, &err);
/* ... use m ... */
uwl_pool_release(p, m);
uwl_pool_free(p);
```

> The C pool is not internally synchronized; in a multithreaded host, guard acquire/release with your
> own lock.

## Building & testing

The loader needs the wasmtime C API SDK (headers + library). The Makefile fetches it into `vendor/`
(gitignored):

```sh
make fetch        # download the wasmtime C API SDK (override WASMTIME_VERSION / WASMTIME_PLATFORM)
make test         # build and run the reference suite (tests/spec_tests.c)
```

Defaults target MinGW gcc on Windows (`x86_64-mingw`, static link). For other platforms:

```sh
make fetch WASMTIME_PLATFORM=x86_64-linux
make test  WASMTIME_PLATFORM=x86_64-linux
```

Set `WASMTIME_LINK=shared` to link the import library instead of the static archive (then keep
`wasmtime.dll`/`.so` alongside your binary).

## API reference

See the doc comments in [`universal_wasm_loader.h`](universal_wasm_loader.h). The typed convenience
calls (`uwl_call_i32` etc.) are the recommended path for `.wit`-accompanied modules; the lower-level
`uwl_call` with explicit `uwl_val_t` values is the escape hatch for raw modules and for callers who
want a per-call `err` out-parameter. Memory ownership: string values you build (`uwl_str`/`uwl_strn`)
and string results written to `out` are heap-owned — free them with `uwl_val_free`; the heap string
from `uwl_call_str` and error strings from the `err` out-parameter are freed with `uwl_string_free`.
The string returned by `uwl_last_error()` is owned by the loader — do not free it.

## Install via vcpkg

This repo is a vcpkg registry. With vcpkg configured (see
[`docs/vcpkg.md`](docs/vcpkg.md)):

```cmake
find_package(universal-wasm-loader-c CONFIG REQUIRED)
target_link_libraries(main PRIVATE unofficial::universal-wasm-loader-c)
```

The port downloads the matching wasmtime C API SDK for your triplet and wires
the include path, the wasmtime library, and the platform system libraries into
that one target. See [`docs/vcpkg.md`](docs/vcpkg.md) for registry setup and the
quick overlay-port path.

## License

MIT. See [LICENSE](LICENSE).
