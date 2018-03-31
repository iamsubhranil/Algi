#pragma once

#include "expr.h"

typedef struct Statement Statement;

typedef struct{
    Expression *target;
    Expression *value;
} SetStatement;

typedef struct{
    uint64_t count;
    Expression **args;
} PrintStatement;

typedef struct{
    uint64_t count;
    Statement **statements;
} BlockStatement;

typedef struct IfStatement{
    Expression *condition;
    BlockStatement thenBlock;
    Statement *elseIf;
    BlockStatement elseBlock;
} IfStatement;

typedef struct{
    Expression *condition;
    BlockStatement statements;
} WhileStatement;

typedef struct{
    Expression *condition;
    BlockStatement statements;
} DoStatement;

typedef struct{
    Expression *name;
    BlockStatement body;
} DefineStatement;

typedef struct{
    Expression *value;
} ReturnStatement;

typedef struct{
    Expression *name;
    BlockStatement body;
} ContainerStatement;

typedef struct{
    Expression *callExpression;
} CallStatement;

typedef enum{
    STATEMENT_SET,
    STATEMENT_PRINT,
    STATEMENT_IF,
    STATEMENT_WHILE,
    STATEMENT_DO,
    STATEMENT_RETURN,
    STATEMENT_DEFINE,
    STATEMENT_CONTAINER,
    STATEMENT_CALL
} StatementType;

struct Statement{
    StatementType type;
    Token token;
    union{
        SetStatement sets;
        PrintStatement prints;
        IfStatement ifs;
        WhileStatement whiles;
        DoStatement dos;
        ReturnStatement rets;
        DefineStatement defs;
        ContainerStatement cons;
        CallStatement calls;
    };
};

void stmt_init();
Statement* stmt_new(StatementType type);
void blockstmt_dispose(BlockStatement s);
void stmt_dispose();
#ifdef DEBUG
void blockstmt_print(BlockStatement st);
void stmt_print(Statement *st);
#endif
