#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include "display.h"
#include "lexer.h"

void pred(const char* msg, ...){
    printf(ANSI_COLOR_RED);
    va_list(args);
    va_start(args, msg);
    vprintf(msg, args);
    printf(ANSI_COLOR_RESET);
}

void pblue(const char* msg, ...){
    printf(ANSI_COLOR_BLUE);
    va_list(args);
    va_start(args, msg);
    vprintf(msg, args);
    printf(ANSI_COLOR_RESET);
}

void pylw(const char* msg, ...){
    printf(ANSI_COLOR_YELLOW);
    va_list(args);
    va_start(args, msg);
    vprintf(msg, args);
    printf(ANSI_COLOR_RESET);
}

void pgrn(const char* msg, ...){
    printf(ANSI_COLOR_GREEN);
    va_list(args);
    va_start(args, msg);
    vprintf(msg, args);
    printf(ANSI_COLOR_RESET);
}

void pcyn(const char* msg, ...){
    printf(ANSI_COLOR_CYAN);
    va_list(args);
    va_start(args, msg);
    vprintf(msg, args);
    printf(ANSI_COLOR_RESET);
}

void pmgn(const char* msg, ...){
    printf(ANSI_COLOR_MAGENTA);
    va_list(args);
    va_start(args, msg);
    vprintf(msg, args);
    printf(ANSI_COLOR_RESET);
}

#ifdef DEBUG
void dbg2(const char* msg, ...){
#else
void dbg(const char* msg, ...){
#endif
    printf(ANSI_FONT_BOLD);
    printf(ANSI_COLOR_GREEN "\n[Debug] ");
    printf(ANSI_COLOR_RESET);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}
#ifdef DEBUG
void info2(const char* msg, ...){
#else
void info(const char* msg, ...){
#endif
    printf(ANSI_FONT_BOLD);
    printf(ANSI_COLOR_BLUE "\n[Info] ");
    printf(ANSI_COLOR_RESET);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}

#ifdef DEBUG
void err2(const char* msg, ...){
#else
void err(const char* msg, ...){
#endif
    printf(ANSI_FONT_BOLD);
    printf(ANSI_COLOR_RED "\n[Error] ");
    printf(ANSI_COLOR_RESET);
    va_list args;
    va_start(args, msg);
    vprintf(msg,args);
}

#ifdef DEBUG
void warn2(const char* msg, ...){
#else
void warn(const char* msg, ...){
#endif
    printf(ANSI_FONT_BOLD);
    printf(ANSI_COLOR_YELLOW "\n[Warning] ");
    printf(ANSI_COLOR_RESET);
    va_list args;
    va_start(args, msg);
    vprintf(msg, args);
}

void lnerr(const char* msg, Token t, ...){
    printf(ANSI_FONT_BOLD);
    printf(ANSI_COLOR_RED "\n[Error] <line %zd> ", t.line);
    printf(ANSI_COLOR_RESET);
    va_list args;
    va_start(args, t);
    vprintf(msg, args);
}

void lnwarn(const char* msg, Token t, ...){
    printf(ANSI_FONT_BOLD);
    printf(ANSI_COLOR_YELLOW "\n[Warning] <line %zd> ", t.line);
    printf(ANSI_COLOR_RESET);
    va_list args;
    va_start(args, t);
    vprintf(msg, args);
}

void lninfo(const char* msg, Token t, ...){
    printf(ANSI_FONT_BOLD);
    printf(ANSI_COLOR_BLUE "\n[Info] <line %zd> ", t.line);
    printf(ANSI_COLOR_RESET);
    va_list args;
    va_start(args, t);
    vprintf(msg, args);
}
