/*
 * spec_tests.c — Reference test suite (SPEC §8) for universalWasmLoader-c.
 *
 * Build & run via the Makefile: `make test`.
 *
 * Calls go through the typed convenience layer (uwl_call_i32 / _f64 / _bool /
 * _str), which reads native C args per the export's WIT signature and reports
 * failures via uwl_last_error().
 */
#define UWL_IMPLEMENTATION
#include "../universal_wasm_loader.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

/* Path to a fixture relative to the test working directory (repo root). */
static const char *FX(const char *base) {
    static char buf[256];
    snprintf(buf, sizeof(buf), "tests/%s", base);
    return buf;
}

static uwl_module_t *load(const char *base, const uwl_host_callback_t *cbs, size_t ncbs) {
    char *err = NULL;
    uwl_module_t *m = uwl_import(FX(base), cbs, ncbs, &err);
    if (!m) { printf("  LOAD ERROR (%s): %s\n", base, err ? err : "?"); uwl_string_free(err); }
    return m;
}

/* Call a string export and compare the owned result to `expect`. */
static int str_eq(uwl_module_t *m, const char *fn, const char *arg, const char *expect) {
    char *got = uwl_call_str(m, fn, arg);
    if (!got) { printf("  CALL ERROR (%s): %s\n", fn, uwl_last_error() ? uwl_last_error() : "?"); return 0; }
    int ok = (strcmp(got, expect) == 0);
    if (!ok) printf("  got '%s' want '%s'\n", got, expect);
    uwl_string_free(got);
    return ok;
}

static void test_math(void) {
    printf("math_50 — numeric round-trip\n");
    uwl_module_t *m = load("math_50.wasm", NULL, 0);
    CHECK(m != NULL, "math_50 loads");
    if (!m) return;
    CHECK(uwl_call_i32(m, "add", 3, 4) == 7, "add(3,4)==7");
    CHECK(fabs(uwl_call_f64(m, "multiply", 2.5, 4.0) - 10.0) < 1e-9, "multiply(2.5,4.0)==10.0");
    CHECK(uwl_call_i32(m, "square", 5) == 25, "square(5)==25");
    uwl_free(m);
}

static void test_booleans(void) {
    printf("booleans_50 — bool normalization\n");
    uwl_module_t *m = load("booleans_50.wasm", NULL, 0);
    CHECK(m != NULL, "booleans_50 loads");
    if (!m) return;
    CHECK(uwl_call_bool(m, "isPositive", 1.0) == 1, "isPositive(1.0)");
    CHECK(uwl_call_bool(m, "isPositive", -1.0) == 0, "isPositive(-1.0)");
    CHECK(uwl_call_bool(m, "inRange", 5.0, 0.0, 10.0) == 1, "inRange(5,0,10)");
    CHECK(uwl_call_bool(m, "inRange", 11.0, 0.0, 10.0) == 0, "inRange(11,0,10)");
    CHECK(uwl_call_bool(m, "isEven", 4) == 1, "isEven(4)");
    CHECK(uwl_call_bool(m, "isEven", 3) == 0, "isEven(3)");
    uwl_free(m);
}

static void test_strings(void) {
    printf("strings_50 — string param + return (Canonical ABI v3.0.0)\n");
    uwl_module_t *m = load("strings_50.wasm", NULL, 0);
    CHECK(m != NULL, "strings_50 loads");
    if (!m) return;
    CHECK(str_eq(m, "greet", "World", "Hello, World!"), "greet(\"World\")==\"Hello, World!\"");
    CHECK(str_eq(m, "shout", "hi", "hihi"), "shout(\"hi\")==\"hihi\"");
    CHECK(uwl_call_i32(m, "strLen", "hello") == 5, "strLen(\"hello\")==5");
    uwl_free(m);
}

static uwl_val_t env_mul(const uwl_val_t *args, size_t n, void *ud) {
    (void)n; (void)ud;
    return uwl_f64(uwl_as_f64(args[0]) * uwl_as_f64(args[1]));
}
static uwl_val_t env_add(const uwl_val_t *args, size_t n, void *ud) {
    (void)n; (void)ud;
    return uwl_i32(uwl_as_i32(args[0]) + uwl_as_i32(args[1]));
}

static void test_imports(void) {
    printf("imports_50 — host import callbacks\n");
    uwl_host_callback_t cbs[2] = {
        { "envMul", env_mul, NULL },
        { "envAdd", env_add, NULL },
    };
    uwl_module_t *m = load("imports_50.wasm", cbs, 2);
    CHECK(m != NULL, "imports_50 loads");
    if (!m) return;
    CHECK(fabs(uwl_call_f64(m, "scale", 3.0, 4.0) - 12.0) < 1e-9, "scale(3,4)==12 (envMul)");
    CHECK(uwl_call_i32(m, "combine", 10, 7) == 17, "combine(10,7)==17 (envAdd)");
    uwl_free(m);
}

static void test_singleton(void) {
    printf("singleton — caches one instance\n");
    uwl_singleton_t *s = uwl_singleton_new(FX("math_50.wasm"), NULL, 0);
    CHECK(s != NULL, "singleton_new");
    char *err = NULL;
    uwl_module_t *a = uwl_singleton_get(s, &err);
    uwl_module_t *b = uwl_singleton_get(s, &err);
    CHECK(a != NULL && a == b, "two get() calls return the same instance");
    if (a) CHECK(uwl_call_i32(a, "add", 3, 4) == 7, "singleton add(3,4)==7");
    uwl_singleton_free(s);
}

static void test_pool(void) {
    printf("pool — independent instances\n");
    char *err = NULL;
    uwl_pool_t *p = uwl_pool_new(FX("math_50.wasm"), NULL, 0, 2, &err);
    CHECK(p != NULL, "pool_new(size=2)");
    if (!p) { printf("  %s\n", err ? err : "?"); uwl_string_free(err); return; }
    CHECK(uwl_pool_size(p) == 2, "pool size==2");
    CHECK(uwl_pool_available(p) == 2, "pool available==2");

    uwl_module_t *a = uwl_pool_acquire(p, &err);
    uwl_module_t *b = uwl_pool_acquire(p, &err);
    CHECK(a && b && a != b, "two concurrent acquires give distinct instances");
    CHECK(uwl_pool_available(p) == 0, "pool available==0 after 2 acquires");
    if (a) CHECK(uwl_call_i32(a, "add", 20, 1) == 21, "pooled add(20,1)==21");
    uwl_pool_release(p, a);
    uwl_pool_release(p, b);
    CHECK(uwl_pool_available(p) == 2, "pool available==2 after release");
    uwl_pool_free(p);
}

static void test_version(void) {
    printf("version pinning — missing version global rejected\n");
    char *err = NULL;
    uwl_module_t *m = uwl_import(FX("math_50.wasm@9"), NULL, 0, &err);
    CHECK(m == NULL, "@9 on a module with no version global fails");
    if (m) uwl_free(m); else uwl_string_free(err);
}

int main(void) {
    test_math();
    test_booleans();
    test_strings();
    test_imports();
    test_singleton();
    test_pool();
    test_version();
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
