#include "lexer.h"
#include "display.h"
#include "algi_common.h"
#include "parser.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>

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

int main(int argc, char *argv[]){
    if(argc != 2){
        err("Usage : %s filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    char *source = read_whole_file(argv[1]);
    if(source == NULL){
        exit(EXIT_FAILURE);
    }
    TokenList l = tokens_scan(source);
    lexer_print_tokens(l);
    
    stmt_init();
    expr_init();
    
    BlockStatement s = parse(l);
    blockstmt_print(s);
   
    printf("\n");

    blockstmt_dispose(s);
    tokens_free(l);
    free(source);
    stmt_dispose();
    return 0;
}
