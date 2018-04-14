#include "parser.h"
#include "display.h"
#include "types.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

static uint32_t present = 0, length = 0, hasErrors = 0;

/* Parser core
 * ===========
 */
static TokenList list;
static size_t presentLine = 0;
static Token presentToken, previousToken;

static uint8_t ueofShown = 0;

static void advance(){
    if(present < length){
        previousToken = presentToken;
        present++;
        presentToken = list.tokens[present];
        presentLine = presentToken.line;
    }
    else{
        if(!ueofShown){
            err("Unexpected end of file!");
            token_print_source(presentToken, 1);
            ueofShown = 1;
            hasErrors++;
        }
    }
}

#define match(x) match2(TOKEN_##x)

static bool match2(TokenType type){
    if(list.tokens[present].type == type){
        return true;
    }
    return false;
}

static const char* tokenStrings[] = { 
#define ET(x) #x,
#include "tokens.h"
#undef ET
};

#define consume(x) consume2(TOKEN_##x)

bool consume2(TokenType type){
    if(match2(type)){
        advance();
        return true;
    }
    else if(presentToken.type == TOKEN_eof){
        if(!ueofShown){
            err("Unexpected end of file!");
            token_print_source(presentToken, 1);
            ueofShown = 1;
            hasErrors++;
        }
        return false;
    }
    else{
        err("Unexpected token : '" ANSI_FONT_BOLD ANSI_COLOR_MAGENTA );
        lexer_print_token(presentToken, 0);
        printf(ANSI_COLOR_RESET "', Expected : '" ANSI_FONT_BOLD "%s" ANSI_COLOR_RESET "'",
                tokenStrings[type]);
        token_print_source(presentToken, 1);
        advance();
        hasErrors++;
        return false;
    }
}

static Expression* expression();

static char* format_string(const char *str, size_t length){
    char *ret = NULL;
    size_t j = 0;
    for(size_t i = 0;i < length;i++, j++){
        ret = (char *)realloc(ret, j+1);
        if(str[i] == '\\'){
            if(str[i+1] == 'n'){
                ret[j] = '\n';
                i++;
            }
            else if(str[i+1] == 't'){
                ret[j] = '\t';
                i++;
            }
            else if(str[i+1] == '"'){
                ret[j] = '"';
                i++;
            }
        }
        else
            ret[j] = str[i];

    }
    ret = (char *)realloc(ret, j+1);
    ret[j] = 0;
    return ret;
}

static bool istype(){
    return match(Number) || match(Integer) || match(String)
        || match(Boolean);
}

static int valuetype_from_token(Token t){
    switch(t.type){
        case TOKEN_Number:
            return VALUE_NUM;
        case TOKEN_Integer:
            return VALUE_INT;
        case TOKEN_Boolean:
            return VALUE_BOOL;
        default:
            return VALUE_STR;
    }
}

