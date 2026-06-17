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
    char *err = NULL;
    uwl_module_t *m = uwl_import("math_50.wasm", NULL, 0, &err);
    if (!m) { fprintf(stderr, "%s\n", err); uwl_string_free(err); return 1; }

    uwl_val_t args[2] = { uwl_i32(3), uwl_i32(4) };
    uwl_val_t out;
    if (uwl_call(m, "add", args, 2, &out, &err) == 0)
        printf("add(3, 4) = %d\n", uwl_as_i32(out));   // 7
    uwl_val_free(&out);

    uwl_free(m);
    return 0;
}
```

Every other translation unit just `#include "universal_wasm_loader.h"` (no macro). Link against the
wasmtime C API library.

## Strings

```c
uwl_val_t name = uwl_str("World");
uwl_val_t out;
uwl_call(m, "greet", &name, 1, &out, &err);   // greet("World")
printf("%s\n", uwl_as_str(out));               // "Hello, World!"
uwl_val_free(&out);                            // free the owned result
uwl_val_free(&name);                           // free the owned argument
```

String parameters are UTF-8-encoded and copied into WASM memory via `cabi_realloc`. String results
follow the SPEC v3.0.0 callee-allocated convention: the loader reads the returned `[ptr, len]` pair,
copies the bytes out, and calls `cabi_post_<name>` to release the module's buffer.

## Host imports

Provide host callbacks (keyed by the **camelCase** WIT import name) for modules that import functions:

```c
static uwl_val_t env_mul(const uwl_val_t *a, size_t n, void *ud) {
    return uwl_f64(uwl_as_f64(a[0]) * uwl_as_f64(a[1]));
}

uwl_host_callback_t cbs[] = { { "envMul", env_mul, NULL } };
uwl_module_t *m = uwl_import("imports.wasm", cbs, 1, &err);
```

## Version pinning

Append `@N` to assert the module's exported `version` i32 global equals `N` (the C shared-library
SONAME convention):

```c
uwl_module_t *m = uwl_import("mod.wasm@2", NULL, 0, &err);  // fails unless version == 2
```

## Lifecycle helpers

**Singleton** (load once, cache — for CLI/bounded use):

```c
uwl_singleton_t *s = uwl_singleton_new("mod.wasm", NULL, 0);
uwl_module_t *m = uwl_singleton_get(s, &err);   // same instance every call
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

See the doc comments in [`universal_wasm_loader.h`](universal_wasm_loader.h). Memory ownership:
string values you build (`uwl_str`/`uwl_strn`) and string results written to `out` are heap-owned —
free them with `uwl_val_free`. Error strings from the `err` out-parameter are freed with
`uwl_string_free`.

## License

Apache-2.0. See [LICENSE](LICENSE).
