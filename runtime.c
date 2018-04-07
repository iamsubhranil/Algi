#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

#include "runtime.h"
#include "display.h"
#include "types.h"

int64_t __algi_to_int(int type, ...){
    //dbg("%s called", __func__);
    char *str, *err = NULL;
    va_list args;
    va_start(args, type);
    switch(type){
        case VALUE_STR:
            {
__algi_int_convert:
                str = va_arg(args, char*);
                //dbg("Recieved string %s", str);
                int64_t res = (int64_t)strtol(str, &err, 10);
                if(*err != 0){
                    err("Unable to convert %s to Integer!", str);
                    exit(EXIT_FAILURE);
                }
                return res;
            }
            break;
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
            }
        default:
            err("Unable to convert Container to Integer!");
            exit(EXIT_FAILURE);
    }
    return 0;
}

double __algi_to_double(int type, ...){
    char *str, *err = NULL;
    va_list args;
    va_start(args, type);
    switch(type){
        case VALUE_STR:
            {
__algi_double_convert:
                str = va_arg(args, char*);
                //dbg("Recieved string %s", str);
                double res = strtod(str, &err);
                if(*err != 0){
                    err("Unable to convert %s to Number!", str);
                    exit(EXIT_FAILURE);
                }
                return res;
            }
            break;
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
            }
        default:
            err("Unable to convert Container to Number!");
            exit(EXIT_FAILURE);
            break;
    }
    return 0;
}

char* __algi_to_string(int type, ...){
    va_list val;
    va_start(val, type);
    char *s = NULL;
    uint64_t len = 0;
    const char* format;
    switch(type){
        case VALUE_INT:
            format = "%" PRId64;
            break;
        case VALUE_NUM:
            format = "%.10g";
            break;
        case VALUE_BOOL:
            {
                int res = va_arg(val, int);
                if(res)
                    len = 4;
                else
                    len = 5;
            }
            break;
        case VALUE_GEN:
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
        default:
            err("Unable to convert Container to String!");
            exit(EXIT_FAILURE);
    }
    va_end(val);
    va_start(val, type);
    if(len == 0){
             len = vsnprintf(NULL, 0, format, val);
             //dbg("Len : %" PRIu64, len);
    }
    else if(len == 4)
        return strdup("True");
    else if(len == 5)
        return strdup("False");
    va_end(val);
    va_start(val, type);
    s = (char *)malloc(len+1);
    vsnprintf(s, len+1, format, val);
    return s;
}

int8_t __algi_to_boolean(int type, ...){
    va_list args;
    char *str;
    va_start(args, type);
    switch(type){
        case VALUE_STR:
            {
__algi_boolean_convert:
                str = va_arg(args, char*);
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
        case VALUE_GEN:
            {
                AlgiGenericValue agv = va_arg(args, AlgiGenericValue);
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
        default:
            err("Unable to convert Container to Boolean!");
            exit(EXIT_FAILURE);
            break;
    }
    return 0;
}

void __algi_generic_print(AlgiGenericValue agv){
    //dbg("%d %ld\n", agv.type, agv.inumber);
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

void __algi_generic_store(AlgiGenericValue *value, int storeType, ...){
    va_list args;
    va_start(args, storeType);
    //dbg("Storing %d to %p", storeType, value);
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
            }
            break;
    }
    value->type = storeType;
}