static Expression* _primary(){
    if(match(number)){
        Expression *ex = expr_new(EXPR_CONSTANT);
        ex->token = presentToken;
        char *error = NULL;
        char *str = (char*)malloc(presentToken.length + 1);
        strncpy(str, presentToken.string, presentToken.length);
        str[presentToken.length] = 0;
        double res = strtod(str, &error);
        if(*error != 0){

            free(str);
            err("Bad Number : " ANSI_FONT_BOLD ANSI_COLOR_MAGENTA );
            lexer_print_token(presentToken, 0);
            printf(ANSI_COLOR_RESET);
            token_print_source(presentToken, 1);
            advance();
            hasErrors++;
            return NULL;
        }
        else{
            free(str);
            ex->consex.dval = res;
            ex->valueType = VALUE_NUM;
            advance();
            return ex;
        }
    }
    if(match(integer)){
        Expression *ex = expr_new(EXPR_CONSTANT);
        ex->token = presentToken;
        char *error = NULL;
        char *str = (char*)malloc(presentToken.length + 1);
        strncpy(str, presentToken.string, presentToken.length);
        str[presentToken.length] = 0;
        //dbg("%s", str);
        int64_t res = strtol(str, &error, 10);
        if(*error != 0){
            free(str);
            err("Bad Integer : " ANSI_FONT_BOLD ANSI_COLOR_MAGENTA );
            lexer_print_token(presentToken, 0);
            printf( ANSI_COLOR_RESET);
            //err("Errornous part : %s", error);
            token_print_source(presentToken, 1);
            advance();
            hasErrors++;
            return NULL;
        }
        else{
            free(str);
            ex->consex.ival = res;
            ex->valueType = VALUE_INT;
            advance();
            return ex;
        }
    }
    if(match(string)){ 
        Expression *ex = expr_new(EXPR_CONSTANT);
        ex->token = presentToken;
        ex->consex.sval = format_string(presentToken.string, presentToken.length);
        advance();
        ex->valueType = VALUE_STR;
        return ex;
    }
    if(match(not) || match(minus)){
        Expression *ex = expr_new(EXPR_UNARY);
        ex->token = presentToken;
        advance();
        ex->unex.right = _primary();
        return ex;
    }
    if(match(Type)){
        Expression *ex = expr_new(EXPR_UNARY);
        ex->token = presentToken;
        advance();
        if(consume(paranthesis_open)){
            if(istype() && list.tokens[present + 1].type == TOKEN_paranthesis_close){
                ex->unex.right = expr_new(EXPR_UNARY);
                ex->unex.right->token = presentToken;
                ex->valueType = valuetype_from_token(presentToken);
                advance(); // type
                advance(); // )
                return ex;
            }
            else {
                ex->valueType = VALUE_GEN;
                ex->unex.right = expression();
            }
            if(consume(paranthesis_close))
                return ex;
        }
        token_print_source(presentToken, 1);
        hasErrors++;
        advance();
        return NULL;
    }
    if(istype() /* || match(Structure) */){
        Token op = presentToken;
        advance();
        if(match(paranthesis_open)){ // cast
            advance();
            Expression *ex = expr_new(EXPR_UNARY);
            ex->token = op;
            ex->unex.right = expression();
            consume(paranthesis_close);
            return ex;
        }
       /* if(match(identifier)){ // typed arguments
            Expression *ex = expr_new(EXPR_VARIABLE);
            ex->token = presentToken;
            advance();
            switch(op.type){
                case TOKEN_Integer:
                    ex->valueType = VALUE_INT;
                    break;
                case TOKEN_Number:
                    ex->valueType = VALUE_NUM;
                    break;
                case TOKEN_String:
                    ex->valueType = VALUE_STR;
                    break;
                case TOKEN_Structure:
                    ex->valueType = VALUE_GEN; // we can't get the structure at compile time,
                                                // so nevertheless, we have to perform runtime
                                                // checks
                    break;
                case TOKEN_Boolean:
                    ex->valueType = VALUE_BOOL;
                    break;
                default:
                    break;
            }
            return ex;
        }
        */
        err("Bad type specification!");
        token_print_source(op, 1);
        hasErrors++;
        return NULL;
    }
   if(match(paranthesis_open)){
        advance();
        Expression *ex = expression();
        consume(paranthesis_close);
        return ex;
   }
   if(match(identifier)){ 
        Expression *ex = expr_new(EXPR_VARIABLE);
        ex->token = presentToken;
        advance();
        ex->valueType = VALUE_UND;
        return ex;
   }

   if(match(True) || match(False) || match(Null)){
        Expression *ces = expr_new(EXPR_CONSTANT);
        ces->token = presentToken;
        ces->valueType = match(True) || match(False) ? VALUE_BOOL : VALUE_STRUCT;
        advance();
        return ces;
   }
    
    err("Unexpected token '" ANSI_COLOR_MAGENTA ANSI_FONT_BOLD "%s" ANSI_COLOR_RESET "'!",tokenStrings[presentToken.type]);
    token_print_source(presentToken, 1);
    hasErrors++;
    advance();

    return NULL;
}

