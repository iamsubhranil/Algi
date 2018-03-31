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
    uint64_t hash;
    Context *context;
    ValueType valType;
    uint64_t arity; // for containers and routines
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

static uint64_t hash(const char *str, uint64_t length){
    uint64_t hash = 5381;
    uint64_t c = 0;
//    printf("\nHashing..\n");
    while (c < length)
        hash = ((hash << 5) + hash) + str[c++]; /* hash * 33 + c */
//    printf("\nInput String : [%s] Hash : %lu\n", str, hash);
    return hash;
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
                        if(right == VALUE_NUM || right == VALUE_INT)
                            return VALUE_NUM;
                        if(right == VALUE_GEN)
                            return VALUE_GEN;
                    case VALUE_INT:
                        if(right == VALUE_NUM || right == VALUE_INT || right == VALUE_GEN)
                            return right;
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
                        return VALUE_GEN;
                }
            } 
        case TOKEN_equal_equal:
        case TOKEN_not_equal:
            if(right == VALUE_BOOL && left == VALUE_BOOL){
                return VALUE_BOOL;
            }
        //case TOKEN_equal:
        case TOKEN_lesser:
        case TOKEN_lesser_equal:
        case TOKEN_greater:
        case TOKEN_greater_equal:
            switch(left){
                case VALUE_INT:
                case VALUE_GEN:
                case VALUE_NUM:
                    if(right == VALUE_NUM || right == VALUE_INT || right == VALUE_GEN)
                        return VALUE_BOOL;
                default:
                    goto bin_type_mismatch;
            }
            break;
        default:
            return VALUE_GEN;
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

static void context_register_declaration(Context *cont, Token name, DeclarationType dType, 
        ValueType vType, Context *dContext, uint64_t arity){
    Declaration d = {dType, name, hash(name.string, name.length), dContext, vType, arity};
    //dbg("Registering declaration for : ");
    //lexer_print_token(name, 0);
    //printf(" with valtype %s at context %p", valueNames[vType], cont);
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
    uint64_t h = hash(identifer.string, identifer.length);
    for(uint64_t i = 0;i < context->count;i++){
        Declaration d = context->declarations[i];
        if(h == d.hash){
            //dbg("Found variable : ");
            //lexer_print_token(identifer, 0);
            //printf(" with valtype %s at context %p", valueNames[d.valType], context);
            return &(context->declarations[i]);
        }
    }
    if(searchSuper){
        return context_get_decl(identifer, context->superContext, 1);
    }
    return NULL;
}

static Declaration* ref_get_decl(Context *context, Expression *ref){
    if(ref->type == EXPR_VARIABLE){
        return context_get_decl(ref->token, context, 0);
    }
    if(ref->type == EXPR_REFERENCE){
        Declaration *d = context_get_decl(ref->token, context, 0);
        if(d == NULL)
            return NULL;
        return ref_get_decl(d->context, ref->refex.refer);
    }
    return NULL;
}

static Declaration* expr_get_decl(Context *context, Expression *expr){
    if(expr->type == EXPR_UNARY){
        return expr_get_decl(context, expr->unex.right);
    }
    if(expr->type == EXPR_REFERENCE)
        return ref_get_decl(context, expr);
    else
        return context_get_decl(expr->token, context, 1);
}

