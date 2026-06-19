/* Minimal consumer of universal-wasm-loader-c installed via vcpkg.
 * Usage: consumer path/to/math_50.wasm  →  prints add(3, 4) = 7 */
#define UWL_IMPLEMENTATION
#include <universal_wasm_loader.h>

#include <stdio.h>

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "math_50.wasm";

    uwl_module_t *m = uwl_import(path, NULL, 0, NULL);
    if (!m) {
        fprintf(stderr, "load failed: %s\n", uwl_last_error());
        return 1;
    }

    int r = uwl_call_i32(m, "add", 3, 4);
    if (uwl_last_error()) {
        fprintf(stderr, "call failed: %s\n", uwl_last_error());
        uwl_free(m);
        return 1;
    }

    printf("add(3, 4) = %d\n", r);
    uwl_free(m);
    return 0;
}
