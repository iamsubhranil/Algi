#include "types.h"
#include "display.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static uint64_t hasErrors = 0;

typedef struct Context Context;

/*typedef struct{
  char *name;
  ValueType varType;
  } Variable;

  typedef struct{
  Context *context;
  } Container;

  typedef struct{
  Context *context;
  ValueType returnType;
  } Function;
  */
typedef enum{
    DECL_UND,
    DECL_VAR,
    DECL_CONTAINER,
    DECL_FUNC
} DeclarationType;

typedef struct{
    DeclarationType declType;
    Token name;
    Context *context;
    ValueType valType;
} Declaration;

//static const Declaration nullDeclaration = {DECL_UND, {NULL, NULL, 0, 0, 0, TOKEN_unknown}, NULL, VALUE_UND};

typedef enum{
    CONT_GLOBAL,
    CONT_FUNC,
    CONT_CONTAINER
} ContextType;

struct Context{
    ContextType type;
    Context *superContext;
    uint64_t id; // index of its declaration in its supercontext
    uint64_t count;
    Declaration *declarations;
};

static const char *valueNames[] = {
    "Undefined",
    "Boolean",
    "Structure",
    "Integer",
    "Number",
    "String",
    "Generic"
};

static void print_types(int count, ...){
    va_list types;
    va_start(types, count);
    while(count > 0){
        printf("%s", valueNames[va_arg(types, int)]);
        if(count > 1)
            printf(" and ");
        count--;
    }
    va_end(types);
}

static ValueType compare_types(ValueType left, ValueType right, Token op){
    //if(left == right)
    //    return left;
    //if(left == VALUE_GEN || right == VALUE_GEN)
    //    return VALUE_GEN;

    switch(op.type){
        // case TOKEN_unknown:
        //     {
        //         if()
        //     }
        case TOKEN_plus:
        case TOKEN_minus:
        case TOKEN_star:
        case TOKEN_backslash:
        case TOKEN_cap:
            {
                switch(left){
                    case VALUE_NUM:
                    case VALUE_INT:
                        if(right == VALUE_NUM)
                            return VALUE_NUM;
                        if(right == VALUE_INT)
                            return VALUE_INT;
                        if(right == VALUE_GEN)
                            return VALUE_GEN;
                        goto bin_type_mismatch;
                    case VALUE_GEN:
                        if(right == VALUE_NUM || right == VALUE_INT || right == VALUE_GEN)
                            return VALUE_GEN;
                    default:
bin_type_mismatch:
                        err("Bad operands for binary operator ");
                        lexer_print_token(op, 0);
                        printf(" : ");
                        print_types(2, left, right);
                        token_print_source(op, 1);
                        hasErrors++;
                        return VALUE_UND;
                }
            } 
        case TOKEN_equal:
        case TOKEN_equal_equal:
        case TOKEN_lesser:
        case TOKEN_lesser_equal:
        case TOKEN_greater:
        case TOKEN_greater_equal:
        case TOKEN_not_equal:
            switch(left){
                case VALUE_INT:
                case VALUE_GEN:
                case VALUE_NUM:
                    if(right == VALUE_GEN || right == VALUE_INT || right == VALUE_GEN)
                        return VALUE_BOOL;
                default:
                    goto bin_type_mismatch;
            }
        default:
            return VALUE_UND;
    }
}

static void** pointer_cache = NULL;
static uint64_t cache_index = 0;

static void register_pointer(void *n, void *o){
    if(o != NULL){
        for(uint64_t i = 0;i < cache_index;i++){
            if(pointer_cache[i] == o){
                pointer_cache[i] = n;
                return;
            }
        }
    }
    cache_index++;
    pointer_cache = (void **)realloc(pointer_cache, sizeof(void *) * cache_index);
    pointer_cache[cache_index - 1] = n;
}

static void context_register_declaration(Context *cont, Token name, DeclarationType dType, ValueType vType, Context *dContext){
    Declaration d = {dType, name, dContext, vType};
    //dbg("Registering declaration for : ");
    //lexer_print_token(name, 0);
    //printf(" with valtype %d at context %p", vType, cont);
    if(dContext != NULL)
        dContext->id = cont->count;
    cont->count++;
    Declaration *bak = cont->declarations;
    cont->declarations = (Declaration *)realloc(cont->declarations, sizeof(Declaration) * cont->count);
    cont->declarations[cont->count - 1] = d;
    register_pointer(cont->declarations, bak);
}

