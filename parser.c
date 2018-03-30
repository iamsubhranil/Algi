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

static Expression* _primary(){
    if(match(number)){
        Expression *ex = expr_new(EXPR_CONSTANT);
        ex->consex.token = presentToken;
        char *error = NULL;
        char *str = (char*)malloc(presentToken.length + 1);
        strncpy(str, presentToken.string, presentToken.length);
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
            advance();
            return ex;
        }
    }
    if(match(integer)){
        Expression *ex = expr_new(EXPR_CONSTANT);
        ex->consex.token = presentToken;
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
            advance();
            return ex;
        }
    }
    if(match(string)){ 
        Expression *ex = expr_new(EXPR_CONSTANT);
        ex->consex.token = presentToken;
        ex->consex.sval = (char *)malloc(presentToken.length + 1);
        strncpy(ex->consex.sval, presentToken.string, presentToken.length);
        ex->consex.sval[presentToken.length] = 0;
        advance();
        return ex;
    }
    if(match(not) || match(minus) || match(Type)){
        Expression *ex = expr_new(EXPR_UNARY);
        ex->unex.token = presentToken;
        advance();
        ex->unex.right = expression();
        return ex;
    }
   if(match(paranthesis_open)){
        advance();
        Expression *ex = expression();
        consume(paranthesis_close);
        return ex;
   }
   if(match(identifier)){
        advance();
        if(match(dot)){
            Expression *ex = expr_new(EXPR_REFERENCE);
            ex->refex.token = previousToken;
            advance();
            ex->refex.refer = expression();
            switch(ex->refex.refer->type){
                case EXPR_REFERENCE:
                case EXPR_VARIABLE:
                case EXPR_CALL:
                    break;
                default:
                    err("Bad reference expression!");
                    token_print_source(ex->refex.token, 1);
                    hasErrors++;
                    break;
            }
            return ex;
        }
        if(match(paranthesis_open)){
            Expression *ex = expr_new(EXPR_CALL);
            ex->calex.token = previousToken;
            ex->calex.arity = 0;
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
            return ex;
        }
        
        Expression *ex = expr_new(EXPR_VARIABLE);
        ex->varex.token = previousToken;
        return ex;
   }

   if(match(True) || match(False) || match(Null)){
        Expression *ces = expr_new(EXPR_CONSTANT);
        ces->consex.token = presentToken;
        advance();
        return ces;
   }
    
    err("Unexpected token '" ANSI_COLOR_MAGENTA ANSI_FONT_BOLD "%s" ANSI_COLOR_RESET "!",tokenStrings[presentToken.type]);
    token_print_source(presentToken, 1);
    hasErrors++;
    advance();

    return NULL;
}

#define exp2(name, super, x) \
    static Expression* _##name(){ \
        Expression *ex = _##super(); \
        while(x){ \
            Expression *root = expr_new(EXPR_BINARY); \
            root->binex.token = presentToken; \
            advance(); \
            root->binex.left = ex; \
            root->binex.right = _##super(); \
            ex = root; \
        } \
        return ex; \
    }

exp2(tothepower, primary, (match(cap)))

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
    if(consume(equal)){
        Expression *value = expression();
        Statement *s = stmt_new2(SET);
        s->sets.target = target;
        s->sets.value = value;
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
    wh->dos.condition = expression();
    return wh;
}

static Statement* statement_Define(){
    Expression *name = expression(); 
    if(name->type == EXPR_CALL){
        Statement* def = stmt_new2(DEFINE);
        def->defs.name = name;
       // def->defs.arguments = NULL;
       // def->defs.arity = 0;
       /* consume(paranthesis_open);
        if(!match(paranthesis_close)){
            def->defs.arguments = add_expression(def->defs.arguments, &def->defs.arity, expression());
            while(match(comma)){
                advance();
                def->defs.arguments = add_expression(def->defs.arguments, &def->defs.arity, expression());
            }
        }
        consume(paranthesis_close); */
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
    Expression *ex = expression();
    Statement *ret = stmt_new2(RETURN);
    ret->rets.value = ex;
    return ret;
}

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
    if(s != NULL) \
        s->sets.target->valueType = VALUE_##y; \
    return s; \
}

deftype(Integer, INT)

deftype(Number, NUM)

deftype(String, STR)

deftype(Boolean, BOOL)

deftype(Structure, STRUCT)

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

static Statement* statement(){ 
        switch(presentToken.type){
#define KEYWORD(name, a) \
            case TOKEN_##name: \
                               consume2(TOKEN_##name); \
            return statement_##name(); \
            break;
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
