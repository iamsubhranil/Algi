#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

#include "runtime.h"
#include "display.h"
#include "types.h"

typedef union{
    int64_t inum;
    double dnum;
    char *str;
} NumConverter;

int64_t __algi_to_int(int32_t type, ...){
    char *str, *err = NULL;
    va_list args;
    va_start(args, type);
    NumConverter nc;
    if(type != VALUE_NUM || type > VALUE_GEN){
        nc.inum = va_arg(args, int64_t);
        type > VALUE_GEN ? type -= VALUE_GEN : 0;
    }
    else
        nc.dnum = va_arg(args, double);
    va_end(args);
    switch(type){
        case VALUE_BOOL:
        case VALUE_INT:
            return nc.inum;
        case VALUE_NUM:
            {
                return (int64_t)nc.dnum;
            }
        case VALUE_STR:
            {
                str = nc.str;
//__algi_int_convert:;
                int64_t res = (int64_t)strtol(str, &err, 10);
                if(*err != 0){
                    err("Unable to convert %s to Integer!", str);
                    exit(EXIT_FAILURE);
                }
                return res;
            }
            break;
            /*
        case VALUE_GEN:
            {
                AlgiGenericValue agv = va_arg(args, AlgiGenericValue);
                if(agv.type == VALUE_INT)
                    return agv.inumber;
                if(agv.type == VALUE_NUM)
                    return (int64_t)agv.dnumber;
                if(agv.type == VALUE_BOOL)
                    return agv.inumber;
                if(agv.type == VALUE_STR){
                    str = agv.string;
                    goto __algi_int_convert;
                }
            }*/
        default:
            err("Unable to convert Container to Integer!");
            exit(EXIT_FAILURE);
    }
    return 0;
}

double __algi_to_double(int32_t type, ...){
    char *str, *err = NULL;
    va_list args;
    va_start(args, type);
    NumConverter nc;
    if(type != VALUE_NUM || type > VALUE_GEN){
        nc.inum = va_arg(args, int64_t);
        type > VALUE_GEN ? type -= VALUE_GEN : 0;
    }
    else
        nc.dnum = va_arg(args, double);
    va_end(args);
    switch(type){
        case VALUE_BOOL:
            return (double)nc.inum;
        case VALUE_INT:
            return (double)nc.inum;
        case VALUE_NUM:
            {
                return nc.dnum;
            }
        case VALUE_STR:
            {
                str = nc.str;
//__algi_double_convert:;
                double res = strtod(str, &err);
                if(*err != 0){
                    err("Unable to convert %s to Number!", str);
                    exit(EXIT_FAILURE);
                }
                return res;
            }
            break;
            /*
        case VALUE_GEN:
            {
                AlgiGenericValue agv = va_arg(args, AlgiGenericValue);
                if(agv.type == VALUE_INT)
                    return agv.inumber;
                if(agv.type == VALUE_NUM)
                    return agv.dnumber;
                if(agv.type == VALUE_BOOL)
                    return agv.inumber;
                if(agv.type == VALUE_STR){
                    str = agv.string;
                    goto __algi_double_convert;
                }
            }*/
        default:
            err("Unable to convert Container to Number!");
            exit(EXIT_FAILURE);
            break;
    }
    return 0;
}

