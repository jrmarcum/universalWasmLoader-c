/* Minimal consumer of universal-wasm-loader-c installed via vcpkg.
 * Usage: consumer path/to/math_50.wasm  →  prints add(3, 4) = 7 */
#define UWL_IMPLEMENTATION
#include <universal_wasm_loader.h>

#include <stdio.h>

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "math_50.wasm";

    char *err = NULL;
    uwl_module_t *m = uwl_import(path, NULL, 0, &err);
    if (!m) {
        fprintf(stderr, "load failed: %s\n", err ? err : "(unknown)");
        uwl_string_free(err);
        return 1;
    }

    uwl_val_t args[2] = { uwl_i32(3), uwl_i32(4) };
    uwl_val_t out;
    if (uwl_call(m, "add", args, 2, &out, &err) != 0) {
        fprintf(stderr, "call failed: %s\n", err ? err : "(unknown)");
        uwl_string_free(err);
        uwl_free(m);
        return 1;
    }

    printf("add(3, 4) = %d\n", uwl_as_i32(out));
    uwl_val_free(&out);
    uwl_free(m);
    return 0;
}
