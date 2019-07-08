#include <stdio.h>
#include "rust-build/src/wasmer-runtime-c-api/lib/runtime-c-api/wasmer.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

static int print_str_called = false;

// Host function that will be imported into the Web Assembly Instance
void print_str(wasmer_instance_context_t *ctx, int32_t ptr, int32_t len)
{
    print_str_called = true;
    const wasmer_memory_t *memory = wasmer_instance_context_memory(ctx, 0);
    uint8_t *mem_bytes = wasmer_memory_data(memory);
    printf("%.*s", len, mem_bytes + ptr);
}

// Use the last_error API to retrieve error messages
void print_wasmer_error()
{
    int error_len = wasmer_last_error_length();
    printf("Error len: `%d`\n", error_len);
    char *error_str = malloc(error_len);
    wasmer_last_error_message(error_str, error_len);
    printf("Error str: `%s`\n", error_str);
}

int main()
{
    // Create a new func to hold the parameter and signature
    // of our `print_str` host function
    wasmer_value_tag params_sig[] = {WASM_I32, WASM_I32};
    wasmer_value_tag returns_sig[] = {};
    wasmer_import_func_t *func = wasmer_import_func_new((void (*)(void *)) print_str, params_sig, 2, returns_sig, 0);

    // Create module name for our imports
    // represented in bytes for UTF-8 compatability
    const char *module_name = "env";
    wasmer_byte_array module_name_bytes = { .bytes = (const uint8_t *) module_name,
                                            .bytes_len = strlen(module_name) };

    // Define a function import
    const char *import_name = "_print_str";
    wasmer_byte_array import_name_bytes = { .bytes = (const uint8_t *) import_name,
                                            .bytes_len = strlen(import_name) };
    wasmer_import_t func_import = { .module_name = module_name_bytes,
                                    .import_name = import_name_bytes,
                                    .tag = WASM_FUNCTION,
                                    .value.func = func };

    // Define a memory import
    const char *import_memory_name = "memory";
    wasmer_byte_array import_memory_name_bytes = { .bytes = (const uint8_t *) import_memory_name,
                                                   .bytes_len = strlen(import_memory_name) };
    wasmer_import_t memory_import = { .module_name = module_name_bytes,
                                      .import_name = import_memory_name_bytes,
                                      .tag = WASM_MEMORY };
    wasmer_memory_t *memory = NULL;
    wasmer_limit_option_t max = { .has_some = true,
                                  .some = 256 };
    wasmer_limits_t descriptor = { .min = 256,
                                   .max = max };

    wasmer_result_t memory_result = wasmer_memory_new(&memory, descriptor);
    if (memory_result != WASMER_OK)
    {
        print_wasmer_error();
    }
    memory_import.value.memory = memory;

    // Define a global import
    const char *import_global_name = "__memory_base";
    wasmer_byte_array import_global_name_bytes = { .bytes = (const uint8_t *) import_global_name,
                                                   .bytes_len = strlen(import_global_name) };
    wasmer_import_t global_import = { .module_name = module_name_bytes,
                                      .import_name = import_global_name_bytes,
                                      .tag = WASM_GLOBAL };

    wasmer_value_t val = { .tag = WASM_I32,
                           .value.I32 = 1024 };
    wasmer_global_t *global = wasmer_global_new(val, false);
    global_import.value.global = global;

    // Define an array containing our imports
    wasmer_import_t imports[] = {func_import, global_import, memory_import};

    // Read the wasm file bytes
    FILE *file = fopen("wasm-sample-app/target.wasm", "r");
    fseek(file, 0, SEEK_END);
    long len = ftell(file);
    uint8_t *bytes = malloc(len);
    fseek(file, 0, SEEK_SET);
    fread(bytes, 1, len, file);
    fclose(file);

    // Creates a WebAssembly Instance from wasm bytes and imports
    wasmer_instance_t *instance = NULL;
    wasmer_result_t compile_result = wasmer_instantiate(&instance, bytes, len, imports, 3);
    printf("Compile result:  %d\n", compile_result);

    if (compile_result != WASMER_OK)
    {
        print_wasmer_error();
    }

    assert(compile_result == WASMER_OK);

    // Call the exported "hello_wasm" function of our instance
    wasmer_value_t params[] = {};
    wasmer_value_t result_one;
    wasmer_value_t results[] = {result_one};
    wasmer_result_t call_result = wasmer_instance_call(instance, "_hello_wasm", params, 0, results, 1);
    printf("\nCall result:  %d\n", call_result);
    assert(call_result == WASMER_OK);
    assert(print_str_called);

    // Use *_destroy methods to cleanup as specified in the header documentation
    wasmer_import_func_destroy(func);
    wasmer_global_destroy(global);
    wasmer_memory_destroy(memory);
    wasmer_instance_destroy(instance);

    return 0;
}