static ValueType check_expression(Expression *e, Context *context, uint8_t searchSuper){
    if(e == NULL || context == NULL)
        return VALUE_UND;
    switch(e->type){
        case EXPR_CONSTANT:
            switch(e->token.type){
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
            {
                ValueType t = check_expression(e->unex.right, context, searchSuper);
#define TYPECAST(type, y) \
                case TOKEN_##type: \
                                if(t != VALUE_##y && t != VALUE_GEN){ \
                                    err("Bad typecast from %s to " #type, valueNames[t]); \
                                    token_print_source(e->token, 1); \
                                    hasErrors++; \
                                } \
                                e->valueType = VALUE_##y; \
                                break; 
                switch(e->token.type){
                    TYPECAST(Integer, INT)
                    TYPECAST(String, STR)
                    TYPECAST(Boolean, BOOL)
                    TYPECAST(Structure, STRUCT)
#undef TYPECAST
                    case TOKEN_Number:
                        if(t != VALUE_INT && t != VALUE_NUM && t != VALUE_GEN){
                            err("Bad typecast from %s to Number!", valueNames[t]);
                            token_print_source(e->token, 1);
                            hasErrors++; 
                        }
                        e->valueType = VALUE_NUM;
                        break;
                    case TOKEN_Type:
                        e->valueType = VALUE_INT;
                        break;
                    default:
                        e->valueType = t;
                        break;
                }
            }
            break;
        case EXPR_VARIABLE:
            {
                Declaration *d = context_get_decl(e->token, context, searchSuper);
                /*if(d != NULL){
                    dbg("Variable ");
                    lexer_print_token(e->varex.token, 0);
                    printf(" value type %s decl type %s", valueNames[e->valueType], valueNames[d->valType]);
                }*/
                if(d != NULL && d->declType == DECL_FUNC){
                    err("Unable to assign routine to variable!");
                    token_print_source(e->token, 1);
                    hasErrors++;
                    break;
                }
                if(e->valueType != VALUE_UND && d != NULL && (int)d->valType != e->valueType){
                    err("Redefining variable with different type!");
                    token_print_source(e->token, 1);
                    err("Previous definition was");
                    token_print_source(d->name, 1);
                    hasErrors++;
                    break;
                }
                if(e->valueType == VALUE_UND)
                    e->valueType = d == NULL ? VALUE_UND : d->valType;
                break;
            }
        case EXPR_REFERENCE:
            {
                Declaration *d = context_get_decl(e->token, context, 1);
                if(d == NULL){
                    err("No such variable : ");
                    lexer_print_token(e->token, 0);
                    token_print_source(e->token, 1);
                    hasErrors++;
                    e->valueType = VALUE_UND;
                    break;
                }
                else if(d->declType == DECL_FUNC){
                    err("Unable to deference routine : ");
                    lexer_print_token(e->token, 0);
                    token_print_source(e->token, 1);
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
                ValueType t = (ValueType)e->calex.args[i]->valueType;
                context_register_declaration(context, e->calex.args[i]->token, DECL_VAR, 
                        t == VALUE_UND ? VALUE_GEN : t, NULL, 0);
            }
            break;
            // context_register_declaration(context->superContext, e->calex.token, DECL_FUNC, VALUE_UND, context);
        case EXPR_CALL:
            {
                Declaration *d = context_get_decl(e->token, context, 1);
                if(d == NULL){
                    err("No such routine or container found : ");
                    lexer_print_token(e->token, 0);
                    hasErrors++;
                    e->valueType = VALUE_UND;
                    break;
                }
                else if(d->declType == DECL_FUNC || d->declType == DECL_CONTAINER){
                    if(d->arity != e->calex.arity ){
                        err("Arity mismatch for ");
                        lexer_print_token(d->name, 0);
                        err("Defined as");
                        token_print_source(d->name, 1);
                        err("Called as");
                        token_print_source(e->token, 1);
                        hasErrors++;
                        break;
                    }
                    for(uint64_t i = 0;i < d->arity;i++){
                        ValueType t = d->context->declarations[i].valType;
                        ValueType s = check_expression(e->calex.args[i], context, 1);
                        //dbg("ArgumentType : %s ParameterType : %s", valueNames[t], valueNames[s]);
                        if(t != s && s != VALUE_GEN && t != VALUE_GEN){
                            err("Argument type mismatch for ");
                            lexer_print_token(d->context->declarations[i].name, 0);
                            err("Defined as %s", valueNames[t]);
                            token_print_source(d->context->declarations[i].name, 1);
                            err("Parameter passed of type %s", valueNames[s]);
                            token_print_source(e->calex.args[i]->token, 1);
                            hasErrors++;
                        }
                    }
                }
                if(d != NULL && d->declType == DECL_CONTAINER)
                    e->valueType = VALUE_STRUCT;
                else
                    e->valueType = d->valType;
            }
            break;
        case EXPR_BINARY:
            {
                ValueType left = check_expression(e->binex.left, context, searchSuper);
                ValueType right = check_expression(e->binex.right, context, searchSuper);
                if(left == VALUE_UND || right == VALUE_UND){
                    err("Binary operation contains one or more undefined variables!");
                    token_print_source(e->token, 1);
                    hasErrors++;
                }
                e->valueType = compare_types(left, right, e->token);
            }
            break;
    }
    //expr_print(e, 5);
    return (ValueType)e->valueType;
}

static void type_check_internal(BlockStatement, Context*);

static Context* context_new(ContextType type, Context *superContext){
        Context *fContext = (Context *)malloc(sizeof(Context));
        register_pointer((void *)fContext, NULL);
        fContext->superContext = superContext;
        fContext->declarations = NULL;
        fContext->count = 0;
        fContext->type = type;
        return fContext;
}

#define reg_x(x, y, z) \
    static void register_##x(Statement *fStatement, Context *context, Context *fContext){ \
        DefineStatement ds = fStatement->defs; \
        context_register_declaration(context, ds.name->token, DECL_##y, VALUE_##z, fContext, ds.name->calex.arity); \
        type_check_internal(ds.body, fContext); \
    }

reg_x(function, FUNC, UND)

reg_x(container, CONTAINER, STRUCT)

    static void check_statement(Statement *s, Context *context){
        //stmt_print(s);
        switch(s->type){
            case STATEMENT_CONTAINER:
                {
                    Context *c = context_new(CONT_CONTAINER, context);
                    check_expression(s->defs.name, c, 0);
                    register_container(s, context, c);
                }
                break;
            case STATEMENT_DEFINE:
                {
                    Context *c = context_new(CONT_FUNC, context);
                    check_expression(s->defs.name, c, 0);
                    register_function(s, context, c);
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
                    /*if(s->sets.target->valueType != VALUE_UND){
                        ValueType check = check_expression(s->sets.target, context, 1);
                        if(s->sets.target->valueType != check){
                            err("Redefining variable with different type!");
                            token_print_source(s->token, 1);
                            hasErrors++;
                            break;
                        }
                    }*/
                    ValueType targetType = check_expression(s->sets.target, context, 1);
                    //dbg("TargetType : %d", (int)targetType);
                    //if(s->sets.target->valueType != VALUE_UND)
                    ValueType valueType = check_expression(s->sets.value, context, 1);
                    /*if(valueType != s->sets.target->valueType){
                      err("Reassigning variable with different type!");
                      token_print_source(s->token, 1);
                      hasErrors++;
                      }*/
                    if(targetType == VALUE_UND){
                        targetType = VALUE_GEN;
                        s->sets.target->valueType = VALUE_GEN;
                    }
                    Declaration *d = NULL;
                    if(s->sets.target->type == EXPR_VARIABLE){
                        d = context_get_decl(s->sets.target->token, context, 1);
                        if(d == NULL){
                            context_register_declaration(context, s->sets.target->token, DECL_VAR, targetType, NULL, 0);
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
                        err("Incompatible Set target : %s <= %s", valueNames[targetType], valueNames[valueType]);
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