static Declaration* context_get_decl(Token identifer, Context *context, uint8_t searchSuper){
    if(context == NULL){
        //err("No such variable found in the present context!");
        //token_print_source(identifer, 1);
        //hasErrors++;
        //dbg("Not found variable : ");
        //lexer_print_token(identifer, 0);
        return NULL;
    }
    //dbg("Searching variable : ");
    //lexer_print_token(identifer, 0);
    for(uint64_t i = 0;i < context->count;i++){
        Declaration d = context->declarations[i];
        if(strncmp(d.name.string, identifer.string, identifer.length) == 0){
            //dbg("Found variable : ");
            //lexer_print_token(identifer, 0);
            //dbg("ValType : %d at context %p", d.valType, context);
            return &(context->declarations[i]);
        }
    }
    if(searchSuper){
        return context_get_decl(identifer, context->superContext, 1);
    }
    return NULL;
}

static ValueType context_get_type(Token identifer, Context *context, uint8_t searchSuper){
    Declaration *d = context_get_decl(identifer, context, searchSuper);
    return d == NULL ? VALUE_UND : d->valType;
}

static Declaration* ref_get_decl(Context *context, Expression *ref){
    if(ref->type == EXPR_VARIABLE){
        return context_get_decl(ref->varex.token, context, 0);
    }
    if(ref->type == EXPR_REFERENCE){
        Declaration *d = context_get_decl(ref->refex.token, context, 0);
        if(d == NULL)
            return NULL;
        return ref_get_decl(d->context, ref->refex.refer);
    }
    return NULL;
}

static Declaration* expr_get_decl(Context *context, Expression *expr){
    if(expr->type == EXPR_REFERENCE)
        return ref_get_decl(context, expr);
    else
        return context_get_decl(expr->calex.token, context, 1);
}

static ValueType check_expression(Expression *e, Context *context, uint8_t searchSuper){
    if(e == NULL)
        return VALUE_UND;
    switch(e->type){
        case EXPR_CONSTANT:
            switch(e->consex.token.type){
                case TOKEN_number:
                    e->valueType = VALUE_NUM;
                    break;
                case TOKEN_integer:
                    e->valueType = VALUE_INT;
                    break;
                case TOKEN_string:
                    e->valueType = VALUE_STR;
                    break;
                case TOKEN_True:
                case TOKEN_False:
                    e->valueType = VALUE_BOOL;
                    break;
                case TOKEN_Null:
                    e->valueType = VALUE_GEN;
                    break;
                default:
                    e->valueType = VALUE_UND;
                    break;
            }
            break;
        case EXPR_UNARY:
            e->valueType = check_expression(e->unex.right, context, searchSuper);
            break;
        case EXPR_VARIABLE:
            e->valueType = context_get_type(e->varex.token, context, searchSuper);
            break;
        case EXPR_REFERENCE:
            {
                Declaration *d = context_get_decl(e->refex.token, context, 1);
                if(d == NULL){
                    err("No such variable : ");
                    lexer_print_token(e->refex.token, 0);
                    token_print_source(e->refex.token, 1);
                    hasErrors++;
                    e->valueType = VALUE_UND;
                    break;
                }
                else if(d->declType == DECL_FUNC){
                    err("Unable to deference routine : ");
                    lexer_print_token(e->refex.token, 0);
                    token_print_source(e->refex.token, 1);
                    hasErrors++;
                    e->valueType = VALUE_UND;
                    break;
                }
                else if(d->valType == VALUE_GEN){
                    e->valueType = VALUE_GEN;
                    break;
                }
                else{
                    e->valueType = check_expression(e->refex.refer, 
                            d->context, 
                            0);
                }
            }
            break;
        case EXPR_DEFINE:
            for(uint64_t i = 0;i < e->calex.arity;i++){
                context_register_declaration(context, e->calex.args[i]->varex.token, DECL_VAR, VALUE_GEN, NULL);
            }
            break;
            // context_register_declaration(context->superContext, e->calex.token, DECL_FUNC, VALUE_UND, context);
        case EXPR_CALL:
            {
                Declaration *d = context_get_decl(e->calex.token, context, 1);
                if(d == NULL){
                    err("No such routine or container found : ");
                    lexer_print_token(e->calex.token, 0);
                    hasErrors++;
                    e->valueType = VALUE_UND;
                }
                else if(d->declType == DECL_CONTAINER)
                    e->valueType = VALUE_STRUCT;
                else
                    e->valueType = d->valType;
            }
            break;
        case EXPR_BINARY:
            e->valueType = compare_types(check_expression(e->binex.left, context, searchSuper),
                    check_expression(e->binex.right, context, searchSuper), e->binex.token);
            break;
    }
    //expr_print(e, 5);
    return (ValueType)e->valueType;
}