static Expression* _call(){
    Expression* e = _primary();
    /* Disbaled for now
    while(e->type != EXPR_CONSTANT && (match(dot) || match(paranthesis_open))){
        if(match(dot)){
            Expression *ex = expr_new(EXPR_REFERENCE);
            ex->refex.parent = e;
            advance();
            ex->refex.refer = _primary();
            switch(ex->refex.refer->type){
                case EXPR_REFERENCE:
                case EXPR_VARIABLE:
                case EXPR_CALL:
                    break;
                default:
                    err("Bad reference expression!");
                    token_print_source(ex->token, 1);
                    hasErrors++;
                    break;
            }
           e = ex; 
        }
        else{
            Expression *ex = expr_new(EXPR_CALL);
            ex->token = e->token;
            ex->calex.arity = 0;
            ex->calex.callee = e;
            ex->calex.args = NULL;
            advance();
            while(!match(paranthesis_close) && !match(eof)){
                if(ex->calex.arity != 0){
                    consume(comma);
                }
                ex->calex.arity++;
                ex->calex.args = (Expression **)realloc(ex->calex.args, sizeof(Expression *) * ex->calex.arity);
                ex->calex.args[ex->calex.arity - 1] = expression();
            }
            consume(paranthesis_close);
            e = ex;
        }
    }*/
    return e;
}

#define exp2(name, super, x) \
    static Expression* _##name(){ \
        Expression *ex = _##super(); \
        while(x){ \
            Expression *root = expr_new(EXPR_BINARY); \
            root->token = presentToken; \
            advance(); \
            root->binex.left = ex; \
            root->binex.right = _##super(); \
            ex = root; \
        } \
        return ex; \
    }

exp2(tothepower, call, (match(cap)))

exp2(multiplication, tothepower, (match(star) || match(backslash)))

exp2(addition, multiplication, (match(plus) || match(minus)))

exp2(glt, addition, (match(greater) || match(greater_equal) || match(lesser) || match(lesser_equal)))

exp2(eq, glt, (match(equal_equal) || match(not_equal)))

//exp2(assignment, eq, (match(equal)))

static Expression* expression(){
    return _eq();
}

#define stmt_new2(x) stmt_new(STATEMENT_##x)

static Statement* statement_Set(){
    Expression *target = expression();
    if(target->type != EXPR_VARIABLE && target->type != EXPR_REFERENCE){
        err("Bad Set target");
        token_print_source(presentToken, 1);
        hasErrors++;
        return NULL;
    }
    if(consume(equal)){
        Expression *value = expression();
        Statement *s = stmt_new2(SET);
        s->sets.target = target;
        s->sets.value = value;
        //if(s->sets.target->type == EXPR_VARIABLE)
            s->sets.target->valueType = VALUE_UND;
        return s;
    }
    return NULL;
}

static Statement* statement();

static void block_add_statement(BlockStatement *block, Statement *s){
    if(!s)
        return;
    block->count++;
    block->statements = (Statement **)realloc(block->statements, sizeof(Statement *) * block->count);
    block->statements[block->count - 1] = s;
}

static BlockStatement statement_block(){
    BlockStatement ret = {0, NULL};
    if(consume(brace_open)){
        while(!match(eof) && !match(brace_close))
            block_add_statement(&ret, statement());
        if(match(brace_close))
           advance();
    }
    return ret;
}

static Statement* statement_If(){
    Expression *cond = expression();
        Statement *s = stmt_new2(IF);
        s->ifs.condition = cond;
        s->ifs.thenBlock = statement_block();
        s->ifs.elseIf = NULL;
        s->ifs.elseBlock = (BlockStatement){0, NULL};
        if(match(Else)){
            advance();
            if(match(If)){
                advance();
                s->ifs.elseIf = statement_If();
            }
            else{
                s->ifs.elseBlock = statement_block();
                //break;
            }
        }
        return s;
   // }
    return NULL;
}

static Statement* statement_Else(){
    err("Else without If!");
    token_print_source(previousToken, 1);
    hasErrors++;
    return NULL;
}

static Expression** add_expression(Expression **store, uint64_t *count, Expression *e){
    (*count)++;
    store = (Expression **)realloc(store, sizeof(Expression *) * *count);
    store[*count - 1] = e;
    return store;
}

static Statement* statement_Print(){
    Statement *print = stmt_new2(PRINT);
    print->prints.args = NULL;
    print->prints.count = 0;
    Expression *e = expression();
    print->prints.args = add_expression(print->prints.args, &print->prints.count, e);
    while(match(comma)){
        advance();
        print->prints.args = add_expression(print->prints.args, &print->prints.count, expression());
    }
    return print;
}

