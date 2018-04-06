#pragma once

#include <stdint.h>
#include <stdarg.h>

typedef struct{
    int type; // Based on ValueType
    union{ // A single 8 byte value will suffice for all of them
        char *string;
        int64_t inumber;
        double dnumber;
        int8_t boolean;
    };
} AlgiGenericValue;

// Update the counter and the array whenever a new
// runtime function is added
#define ALGI_RUNTIME_FUNCTION_COUNT 4

typedef struct{
    const char* name;
    void* address;
} AlgiRuntimeFunction;

extern int64_t __algi_to_int(int type, ...);

extern double __algi_to_double(int type, ...);

extern char* __algi_to_string(int type, ...);

extern int8_t __algi_to_boolean(int type, ...);

#define ALGI_DECLARE_RUNTIME_FUNCTION(name) \
    {"__" #name, (void*)&__##name}

static AlgiRuntimeFunction algi_runtime_functions[] = {
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_to_int),
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_to_double),
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_to_string),
    ALGI_DECLARE_RUNTIME_FUNCTION(algi_to_boolean)
};
