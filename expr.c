#include <stdlib.h>

#include "expr.h"
#include "display.h"

static Expression **cache = NULL;
static uint64_t cache_pointer = 0, cache_capacity = 0;

static void cache_inc(uint64_t by){
    cache_capacity += by;
    cache = (Expression **)realloc(cache, sizeof(Expression *) * cache_capacity);
}

void expr_init(){
    cache_inc(10);
}

static Expression* expr_new2(){
    Expression* expr = (Expression *)malloc(sizeof(Expression));
    if(!expr){
        err("Unable to allocate memory for syntax tree!");
        abort();
    }
    if(cache_pointer >= cache_capacity){
        cache_inc(2 * cache_capacity);
    }
    cache[cache_pointer] = expr;
    cache_pointer++;
    return expr;
}

Expression* expr_new(ExpressionType type){
   Expression* e = expr_new2();
   e->type = type;
   e->valueType = 0;
   return e;
}

/*
void expr_dispose2(Expression *exp){
    switch(exp->type){
        case EXPR_VARIABLE:
        case EXPR_CONSTANT:
            free(exp);
            break;
        case EXPR_BINARY:
            expr_dispose2(exp->binex.left);
            expr_dispose2(exp->binex.right);
            free(exp);
            break;
        case EXPR_UNARY:
            expr_dispose2(exp->unex.right);
            free(exp);
            break;
        case EXPR_CALL:
            for(uint64_t i = 0;i < exp->calex.arity;i++)
                expr_dispose2(exp->calex.args[i]);
            free(exp);
            break;
        case EXPR_REFERENCE:
            expr_dispose2(exp->refex.refer);
            free(exp);
            break;
    }
}*/

void expr_dispose(){
    for(uint64_t i = 0;i < cache_pointer;i++){
      //  if(cache[i] != NULL)
      if(cache[i]->type == EXPR_CALL){
            free(cache[i]->calex.args);
      }
            free(cache[i]);
    }
    free(cache);
}

#ifdef DEBUG

#include <stdio.h>
#include <inttypes.h>

static const char *exprString[] = {
    "ConstantExpression",
    "VariableExpression",
    "BinaryExpression",
    "UnaryExpression",
    "CallExpression",
    "DefineExpression",
    "ReferenceExpression"
} ;

//"|-- ReferenceExpression"
//"     |-- VariableExpression
//"          |-- 
static uint64_t printSpace = 0;

void expr_print_space(uint64_t sp){
    for(uint64_t i = 0;i < sp;i++)
        printf("%s", i%5 == 0 ? "|" : " ");
}

static void expr_print2(Expression *expr){
    printf("\n");
    if(printSpace > 0){
        print_space();
        if(expr == NULL){
            printf("|-- BadExpression!");
            return;
        }
        else
            printf("|-- %s", exprString[expr->type]);
        if(expr->valueType != 0)
            printf(" (Type %d)", expr->valueType);
    }
    uint64_t bak = printSpace;
    switch(expr->type){
        case EXPR_CONSTANT:
            printf("\n");
            printSpace += 5;
            print_space();
            printf("|-- ");
            switch(expr->consex.token.type){
                case TOKEN_number : printf("NumericConstant : %g", expr->consex.dval);
                                    break;
                case TOKEN_integer: printf("IntegerConstant : %" PRId64, expr->consex.ival);
                                    break;
                case TOKEN_string : printf("StringConstant : ");
                                    lexer_print_token(expr->consex.token, 0);
                                    break;
                case TOKEN_True: printf("BooleanConstant : True");
                                 break;
                case TOKEN_False: printf("BooleanConstant : False");
                                  break;
                case TOKEN_Null: printf("NullConstant : Null");
                                 break;
                default: break;
            }
            break;
        case EXPR_VARIABLE:
            printf(" : ");
            lexer_print_token(expr->varex.token, 0);
            break;
        case EXPR_UNARY:
            printf(" : ");
            lexer_print_token(expr->unex.token, 0);
            printSpace += 5;
            expr_print2(expr->unex.right);
            break;
        case EXPR_BINARY:
            printf(" : ");
            lexer_print_token(expr->binex.token, 0);
            printSpace += 5;
            printf("\n");
            print_space();
            printf("|-- LeftOperand");
            printSpace += 5;
            expr_print2(expr->binex.left);
            printSpace -= 5;
            printf("\n");
            print_space();
            printf("|-- RightOperand");
            printSpace += 5;
            expr_print2(expr->binex.right);
            break;
        case EXPR_DEFINE:
        case EXPR_CALL:
            printf("\n");
            printSpace += 5;
            print_space();
            printf("|-- Callee : ");
            lexer_print_token(expr->calex.token, 0);
            printf("\n");
            print_space();
            printf("|-- Arguments");
            printSpace += 5;
            for(uint64_t i = 0;i < expr->calex.arity;i++)
                expr_print2(expr->calex.args[i]);
            break;
        case EXPR_REFERENCE:
            printf(" : ");
            lexer_print_token(expr->refex.token, 0);
            printSpace += 5;
            expr_print2(expr->refex.refer);
            break;
    }
    printSpace = bak;
}

void expr_print(Expression *exp, uint64_t printS){
    printSpace = printS;
    expr_print2(exp);
}

#endif