static Statement* statement_While(){
    Statement *wh = stmt_new2(WHILE);
    wh->whiles.condition = expression();
    wh->whiles.statements = statement_block();
    return wh;
}

static Statement* statement_Do(){
    Statement *wh = stmt_new2(DO);
    wh->dos.statements = statement_block();
    if(consume(While)){
        wh->dos.condition = expression();
    }
    return wh;
}

/*

static Statement* statement_Define(){
    Expression *name = expression(); 
    if(name->type == EXPR_CALL){
        name->type = EXPR_DEFINE;
        Statement* def = stmt_new2(DEFINE);
        def->defs.name = name;
       // def->defs.arguments = NULL;
       // def->defs.arity = 0;
       // consume(paranthesis_open);
      //  if(!match(paranthesis_close)){
       //     def->defs.arguments = add_expression(def->defs.arguments, &def->defs.arity, expression());
       //     while(match(comma)){
       //         advance();
       //         def->defs.arguments = add_expression(def->defs.arguments, &def->defs.arity, expression());
       //     }
       // }
       // consume(paranthesis_close);
        def->defs.body = statement_block();

        return def;
    }
    else{
        err("Expected identifier!");
        token_print_source(presentToken, 1);
        hasErrors++;
    }
    return NULL;
}

static Statement* statement_Container(){
    Statement *s = statement_Define();
    if(s != NULL){
        s->type = STATEMENT_CONTAINER;
    }
    return s;
}

static Statement* statement_Return(){
    Statement *ret = stmt_new2(RETURN);
    Expression *ex = expression();
    ret->rets.value = ex;
    return ret;
}
*/

#define unexcons(x) \
static Statement* statement_##x(){ \
    err("Unexpected token " #x "!"); \
    token_print_source(previousToken, 1); \
    return NULL; \
}

unexcons(True)

unexcons(False)

unexcons(Null)

unexcons(Type)

#define deftype(x, y) \
static Statement* statement_##x(){ \
    Statement *s = statement_Set(); \
    if(s != NULL && s->sets.target->type == EXPR_VARIABLE) \
        s->sets.target->valueType = VALUE_##y; \
    else if(s != NULL){ \
        err("Unable to specify type of referenced target!"); \
        token_print_source(s->sets.target->token, 1); \
        hasErrors++; \
        return NULL; \
    } \
    return s; \
}

deftype(Integer, INT)

deftype(Number, NUM)

deftype(String, STR)

deftype(Boolean, BOOL)

deftype(Structure, STRUCT)

/*
static Statement* statement_Call(){
    Expression *callee = expression();
    if(callee->type == EXPR_CALL){
        Statement *cs = stmt_new2(CALL);
        cs->calls.callExpression = callee;
        return cs;
    }
    else{
        err("Expected call expression!");
        token_print_source(presentToken, 1);
        hasErrors++;
        advance();
        return NULL;
    }
}
*/
static Statement* statement(){ 
        switch(presentToken.type){
#define KEYWORD(name, a) \
            case TOKEN_##name: \
            { \
            Token bak = presentToken; \
            consume2(TOKEN_##name); \
            Statement *s = statement_##name(); \
            if(s != NULL) \
                s->token = bak; \
            return s; \
            break; \
        }
#include "keywords.h"
#undef KEYWORD
            default:
//badtoken:
                err("Bad token : '" ANSI_FONT_BOLD ANSI_COLOR_MAGENTA );
                lexer_print_token(presentToken, 0);
                printf("'" ANSI_COLOR_RESET);
                token_print_source(presentToken, 1);
                hasErrors++;
                advance();
                return NULL;
        }
}

BlockStatement parse(TokenList l){
    presentToken = l.tokens[0];
    presentLine = presentToken.line;
    list = l;
    length = list.count;
    ueofShown = 0;
    BlockStatement ret = {0, NULL};
    while(!match(eof)){
        block_add_statement(&ret, statement());
    }
    return ret;
}

uint64_t parser_has_errors(){
    return hasErrors;
}
