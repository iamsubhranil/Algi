#pragma once

#include "lexer.h"
//#include "types.h"

typedef struct Expression Expression;

/*typedef struct{
    Token token;
} VariableExpression;
*/
typedef struct{
    union{
       double dval;
       int64_t ival;
       char *sval;
    };
} ConstantExpression;

typedef struct{
    Expression *left;
    Expression *right;
} BinaryExpression;

typedef struct{
    Expression *right;
} UnaryExpression;

typedef struct{
    Expression **args;
    uint64_t arity;
} CallExpression;

typedef struct{
    Expression *refer;
} ReferenceExpression;

typedef enum{
    EXPR_CONSTANT,
    EXPR_VARIABLE,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_CALL,
    EXPR_DEFINE, // Same as CALL, except used with Define/Container
    EXPR_REFERENCE
} ExpressionType;

struct Expression{
    ExpressionType type;
    Token token;
    int valueType;
    union{
        //VariableExpression varex;
        ConstantExpression consex;
        BinaryExpression binex;
        UnaryExpression unex;
        CallExpression calex;
        ReferenceExpression refex;
    };
};

// Functions

void expr_init();
Expression* expr_new(ExpressionType type);
void expr_dispose();
#ifdef DEBUG
void expr_print(Expression *exp, uint64_t printSpace);
void expr_print_space(uint64_t space);
#define print_space() expr_print_space(printSpace)
#endif
