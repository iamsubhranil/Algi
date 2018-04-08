#pragma once

#include <stdint.h>
#include <stdarg.h>

typedef struct{
    int8_t type; // Based on ValueType
    union{ // A single 8 byte value will suffice for all of them
        char *string;
        int64_t inumber;
        double dnumber;
        int8_t boolean;
    };
} AlgiGenericValue;

// Update the counter and the array whenever a new
// runtime function is added
#define ALGI_RUNTIME_FUNCTION_COUNT 6

typedef struct{
    const char* name;
    void* address;
} AlgiRuntimeFunction;

extern int64_t __algi_to_int(int32_t type, ...);

extern double __algi_to_double(int32_t type, ...);

extern char* __algi_to_string(int32_t type, ...);

extern int8_t __algi_to_boolean(int32_t type, ...);

extern void __algi_generic_print(AlgiGenericValue value);

extern void __algi_generic_store(AlgiGenericValue *value, int32_t storeType, ...);

typedef enum{
    ALGI_TO_INT = 0,
    ALGI_TO_DOUBLE,
    ALGI_TO_STRING,
    ALGI_TO_BOOLEAN,
    ALGI_GENERIC_PRINT,
    ALGI_GENERIC_STORE
} AlgiFunctionIndex;
    
#define ALGI_DECLARE_RUNTIME_FUNCTION(name) \
    {"__" #name, (void*)&__##name}

static AlgiRuntimeFunction algi_runtime_functions[] = {
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_to_int),
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_to_double),
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_to_string),
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_to_boolean),
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_generic_print),
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_generic_store)
};

#undef ALGI_DECLARE_RUNTIME_FUNCTION
