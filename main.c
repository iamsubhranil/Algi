#include "lexer.h"
#include "display.h"
#include "algi_common.h"
#include "parser.h"
#include "types.h"
#include "codegen.h"
#include "timer.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <inttypes.h>

static char* read_whole_file(const char* fileName){
    struct stat statbuf;
    stat(fileName, &statbuf);
    if(S_ISDIR(statbuf.st_mode)){
        err("Given argument is a directory!");
        return NULL;
    }
    char *buffer = NULL;
    long length;
    FILE *f = fopen(fileName, "rb");
    if(f){
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
#ifdef DEBUG
        dbg("Buffer allocated of length %ld bytes", length + 1);
#endif
        buffer = (char *)malloc(length + 1);
        if(buffer){
            fread(buffer, 1, length, f);
            buffer[length] = '\0';
        }
        fclose(f);
        return buffer;
    }
    return NULL;
}

static TokenList tokenList = {NULL, 0, 0, 0};

static void tokens_free_all(){
    if(tokenList.count > 0)
        tokens_free(tokenList);
}

static BlockStatement statements = {0, NULL};

static void block_free_all(){
    if(statements.count > 0)
        blockstmt_dispose(statements);
}

static char* source = NULL;

static void source_free(){
    if(source != NULL)
        free(source);
}

int main(int argc, char *argv[]){
    atexit(codegen_dispose);

    if(argc != 2){
        err("Usage : %s filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    source = read_whole_file(argv[1]);
    if(source == NULL){
        exit(EXIT_FAILURE);
    }
    atexit(source_free);

#ifdef DEBUG
    timer_start("Lexing");
#endif

    tokenList = tokens_scan(source);
    atexit(tokens_free_all);

#ifdef DEBUG
    timer_end();
#endif

    if(lexer_has_errors()){
        err("%" PRIu64 " errors occurred while lexing!", lexer_has_errors());
#ifdef DEBUG
        err("Press C to abort : ");
        char c = getc(stdin);
        if(c == 'c' || c == 'C')
#endif
            return 0;
    }

#ifdef DEBUG 
    lexer_print_tokens(tokenList);
#endif

    stmt_init();
    atexit(stmt_dispose);

    expr_init();
    atexit(expr_dispose);

#ifdef DEBUG
    timer_start("Parsing");
#endif

    statements = parse(tokenList);
    atexit(block_free_all);

#ifdef DEBUG
    timer_end();
#endif

    if(parser_has_errors()){
        err("%" PRIu64 " errors occurred while parsing!", parser_has_errors());
#ifdef DEBUG
        err("Press C to abort : ");
        char c = getc(stdin);
        if(c == 'c' || c == 'C')
#endif
            exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    blockstmt_print(statements);
#endif
    printf("\n\n");
    fflush(stdout);
#ifdef DEBUG
    timer_start("Type Checking");
#endif

    type_check(statements);
    atexit(type_dispose);
    
    printf("\n\n");
#ifdef DEBUG
    timer_end();
    //timer_start("JIT Compilation");
#endif
    if(type_has_errors()){
        err("%" PRIu64 " errors occurred while type checking!", type_has_errors());
#ifdef DEBUG
        err("Press C to abort : ");
        char c = getc(stdin);
        if(c == 'c' || c == 'C')
#endif
            exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    blockstmt_print(statements);
#endif
    codegen_compile(statements); 
    return 0;
}
