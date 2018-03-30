#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <inttypes.h>

#include "lexer.h"
#include "display.h"


/* The code below is for testing and debugging purposes.
 * =====================================================
 */

#ifdef DEBUG

static const char* tokenStrings[] = { 
#define ET(x) #x,
#include "tokens.h"
#undef ET
};

void lexer_print_token(Token t, uint8_t printType){
    for(uint64_t i = 0;i < t.length;i++)
        pmgn("%c", t.string[i]);
    if(printType)
        pblue("(%s) \t", tokenStrings[t.type]);
}

void lexer_print_tokens(TokenList list){
    size_t prevLine = 0;
    for(uint64_t i = 0;i < list.count;i++){
        if(prevLine != list.tokens[i].line){
            pmgn(ANSI_FONT_BOLD "\n<line %zd>", list.tokens[i].line);
            prevLine = list.tokens[i].line;
        }
        lexer_print_token(list.tokens[i], 1);
    }
}

#endif // DEBUG

static Token keywords[] = {
#define ET(x) TOKEN_##x
#define KEYWORD(name, length) {#name, NULL, length, 0, 0, ET(name)},
#include "keywords.h"
#undef KEYWORD
#undef ET
};

static char* source = NULL;
static size_t present = 0, length = 0, start = 0, line = 1;
static Token lastToken;

static void addToken(TokenList *list, Token token){
    list->tokens = (Token *)realloc(list->tokens, sizeof(Token) * (++list->count));
    token.source = list->source;
    list->tokens[list->count - 1] = token;
}

#define makeToken(x) makeToken2(TOKEN_##x)

static Token makeToken2(TokenType type){
    Token t;
    t.source = NULL;
    t.type = type;
    t.line = line;
    t.string = &source[start];
    t.start = start;
    t.length = present - start+1;
    lastToken = t;

//#ifdef DEBUG
//    dbg("TokenType : %s", tokenStrings[type]);
//    dbg("Line : %zd", line);
//    dbg("Start : %zd", start);
//    dbg("End : %zd\n", present);
//#endif
    
    return t;
}

// rtype 1 --> error
// rtype 2 --> warning
// rtype 0 --> info
static void print_source(const char *s, size_t line, size_t hfrom, size_t hto, uint8_t rtype){
    if(rtype == 1)
        pred( ANSI_FONT_BOLD "\n<line %zd>\t" ANSI_COLOR_RESET, line);
    else if(rtype == 2)
        pylw( ANSI_FONT_BOLD "\n<line %zd>\t" ANSI_COLOR_RESET, line);
    else
        pblue( ANSI_FONT_BOLD "\n<line %zd>\t" ANSI_COLOR_RESET, line);

    if(s == NULL){
        pmgn("<source not available>");
        return;
    }
    size_t l = 1, p = 0;
    while(l < line){
        if(s[p] == '\n')
            l++;
        p++;
    }
    for(size_t i = p;s[i] != '\n' && s[i] != '\0';i++)
        printf("%c", s[i]);
    printf("\n            \t");
    for(size_t i = p;i < hto;i++){
        if(i >= hfrom && i <= hto)
            pmgn(ANSI_FONT_BOLD "~" ANSI_COLOR_RESET);
        else
            printf(" ");
    }
    printf("\n");
}

void token_print_source(Token t, uint8_t rtype){
    print_source(t.source, t.line, t.start, t.start + t.length, rtype);
}

static Token makeKeyword(){
    while(isalnum(source[present]))
        present++;
    char word[present - start + 1];
    for(uint64_t i = start;i < present;i++){
        word[i - start] = source[i];
    };
    word[present - start] = '\0';
    for(uint64_t i = 0;i < sizeof(keywords)/sizeof(Token);i++){
        if(keywords[i].length == (present - start)){
            if(strcmp(keywords[i].string, word) == 0){
                present--;
                return makeToken2(keywords[i].type);
            }
        }
    }

    present--;
    return makeToken(identifier);
}

static Token makeNumber(){
    uint8_t decimal = 0;
    uint64_t bak;
    while(isdigit(source[present]) || source[present] == '.'){
        if(source[present] == '.'){
            if(decimal == 0)
                bak = present;
            decimal++;
        }
        present++;
    }
    present--;
    if(((decimal == 1) && bak == present) || decimal > 1){
        return makeToken(unknown);
    }
    if(decimal == 1)
        return makeToken(number);
    return makeToken(integer);
}

static Token nextToken(uint8_t atStart){
    if(!atStart)
        present++;
    start = present;
    if(present == length)
        return makeToken(eof);
    if(isalpha(source[present])){
        return makeKeyword();
    }
    else if(isdigit(source[present])){
        return makeNumber();
    }
    switch(source[present]){
        case ' ':
        case '\t':
        case '\r':
            return nextToken(0);
        case '\n':
            line++;
            return nextToken(0);
        case ',':
            return makeToken(comma);
        case '.':
            return makeToken(dot);
        case ':':
            return makeToken(colon);
        case '+':
            return makeToken(plus);
        case '-':
            return makeToken(minus);
        case '/':
            return makeToken(backslash);
        case '*':
            return makeToken(star);
        case '^':
            return makeToken(cap);
        case '{':
            return makeToken(brace_open);
        case '}':
            return makeToken(brace_close);
        case '(':
            return makeToken(paranthesis_open);
        case ')':
            return makeToken(paranthesis_close);
        case '>':
            if(source[present + 1] == '='){
                present++;
                return makeToken(greater_equal);
            }
            return makeToken(greater);
        case '<':
            if(source[present + 1] == '='){
                present++;
                return makeToken(lesser_equal);
            }
            return makeToken(lesser);
        case '=':
            if(source[present + 1] == '='){
                present++;
                return makeToken(equal_equal);
            }
            return makeToken(equal);
        case '!':
            if(source[present + 1] == '='){
                present++;
                return makeToken(not_equal);
            }
            return makeToken(not);
        case '"':
            {
                present++;
                start = present;
                while(source[present] != '"' && source[present] != '\0'){
                    if(source[present] == '\\' && source[present+1] == '"')
                        present++;
                    else if(source[present] == '\n')
                        line++;
                    present++;
                }
                present--;
                Token t = makeToken(string);
                present++;
                //if(source[present] == '"')
                //    present++;
                return t;
            }
    }
    return makeToken(unknown);
}

TokenList tokens_scan(const char* line){
    source = strdup(line);
    length = strlen(source);
    present = 0;
    start = 0;

    TokenList list = {source, NULL, 0, 0};
    while(present < length){
        Token t = nextToken(present == 0);
        list.hasError += t.type == TOKEN_unknown;
        addToken(&list, t);
        if(t.type == TOKEN_unknown){ 
            err("Unexpected token '");
            lexer_print_token(t, 0);
            printf("'");
            token_print_source(list.tokens[list.count - 1], 1);
        }
    }
    if(list.tokens[list.count - 1].type != TOKEN_eof)
        addToken(&list, makeToken(eof));
    return list;
}


void tokens_free(TokenList list){
    free(list.tokens);
    free(list.source);
}
