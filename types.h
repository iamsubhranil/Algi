#pragma once

#include "stmt.h"
#include <stdbool.h>

typedef enum{
    VALUE_UND, // A variable or function whose value is not yet determined
    VALUE_BOOL,
    VALUE_STRUCT,
    VALUE_INT,
    VALUE_NUM,
    VALUE_STR,
    VALUE_GEN
} ValueType;

//typedef struct Container Container;

/*
typedef struct{
    ValueType type;
    union{
        bool bval;
        Container cval;
        int64_t ival;
        double nval;
        char *strval;
    };
} Value;
*/
void type_check(BlockStatement statements);
uint64_t type_has_errors();
void type_dispose();
