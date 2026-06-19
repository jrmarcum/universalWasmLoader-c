/*
 * adder.c — wrap a WASM export in a native-looking C function.
 *
 * The module exports  add: func(a: s32, b: s32) -> s32  (see math_50.wit).
 * We expose it to the rest of the program as an ordinary  int adder(int, int).
 *
 * Two patterns are shown:
 *   1. adder()        — a plain int(int,int), backed by a cached singleton so
 *                       callers never see the module handle.
 *   2. adder_with()   — explicit: you pass the module in. Use this when you
 *                       manage the module's lifetime yourself (pool, etc.).
 *
 * Build (from the repo root, after `make fetch`):
 *   V=wasmtime-v45.0.2-x86_64-mingw-c-api
 *   cc -std=c11 -Ivendor/$V/include examples/adder/adder.c -o adder.exe \
 *      -Lvendor/$V/lib vendor/$V/lib/libwasmtime.a \
 *      -lws2_32 -lbcrypt -luserenv -lntdll -lole32 -lShlwapi -ladvapi32 \
 *      -lkernel32 -luuid -lpthread
 *   ./adder.exe            # → adder(3, 4) = 7
 */
#define UWL_IMPLEMENTATION
#include "../../universal_wasm_loader.h"

#include <stdio.h>
#include <stdlib.h>

/* Path to the module (with its companion math_50.wit alongside). */
#define ADDER_WASM "tests/math_50.wasm"

/* ── Pattern 1: native-looking int adder(int, int) ──────────────────────────
 * The module is loaded once on first use and cached in a singleton, so this
 * looks and behaves like any other C function. */
int adder(int a, int b) {
    static uwl_singleton_t *s = NULL;
    if (!s) {
        s = uwl_singleton_new(ADDER_WASM, NULL, 0);
        if (!s) { fprintf(stderr, "adder: out of memory\n"); exit(1); }
    }

    char *err = NULL;
    uwl_module_t *m = uwl_singleton_get(s, &err);
    if (!m) { fprintf(stderr, "adder: load failed: %s\n", err ? err : "?"); uwl_string_free(err); exit(1); }

    int sum = uwl_call_i32(m, "add", a, b);   /* WIT says add(s32, s32) -> s32 */
    if (uwl_last_error()) { fprintf(stderr, "adder: call failed: %s\n", uwl_last_error()); exit(1); }
    return sum;
}

/* ── Pattern 2: explicit module handle ──────────────────────────────────────
 * Same call, but the caller owns the module (loaded once, reused many times). */
int adder_with(uwl_module_t *m, int a, int b) {
    int sum = uwl_call_i32(m, "add", a, b);
    if (uwl_last_error()) { fprintf(stderr, "adder_with: %s\n", uwl_last_error()); exit(1); }
    return sum;
}

int main(void) {
    /* Pattern 1: just call it like a normal function. */
    printf("adder(3, 4) = %d\n", adder(3, 4));
    printf("adder(20, 1) = %d\n", adder(20, 1));   /* reuses the cached module */

    /* Pattern 2: load once, pass the handle in. */
    uwl_module_t *m = uwl_import(ADDER_WASM, NULL, 0, NULL);
    if (!m) { fprintf(stderr, "load failed: %s\n", uwl_last_error()); return 1; }
    printf("adder_with(m, 5, 6) = %d\n", adder_with(m, 5, 6));
    uwl_free(m);

    return 0;
}