static void type_check_internal(BlockStatement, Context*);

#define reg_x(x, y, z) \
    static void register_##x(Statement *fStatement, Context *context){ \
        Context *fContext = (Context *)malloc(sizeof(Context)); \
        register_pointer((void *)fContext, NULL); \
        fContext->superContext = context; \
        fContext->declarations = NULL; \
        fContext->count = 0; \
        fContext->type = CONT_##y; \
        DefineStatement ds = fStatement->defs; \
        context_register_declaration(context, ds.name->varex.token, DECL_##y, VALUE_##z, fContext); \
        type_check_internal(ds.body, fContext); \
    }

reg_x(function, FUNC, UND)

reg_x(container, CONTAINER, STRUCT)

    static void check_statement(Statement *s, Context *context){
        //stmt_print(s);
        switch(s->type){
            case STATEMENT_CONTAINER:
                check_expression(s->defs.name, context, 0);
                register_container(s, context);
                break;
            case STATEMENT_DEFINE:
                {
                    check_expression(s->defs.name, context, 0);
                    register_function(s, context);
                    // type_check_internal(s->defs.body, context);
                }
                break;
            case STATEMENT_RETURN:
                {
                    if(context->type == CONT_FUNC){
                        ValueType *t = &(context->superContext->declarations[context->id].valType);
                        if(*t == VALUE_UND || *t != VALUE_GEN){
                            ValueType presentType = check_expression(s->rets.value, context, 1);
                            if(*t != presentType){
                                *t = VALUE_GEN;
                            }
                        }
                    }
                    else{
                        err("Return statement outside of a routine!");
                        token_print_source(s->token, 1);
                        hasErrors++;
                    }
                    break;
                }
            case STATEMENT_SET:
                {
                    // dbg("s->sets.target->valueType : %d", (int)s->sets.target->valueType);
                    ValueType targetType = (ValueType)(s->sets.target->valueType == VALUE_UND 
                            ? check_expression(s->sets.target, context, 1)
                            : s->sets.target->valueType);
                    //dbg("TargetType : %d", (int)targetType);
                    ValueType valueType = check_expression(s->sets.value, context, 1);
                    if(targetType == VALUE_UND){
                        targetType = VALUE_GEN;
                        s->sets.target->valueType = VALUE_GEN;
                    }
                    Declaration *d = NULL;
                    if(s->sets.target->type == EXPR_VARIABLE){
                        d = context_get_decl(s->sets.target->varex.token, context, 1);
                        if(d == NULL){
                            context_register_declaration(context, s->sets.target->varex.token, DECL_VAR, targetType, NULL);
                            d = &(context->declarations[context->count - 1]);
                        }
                    }
                    else{
                        d = ref_get_decl(context, s->sets.target);
                        if(d == NULL && s->sets.target->valueType == VALUE_GEN){
                            valueType = VALUE_GEN;
                        }
                        else if(d == NULL){
                            err("No such member found!");
                            token_print_source(s->token, 1);
                            hasErrors++;
                            break;
                        }
                    }
                    if(targetType == valueType ||
                            targetType == VALUE_GEN){
                        if(valueType == VALUE_STRUCT){
                            //dbg("Declaration : %p", d);
                            Declaration *structD = expr_get_decl(context, s->sets.value);
                            //dbg("StructD : %p", structD);
                            if(structD == NULL){
                                err("No such routine or container!");
                                token_print_source(s->token, 1);
                                hasErrors++;
                                break;
                            }
                            else
                                d->context = structD->context;
                        }
                        break;
                    }
                    else if(valueType == VALUE_GEN)
                        break;
                    else if(valueType == VALUE_INT && targetType == VALUE_NUM)
                        break;
                    /*else if((targetType == VALUE_NUM && (valueType == VALUE_INT || 
                                     valueType == VALUE_GEN)))
                            break;
                    else if(targetType == VALUE_INT && valueType == VALUE_GEN)
                        break;
                    else if(targetType == VALUE_STR && valueType == VALUE_GEN)
                        break;*/
                    else{
                        //dbg("TargetType : %d\n", (int)targetType);
                        err("Incompatible Set target : ");
                        print_types(2, targetType, valueType);
                        // DONE: token_print_source
                        token_print_source(s->token, 1);
                        hasErrors++;
                    }
                    break;
                }
            case STATEMENT_CALL:
                // TODO: argument type checking
                break;
            case STATEMENT_DO:
            case STATEMENT_WHILE:
                {
                    ValueType t = check_expression(s->dos.condition, context, 1);
                    if(t != VALUE_BOOL && t != VALUE_GEN){
                        err("Expected Boolean type as condition!");
                        //DONE: token_print_source
                        token_print_source(s->token, 1);
                        hasErrors++;
                        break;
                    }
                    type_check_internal(s->dos.statements, context);
                    break;
                }
            case STATEMENT_IF:
                {
                    ValueType t = check_expression(s->ifs.condition, context, 1);
                    if(t != VALUE_BOOL && t != VALUE_GEN){
                        err("Expected Boolean type as condition!");
                        //DONE: token_print_source
                        token_print_source(s->token, 1);
                        hasErrors++;
                        break;
                    }
                    if(s->ifs.elseIf != NULL)
                        check_statement(s->ifs.elseIf, context);
                    if(s->ifs.elseBlock.count > 0)
                        type_check_internal(s->ifs.elseBlock, context);
                    break;
                }
            case STATEMENT_PRINT:
                {
                    for(uint64_t i = 0;i < s->prints.count;i++)
                        check_expression(s->prints.args[i], context, 1);
                    break;
                }
        }
    }

