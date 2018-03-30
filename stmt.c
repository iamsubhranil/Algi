#include <stdlib.h>

#include "stmt.h"
#include "display.h"

static Statement **cache = NULL;
static uint64_t cache_pointer = 0, cache_capacity = 0;

static void cache_inc(uint64_t by){
    cache_capacity += by;
    cache = (Statement **)realloc(cache, sizeof(Statement *) * cache_capacity);
}

void stmt_init(){
    cache_inc(10);
}

static Statement* stmt_new2(){
    Statement* stmt = (Statement *)malloc(sizeof(Statement));
    if(!stmt){
        err("Unable to allocate memory for syntax tree!");
        abort();
    }
    if(cache_pointer >= cache_capacity){
        cache_inc(2 * cache_capacity);
    }
    cache[cache_pointer] = stmt;
    cache_pointer++;
    return stmt;
}

Statement* stmt_new(StatementType type){
   Statement* e = stmt_new2();
   e->type = type;
   return e;
}


//static void stmt_dispose2(Statement *stmt);

void blockstmt_dispose(BlockStatement s){
    free(s.statements);
}
/*
static void stmt_dispose2(Statement *stmt){
    switch(stmt->type){
        case STATEMENT_CALL:
            expr_dispose2(stmt->calls.callExpression);
            break;
        case STATEMENT_SET:
            expr_dispose2(stmt->sets.target);
            expr_dispose2(stmt->sets.value);
            //free(stmt);
            break;
        case STATEMENT_PRINT:
            for(uint64_t i = 0;i < stmt->prints.count;i++)
                expr_dispose2(stmt->prints.args[i]);
            //free(stmt);
            break;
        case STATEMENT_RETURN:
            expr_dispose2(stmt->rets.value);
            //free(stmt);
            break;
        case STATEMENT_WHILE:
        case STATEMENT_DO:
            expr_dispose2(stmt->whiles.condition);
            blockstmt_dispose(stmt->whiles.statements);
            //free(stmt);
            break;
        case STATEMENT_CONTAINER:
        case STATEMENT_DEFINE:
            expr_dispose2(stmt->cons.name);
            blockstmt_dispose(stmt->cons.body);
            //free(stmt);
            break;
        case STATEMENT_IF:{
            expr_dispose2(stmt->ifs.condition);
            if(stmt->ifs.elseBlock != NULL)
                stmt_dispose2(stmt->ifs.elseBlock);
            //free(stmt);
            break;
                          }
    }
}
*/
void stmt_dispose(){
    for(uint64_t i = 0;i < cache_pointer;i++){
        //if(cache[i] != NULL)
            //stmt_dispose2(cache[i]);
        switch(cache[i]->type){
            case STATEMENT_IF:
                blockstmt_dispose(cache[i]->ifs.thenBlock);
                blockstmt_dispose(cache[i]->ifs.elseBlock);
                break;
            case STATEMENT_WHILE:
            case STATEMENT_DO:
                blockstmt_dispose(cache[i]->whiles.statements);
                break;
            case STATEMENT_DEFINE:
            case STATEMENT_CONTAINER:
                blockstmt_dispose(cache[i]->defs.body);
                break;
            case STATEMENT_PRINT:
                free(cache[i]->prints.args);
            default:
                break;
        }
        free(cache[i]);
    }
    free(cache);
}

#ifdef DEBUG

#include <stdio.h>
#include <inttypes.h>

static const char *stmtString[] = {
    "SetStatement",
    "PrintStatement",
    "IfStatement",
    "WhileStatement",
    "DoStatement",
    "ReturnStatement",
    "DefineStatement",
    "ContainerStatement"
} ;

//"|-- ReferenceStatement"
//"     |-- VariableStatement
//"          |-- 
static uint64_t printSpace = 0;

static void print_space(){
    for(uint64_t i = 0;i < printSpace;i++)
        printf("%c", i%5 == 0?'|':' ');
}

static void stmt_print2(Statement *stmt){
    printf("\n");
    uint64_t bak = printSpace;
    if(printSpace > 0){
        print_space();
        printf("|-- %s", stmtString[stmt->type]);
        printf("\n");
        printSpace += 5;
        print_space();
    }
    switch(stmt->type){
        case STATEMENT_CALL:
            printf("|-- Callee");
            expr_print(stmt->calls.callExpression, printSpace+5);
            break;
        case STATEMENT_SET:
            printf("|-- Target");
            expr_print(stmt->sets.target, printSpace+5);
            printf("\n");
            print_space();
            printf("|-- Value");
            expr_print(stmt->sets.value, printSpace+5);
            break;
        case STATEMENT_WHILE:
        case STATEMENT_DO:
            printf("|-- Condition");
            expr_print(stmt->dos.condition, printSpace + 5);
            printf("\n");
            print_space();
            printf("|-- Statements");
            printSpace += 5;
            blockstmt_print(stmt->dos.statements);
            break;
        case STATEMENT_DEFINE:
        case STATEMENT_CONTAINER:
            printf("|-- Declaration");
            expr_print(stmt->cons.name, printSpace + 5);
            printf("\n");
            print_space();
            printf("|-- Body");
            printSpace += 5;
            blockstmt_print(stmt->cons.body);
            break;
        case STATEMENT_PRINT:
            for(uint64_t i = 0;i < stmt->prints.count;i++){
                printf("|-- Argument%ld", (i+1));
                expr_print(stmt->prints.args[i], printSpace + 5);
                printf("\n");
                print_space();
            }
            break;
        case STATEMENT_RETURN:
            printf("|-- Value");
            expr_print(stmt->rets.value, printSpace + 5);
            break;
        case STATEMENT_IF:
            printf("|-- Condition");
            expr_print(stmt->ifs.condition, printSpace + 5);
            printf("\n");
            print_space();
            printf("|-- ThenBlock");
            printSpace += 5;
            blockstmt_print(stmt->ifs.thenBlock);
            printSpace -= 5;
            if(stmt->ifs.elseIf){
                printf("\n");
                print_space();
                printf("|-- ElseIfBlock");
                printSpace += 5;
                stmt_print2(stmt->ifs.elseIf);
                printSpace -= 5;
            }
            if(stmt->ifs.elseBlock.count > 0){
                printf("\n");
                print_space();
                printf("|-- ElseBlock");
                printSpace += 5;
                blockstmt_print(stmt->ifs.elseBlock);
            }
            break;
    }
    printSpace = bak;
}

void blockstmt_print(BlockStatement st){
    for(uint64_t i = 0;i < st.count;i++){
        printf("\n");
        print_space();
        printf("|-- Statement%ld", (i+1));
        printSpace += 5;
        stmt_print2(st.statements[i]);
        printSpace -= 5;
    }
}

void stmt_print(Statement *stmt){
    printSpace = 0;
    stmt_print2(stmt);
}

#endif