char* __algi_to_string(int32_t type, ...){
    va_list args;
    va_start(args, type);
    char *s = NULL;
    uint64_t len = 0;
    NumConverter nc;
    if(type != VALUE_NUM || type > VALUE_GEN){
        nc.inum = va_arg(args, int64_t);
        type > VALUE_GEN ? type -= VALUE_GEN : 0;
    }
    else
        nc.dnum = va_arg(args, double);
    va_end(args);
    switch(type){
        case VALUE_INT:
            len = snprintf(NULL, 0, "%" PRId64, nc.inum);
            break;
        case VALUE_NUM:
            len = snprintf(NULL, 0, "%.10g", nc.dnum);
            break;
        case VALUE_STR:
            return nc.str;
        case VALUE_BOOL:
            {
                int res = nc.inum;
                if(res)
                    return strdup("True");
                else
                    return strdup("False");
            }
            break;
       /* case VALUE_GEN:
            {
                AlgiGenericValue agv = va_arg(val, AlgiGenericValue);
                if(agv.type == VALUE_INT)
                    format = "%" PRId64;
                else if(agv.type == VALUE_NUM)
                    format = "%.10g";
                else if(agv.type == VALUE_BOOL){
                    if(agv.inumber)
                        len = 4;
                    else
                        len = 5;
                }
                else if(agv.type == VALUE_STR)
                    return agv.string;
            }
            break;
            */
        default:
            err("Unable to convert Container to String!");
            exit(EXIT_FAILURE);
    }

    s = (char *)malloc(len+1);
    if(type == VALUE_INT)
        snprintf(s, len+1, "%" PRId64, nc.inum);
    else
        snprintf(s, len+1, "%.10g", nc.dnum);
    return s;
}

int8_t __algi_to_boolean(int32_t type, ...){
    //dbg("Type : %d", type);
    va_list args;
    char *str;
    va_start(args, type);
    NumConverter nc;
    if(type != VALUE_NUM || type > VALUE_GEN){
        nc.inum = va_arg(args, int64_t);
        type > VALUE_GEN ? type -= VALUE_GEN : 0;
    }
    else
        nc.dnum = va_arg(args, double);
    va_end(args);
    switch(type){
        case VALUE_BOOL:
            return nc.inum ? 1 : 0;
        case VALUE_INT:
            return nc.inum ? 1 : 0;
        case VALUE_NUM:
            {
                return nc.dnum ? 1 : 0;
            }
        case VALUE_STR:
            {
                str = nc.str;
//__algi_boolean_convert:;
                if(strcmp(str, "True") == 0)
                    return 1;
                else if(strcmp(str, "False") == 0)
                    return 0;
                else{
                    err("Unable to convert String \"%s\" to Boolean!", str);
                    exit(EXIT_FAILURE);
                }
            }
            break;
            /*
        case VALUE_GEN:
            {
                AlgiGenericValue agv = va_arg(args, AlgiGenericValue);
                //dbg("agv.type : %d", agv.type);
                if(agv.type == VALUE_INT)
                    return agv.inumber > 0 ? 1 : 0;
                if(agv.type == VALUE_NUM)
                    return agv.dnumber > 0 ? 1 : 0;
                if(agv.type == VALUE_BOOL)
                    return agv.inumber;
                if(agv.type == VALUE_STR){
                    str = agv.string;
                    goto __algi_boolean_convert;
                }
            }
            */
        default:
            err("Unable to convert Container to Boolean!");
            exit(EXIT_FAILURE);
            break;
    }
    return 0;
}

void __algi_generic_print(AlgiGenericValue agv){
    switch(agv.type){
        case VALUE_BOOL:
            printf("%s", agv.inumber ? "True" : "False");
            break;
        case VALUE_INT:
            printf("%" PRId64, agv.inumber);
            break;
        case VALUE_NUM:
            printf("%.10g", agv.dnumber);
            break;
        case VALUE_STR:
            printf("%s", agv.string);
            break;
    }
}

void __algi_generic_store(AlgiGenericValue *value, int32_t storeType, ...){
    va_list args;
    va_start(args, storeType);
    switch(storeType){
        case VALUE_BOOL:
            value->inumber = va_arg(args, int);
            break;
        case VALUE_INT:
            value->inumber = va_arg(args, int64_t);
            break;
        case VALUE_NUM:
            value->dnumber = va_arg(args, double);
            break;
        case VALUE_STR:
            value->string = va_arg(args, char*);
            break;
        case VALUE_GEN:
            {
                AlgiGenericValue agv = va_arg(args, AlgiGenericValue);
                value->type = agv.type;
                value->inumber = agv.inumber;
                return;
            }
    }
    value->type = storeType;
}