static Context globalContext = {CONT_GLOBAL, NULL, 0, 0, NULL};

static void type_check_internal(BlockStatement statements, Context *context){
    for(uint64_t i = 0;i < statements.count;i++){
        switch(statements.statements[i]->type){
            case STATEMENT_CONTAINER:
                if(context->type == CONT_FUNC){
                    err("Unable to declare a container inside a routine ");
                    // DONE: print both super and child names
                    lexer_print_token(context->superContext->declarations[context->id].name, 0);
                    token_print_source(statements.statements[i]->token, 1);
                    hasErrors++;
                    break;
                }
                if(context->type == CONT_CONTAINER){
                    err("Unable to nest container inside ");
                    // DONE: print both super and child names
                    lexer_print_token(context->superContext->declarations[context->id].name, 0);
                    token_print_source(statements.statements[i]->token, 1);
                    hasErrors++;
                    break;
                }
                //register_container(statements.statements[i], context);
                goto cst;
            case STATEMENT_DEFINE:
                if(context->type == CONT_FUNC){
                    err("Unable to nest routine inside ");
                    // DONE: print both super and child names
                    lexer_print_token(context->superContext->declarations[context->id].name, 0);
                    token_print_source(statements.statements[i]->token, 1);
                    hasErrors++;
                    break;
                }
                //register_function(statements.statements[i], context);
            default:
cst:
                check_statement(statements.statements[i], context);
                break;
        }
    }
   // for(uint64_t i = 0;i < statements.count;i++){
   //     check_statement(statements.statements[i], context);
   // }
}

void type_check(BlockStatement statements){
    type_check_internal(statements, &globalContext);
}

void type_dispose(){
    for(uint64_t i = 0;i < cache_index;i++){
        free(pointer_cache[i]);
    }
    free(pointer_cache);
}

uint64_t type_has_errors(){
    return hasErrors;
}
