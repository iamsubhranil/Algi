#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// LLVM Core
#include <llvm-c/Types.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/OrcBindings.h>

// Optimizations
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/Vectorize.h>

#include "expr.h"
#include "stmt.h"
#include "types.h"
#include "display.h"
#include "codegen.h"
#include "timer.h"
#include "runtime.h"

typedef struct{
    LLVMValueRef ref;
    uint64_t hash;
} VariableRef;

static uint64_t variableRefPointer = 0;
static VariableRef *variables = NULL;

static LLVMTypeRef get_generic_structure_type(){
    LLVMTypeRef types [] = {LLVMInt8Type(), LLVMInt64Type()};
    return LLVMStructType(types, 2, 0);
}

//static LLVMValueRef get_container_structure(){
//    LLVMValueRef 
//}

static uint64_t hash(const char *str, uint64_t length){
    uint64_t hash = 5381;
    uint64_t c = 0;
    //    printf("\nHashing..\n");
    while (c < length)
        hash = ((hash << 5) + hash) + str[c++]; /* hash * 33 + c */
    //    printf("\nInput String : [%s] Hash : %lu\n", str, hash);
    return hash;
}

static LLVMValueRef get_variable_ref(Expression *varE, uint8_t declareIfNotfound, LLVMBuilderRef builder){
    varE->hash = hash(varE->token.string, varE->token.length);
    //dbg("Searching for hash %ld", varE->hash);
    for(uint64_t i = 0;i < variableRefPointer;i++){
        if(variables[i].hash == varE->hash){
            return variables[i].ref;
        }
    }
    if(declareIfNotfound){
        //dbg("Declaring ");
        //lexer_print_token(varE->token, 0);
        variableRefPointer++;
        variables = (VariableRef *)realloc(variables, sizeof(VariableRef) * variableRefPointer);
        LLVMValueRef ref;
        switch(varE->valueType){
            case VALUE_GEN:
                ref = LLVMBuildAlloca(builder, get_generic_structure_type(), "localGeneric");
                break;
            case VALUE_STR:
                ref = LLVMBuildAlloca(builder, LLVMPointerType(LLVMInt8Type(), 0), "localString");
                break;
            case VALUE_NUM:
                ref = LLVMBuildAlloca(builder, LLVMDoubleType(), "localDouble");
                break;
            case VALUE_INT:
                ref = LLVMBuildAlloca(builder, LLVMInt64Type(), "localInt");
                break;
            case VALUE_STRUCT:
                ref = LLVMBuildAlloca(builder, get_generic_structure_type(), "localContainer");
                break;
            case VALUE_BOOL:
                ref = LLVMBuildAlloca(builder, LLVMInt1Type(), "localBool");
                break;
        }
        variables[variableRefPointer - 1].ref = ref;
        variables[variableRefPointer - 1].hash = varE->hash;
        return ref;
    }
    return LLVMConstNull(LLVMInt1Type());
}

// Marks which runtime functions have been used in the program
// to perform the global mapping of their physical address
// to the engine
static uint8_t runtime_function_used[ALGI_RUNTIME_FUNCTION_COUNT] = {0};

static ValueType convert_llvmtype_to_algitype(LLVMTypeRef type){
    if(type == get_generic_structure_type())
        return VALUE_GEN;
    if(type == LLVMInt1Type())
        return VALUE_BOOL;
    if(type == LLVMInt64Type())
        return VALUE_INT;
    if(type == LLVMDoubleType())
        return VALUE_NUM;
    return VALUE_STR;
}


static LLVMValueRef build_cast_call(LLVMBuilderRef builder, LLVMModuleRef module, LLVMValueRef val, ValueType castType){
    LLVMTypeRef argumentType[2];
    argumentType[0] = LLVMInt32Type();

    LLVMValueRef argumentValue[2];

    LLVMTypeRef returnType, valueType;
    const char* callee = "__algi_invalid_cast";
    if(LLVMIsAAllocaInst(val)){
        valueType = LLVMGetAllocatedType(val);
        val = LLVMBuildLoad(builder, val, "loadtmp");
    }
    else{
        valueType = LLVMTypeOf(val);
    }

    argumentType[1] = valueType;
    argumentValue[1] = val;

    ValueType algiType = convert_llvmtype_to_algitype(valueType);
    switch(castType){
        case VALUE_INT:
            {
                runtime_function_used[ALGI_TO_INT] = 1;
                callee = "__algi_to_int";
                returnType = LLVMInt64Type();
            }
            break;
        case VALUE_NUM:
            {
                runtime_function_used[ALGI_TO_DOUBLE] = 1;
                callee = "__algi_to_double";
                returnType = LLVMDoubleType();
            }
            break;
        case VALUE_STR:
            {
                runtime_function_used[ALGI_TO_STRING] = 1;
                callee = "__algi_to_string";
                returnType = LLVMPointerType(LLVMInt8Type(), 0);
            }
            break;
        case VALUE_BOOL:
            {
                runtime_function_used[ALGI_TO_BOOLEAN] = 1;
                callee = "__algi_to_boolean";
                returnType = LLVMInt1Type();
            }
            break;
        default:
            {
                algiType = VALUE_UND;
                returnType = LLVMVoidType();
            }
            break;
    }
    if(algiType == VALUE_STR && LLVMGetTypeKind(valueType) == LLVMArrayTypeKind){
        dbg("Changing string type for val : \n");
        LLVMDumpValue(val);
        size_t l;
        val = LLVMBuildGlobalString(builder, LLVMGetAsString(val, &l), "__runtime_call_tmp");
        argumentValue[1] = val;
    }
    else if(algiType == VALUE_GEN){
        argumentValue[0] = LLVMBuildExtractValue(builder, val, 0, "genextrct0");
        argumentValue[0] = LLVMBuildIntCast(builder, argumentValue[0], LLVMInt32Type(), "gen0cast");
        LLVMValueRef cons = LLVMConstInt(LLVMInt32Type(), VALUE_GEN, 0);
        argumentValue[0] = LLVMBuildAdd(builder, argumentValue[0], cons, "gentype");
        argumentValue[1] = LLVMBuildExtractValue(builder, val, 1, "genextrct1"); 
    }
    if(algiType != VALUE_GEN)
        argumentValue[0] = LLVMConstInt(LLVMInt32Type(), algiType, 0);
    LLVMTypeRef f = LLVMFunctionType(returnType, argumentType, 1, 1);
    LLVMValueRef fn = LLVMGetNamedFunction(module, callee);
    if(fn == NULL){
        fn = LLVMAddFunction(module, callee, f);
    }
    return LLVMBuildCall(builder, fn, argumentValue, 2, "__algi_internal_cast_res");
}

static LLVMValueRef expr_compile(Expression *e, LLVMContextRef context, LLVMBuilderRef builder, LLVMModuleRef module){
    switch(e->type){
        case EXPR_CONSTANT:
            {
                switch(e->token.type){
                    case TOKEN_integer:
                        return LLVMConstInt(LLVMInt64Type(), e->consex.ival, 0);
                    case TOKEN_number:
                        return LLVMConstReal(LLVMDoubleType(), e->consex.dval);
                    case TOKEN_string:
                        return LLVMConstString(e->consex.sval, strlen(e->consex.sval), 0);
                    case TOKEN_True:
                        return LLVMConstInt(LLVMInt1Type(), 1, 1);
                    case TOKEN_False:
                        return LLVMConstInt(LLVMInt1Type(), 0, 1);
                    default:
                        return LLVMConstNull(LLVMInt1Type());
                }
            }
        case EXPR_UNARY:
            {
                LLVMValueRef expVal = expr_compile(e->unex.right, context, builder, module);
                LLVMTypeRef t = LLVMTypeOf(expVal);
                if(LLVMIsAAllocaInst(expVal)){
                    t = LLVMGetAllocatedType(expVal);
                    expVal = LLVMBuildLoad(builder, expVal, "tmpLoad");
                }
#define ISINT() \
                if(t == LLVMInt64Type() \
                        || t == LLVMInt1Type())
#define ISFLT() \
                if(t == LLVMDoubleType())
#define ISBOOL() \
                if(t == LLVMInt1Type())
                switch(e->token.type){
                    case TOKEN_minus:
                        ISINT()
                            return LLVMBuildNeg(builder, expVal, "negtmp");
                        else ISFLT()
                            return LLVMBuildFNeg(builder, expVal, "fnegtmp");
                        return build_cast_call(builder, module, expVal, VALUE_NUM);
                    case TOKEN_not:
                        ISBOOL()
                            return LLVMBuildNot(builder, expVal, "nottmp");
                        return build_cast_call(builder, module, expVal, VALUE_BOOL);
                    case TOKEN_Integer:
                        ISINT(){
                            ISBOOL()
                                return LLVMBuildZExt(builder, expVal, LLVMInt64Type(), "bcastint");
                            return expVal;
                        }
                        else ISFLT()
                            return LLVMBuildFPToSI(builder, expVal, LLVMInt64Type(), "fcasttmp");
                        return build_cast_call(builder, module, expVal, VALUE_INT);
                    case TOKEN_Number:
                        ISINT(){
                            ISBOOL()
                                return LLVMBuildUIToFP(builder, expVal, LLVMDoubleType(), "bdoublecasttmp");
                            return LLVMBuildSIToFP(builder, expVal, LLVMDoubleType(), "doublecasttmp");
                        }
                        else ISFLT()
                            return expVal;

                        return build_cast_call(builder, module, expVal, VALUE_NUM);
                        // case TOKEN_Structure:
                        //     return LLVMBuildPointerCast(builder, expVal, LLVMInt64Type(), "pointercasttmp");
                    case TOKEN_Boolean:
                        ISINT(){
                            ISBOOL()
                                return expVal;
                            return LLVMBuildIntCast(builder, expVal, LLVMInt1Type(), "boolcasttmp");
                        }
                        return build_cast_call(builder, module, expVal, VALUE_BOOL);
                    case TOKEN_String:
                        return build_cast_call(builder, module, expVal, VALUE_STR);
                    case TOKEN_Type:
                        if(t == get_generic_structure_type()){ 
                            expVal = LLVMBuildExtractValue(builder, expVal, 0, "typeextract");
                            return LLVMBuildIntCast(builder, expVal, LLVMInt64Type(), "typecnv");
                        }
                         return LLVMConstInt(LLVMInt64Type(), convert_llvmtype_to_algitype(t), 0); 
                    default:
                        // TODO: Handle this properly
                        return LLVMConstNull(LLVMInt1Type());
#undef ISNUM
                }
            }
        case EXPR_VARIABLE:
            return get_variable_ref(e, 1, builder);
        case EXPR_BINARY:
            {
                LLVMValueRef left = expr_compile(e->binex.left, context, builder, module);
                LLVMValueRef right = expr_compile(e->binex.right, context, builder, module);
                LLVMTypeRef leftType = LLVMVoidType();
                LLVMTypeRef rightType = LLVMVoidType();
                if(LLVMIsAAllocaInst(left)){
                    leftType = LLVMGetAllocatedType(left);
                    left = LLVMBuildLoad(builder, left, "tmpLoad");
                }
                else
                    leftType = LLVMTypeOf(left);
                if(LLVMIsAAllocaInst(right)){
                    rightType = LLVMGetAllocatedType(right);
                    right = LLVMBuildLoad(builder, right, "tmpLoad");
                }
                else
                    rightType = LLVMTypeOf(right);
                if((leftType != LLVMInt64Type()) || (rightType != LLVMInt64Type())){
                    if(leftType == LLVMInt64Type()){
                        left = LLVMBuildSIToFP(builder, left, LLVMDoubleType(), "dconvtmp");
                    }
                    else
                        right = LLVMBuildSIToFP(builder, right, LLVMDoubleType(), "dconvtmp");
                }
#define IFINT() \
                if(leftType == LLVMInt64Type() && rightType == LLVMInt64Type())
#define IFTYPECHECK(insIfInt, tempName) \
                IFINT() \
                    return LLVMBuild##insIfInt(builder, left, right, "i" tempName); \
                return LLVMBuildF##insIfInt(builder, left, right, "f" tempName);
                switch(e->token.type){
                    case TOKEN_plus:
                        IFTYPECHECK(Add, "addtmp")
                    case TOKEN_minus:
                            IFTYPECHECK(Sub, "subtmp")
                    case TOKEN_star:
                                IFTYPECHECK(Mul, "multmp")
                    case TOKEN_backslash:
                                    IFINT()
                                        return LLVMBuildSDiv(builder, left, right, "idivtmp");
                                    return LLVMBuildFDiv(builder, left, right, "fdivtmp");
                    case TOKEN_greater:
                                    IFINT(){
                                        return LLVMBuildICmp(builder, LLVMIntSGT, left, right, "igttmp");
                                    }
                                    else
                                        return LLVMBuildFCmp(builder, LLVMRealOGT, left, right, "fgttmp");
                    case TOKEN_greater_equal:
                                    IFINT(){
                                        return LLVMBuildICmp(builder, LLVMIntSGE, left, right, "igetmp");
                                    }
                                    else
                                        return LLVMBuildFCmp(builder, LLVMRealOGE, left, right, "fgetmp");
                    case TOKEN_lesser:
                                    IFINT(){
                                        return LLVMBuildICmp(builder, LLVMIntSLT, left, right, "ilttmp");
                                    }
                                    else
                                        return LLVMBuildFCmp(builder, LLVMRealOLT, left, right, "flttmp");
                    case TOKEN_lesser_equal:
                                    IFINT(){
                                        return LLVMBuildICmp(builder, LLVMIntSLE, left, right, "iletmp");
                                    }
                                    else
                                        return LLVMBuildFCmp(builder, LLVMRealOLE, left, right, "fletmp");
                    case TOKEN_equal_equal:
                                    IFINT(){
                                        return LLVMBuildICmp(builder, LLVMIntEQ, left, right, "ieqtmp");
                                    }
                                    else
                                        return LLVMBuildFCmp(builder, LLVMRealOEQ, left, right, "feqtmp");
                    case TOKEN_not_equal:
                                    IFINT(){
                                        return LLVMBuildICmp(builder, LLVMIntNE, left, right, "inetmp");
                                    }
                                    else
                                        return LLVMBuildFCmp(builder, LLVMRealONE, left, right, "fnetmp");
                    default: // TOKEN_cap
                                    {
                                        if(LLVMTypeOf(left) != LLVMDoubleType()){
                                            left = LLVMBuildSIToFP(builder, left, LLVMDoubleType(), "dcasttmp");
                                        }
                                        else
                                            LLVMDumpValue(left);
                                        if(LLVMTypeOf(right) != LLVMDoubleType()){
                                            right = LLVMBuildSIToFP(builder, right, LLVMDoubleType(), "dcasttmp");
                                        }
                                        LLVMValueRef fn = LLVMGetNamedFunction(module, "pow");
                                        if(fn == NULL){
                                            LLVMTypeRef params[] = {LLVMDoubleType(), LLVMDoubleType()};
                                            LLVMTypeRef ret = LLVMDoubleType();
                                            LLVMTypeRef fType = LLVMFunctionType(ret, params, 2, 0);
                                            fn = LLVMAddFunction(module, "pow", fType);
                                        }
                                        LLVMValueRef ref[] = {left, right};
                                        return LLVMBuildCall(builder, fn, ref, 2, "__algi_internal_pow_res");
                                    }
                }
            }
        case EXPR_REFERENCE:
            {
                //LLVMValueRef ref = get_variable_ref(e, 0);
                return LLVMConstNull(LLVMInt1Type());
            }
        default: // EXPR_CALL. EXPR_DEFINE will be handled by the statements
            {
                LLVMValueRef args[e->calex.arity];
                for(uint64_t i = 0;i < e->calex.arity;i++){
                    args[i] = expr_compile(e->calex.args[i], context, builder, module);
                }
                char *fName = (char *)malloc(e->token.length + 1);
                strncpy(fName, e->token.string, e->token.length);
                fName[e->token.length] = 0;
                LLVMValueRef fn = LLVMGetNamedFunction(module, fName);
                return LLVMBuildCall(builder, fn, args, e->calex.arity, "__algi_internal_func_res");
            }
    }
}

static LLVMValueRef blockstmt_compile(BlockStatement, LLVMBuilderRef, LLVMModuleRef, LLVMContextRef, uint8_t);

static LLVMValueRef statement_compile(Statement *s, LLVMBuilderRef builder, LLVMModuleRef module, LLVMContextRef context){
    switch(s->type){
        case STATEMENT_SET:
            {
                LLVMValueRef target = expr_compile(s->sets.target, context, builder, module);
                LLVMValueRef value = expr_compile(s->sets.value, context, builder, module);
                if(s->sets.value->valueType == VALUE_STR){
                    if(LLVMGetTypeKind(LLVMTypeOf(value))
                        == LLVMArrayTypeKind){
                        size_t length;
                        LLVMValueRef vRef = LLVMBuildGlobalString(builder, LLVMGetAsString(value, &length), "gString");
                        LLVMValueRef idx[] = {LLVMConstInt(LLVMInt32Type(), 0, 0), LLVMConstInt(LLVMInt32Type(), 0, 0)};
                        //value = LLVMBuildStore(builder, LLVMBuildGEP(builder, vRef, idx, 2, "gStringGEP"), target);
                        
                        if(s->sets.target->valueType != VALUE_GEN)
                            return LLVMBuildStore(builder, LLVMBuildGEP(builder, vRef, idx, 2, "gStringGEP"), target);
                        else
                            value = vRef;
                    }
                    else if(LLVMIsAAllocaInst(value)){
                        value = LLVMBuildLoad(builder, value, "tmpStringLoad");
                    }
                }
                if(s->sets.target->valueType == VALUE_GEN){
                    if(LLVMIsAAllocaInst(value))
                        value = LLVMBuildLoad(builder, value, "tmpValLoad");
                    LLVMTypeRef argType[] = {LLVMPointerType(get_generic_structure_type(), 0),
                                LLVMInt32Type(), LLVMTypeOf(value)};
                    LLVMTypeRef fType = LLVMFunctionType(LLVMVoidType(), argType, 2, 1);
                    LLVMValueRef fn;
                    if((fn = LLVMGetNamedFunction(module, "__algi_generic_store")) == NULL)
                        fn = LLVMAddFunction(module, "__algi_generic_store", fType);
                    runtime_function_used[ALGI_GENERIC_STORE] = 1;
                    LLVMValueRef r[3];
                    r[0] = target;
                    r[1] = LLVMConstInt(LLVMInt32Type(), s->sets.value->valueType, 0);
                    r[2] = value;
                    return LLVMBuildCall(builder, fn, r, 3, "");
                }
                // if(LLVMTypeOf(target) != LLVMTypeOf(value)){
                //     
                // }
                return LLVMBuildStore(builder, value, target);
            }
            break;
        case STATEMENT_IF:
            {
                LLVMValueRef r = expr_compile(s->ifs.condition, context, builder, module);
                //r = LLVMBuildFCmp(builder, LLVMRealOEQ, r, LLVMConstReal(LLVMDoubleType(), 0.0), "ifcond");
                LLVMValueRef parent = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));

                LLVMBasicBlockRef thenBB = LLVMAppendBasicBlockInContext(context, parent, "then");
                LLVMBasicBlockRef elseBB = LLVMAppendBasicBlock(parent, "else");
                LLVMBasicBlockRef mergeBB = LLVMAppendBasicBlock(parent, "ifcont");

                LLVMBuildCondBr(builder, r, thenBB, elseBB);

                LLVMPositionBuilderAtEnd(builder, thenBB);

                blockstmt_compile(s->ifs.thenBlock, builder, module, context, 0);

                LLVMBuildBr(builder, mergeBB);

                //thenBB = LLVMGetInsertBlock(builder);

                //LLVMInsertBasicBlock(elseBB, "else");
                LLVMPositionBuilderAtEnd(builder, elseBB);

                LLVMValueRef elseV = NULL;
                if(s->ifs.elseIf == NULL && s->ifs.elseBlock.count > 0)
                    elseV = blockstmt_compile(s->ifs.elseBlock, builder, module, context, 0);
                else if(s->ifs.elseIf != NULL)
                    elseV = statement_compile(s->ifs.elseIf, builder, module, context);

                LLVMBuildBr(builder, mergeBB);

                //elseBB = LLVMGetInsertBlock(builder);

                //LLVMInsertBasicBlock(mergeBB, "merge");
                LLVMPositionBuilderAtEnd(builder, mergeBB);

                //LLVMValueRef phi = LLVMBuildPhi(builder, LLVMDoubleType(), "ifphi");
                //LLVMAddIncoming(phi, &thenV, &thenBB, 1);
                //if(elseV != NULL)
                //    LLVMAddIncoming(phi, &elseV, &elseBB, 1);
                return LLVMConstInt(LLVMInt1Type(), 0, 0);
            }
            break;
        case STATEMENT_WHILE:
            {

                LLVMValueRef parent = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
                LLVMBasicBlockRef wCond = LLVMAppendBasicBlock(parent, "wcond");
                LLVMBuildBr(builder, wCond);
                LLVMPositionBuilderAtEnd(builder, wCond);

                LLVMValueRef condV = expr_compile(s->whiles.condition, context, builder, module);

                LLVMBasicBlockRef wbody = LLVMAppendBasicBlock(parent, "wbody");
                LLVMBasicBlockRef cont = LLVMAppendBasicBlock(parent, "wcont");

                LLVMBuildCondBr(builder, condV, wbody, cont);

                LLVMPositionBuilderAtEnd(builder, wbody);

                blockstmt_compile(s->whiles.statements, builder, module, context, 0);

                LLVMBuildBr(builder, wCond);
                //LLVMBuildCondBr(builder, condV, wbody, cont);

                LLVMPositionBuilderAtEnd(builder, cont);
                //LLVMBuildRetVoid(builder);

                //LLVMBuildBr(builder, parent);

                return LLVMConstInt(LLVMInt1Type(), 0, 0);
            }
        case STATEMENT_DO:
            {
                LLVMValueRef parent = LLVMGetBasicBlockParent(LLVMGetInsertBlock(builder));
                LLVMBasicBlockRef dbody = LLVMAppendBasicBlock(parent, "dbody");
                LLVMBuildBr(builder, dbody);
                LLVMPositionBuilderAtEnd(builder, dbody);
                blockstmt_compile(s->dos.statements, builder, module, context, 0);
                LLVMValueRef cond = expr_compile(s->dos.condition, context, builder, module);
                LLVMBasicBlockRef cont = LLVMAppendBasicBlock(parent, "dcont");
                LLVMBuildCondBr(builder, cond, dbody, cont);
                LLVMPositionBuilderAtEnd(builder, cont);
                return LLVMConstInt(LLVMInt1Type(), 0, 0);
            }
        case STATEMENT_PRINT:
            {
                LLVMValueRef params[2];
                LLVMValueRef ret;
                for(uint64_t i = 0;i < s->prints.count;i++){
                    LLVMValueRef val = expr_compile(s->prints.args[i], context, builder, module);
                    LLVMTypeRef valRef = LLVMVoidType();
                    if(LLVMIsAAllocaInst(val)){
                        //dbg("It is alloca dude!\n");
                        valRef = LLVMGetAllocatedType(val);
                        val = LLVMBuildLoad(builder, val, "valueLoad");
                    }
                    else
                        valRef = LLVMTypeOf(val);
                    LLVMTypeRef paramTypes[2];
                    LLVMValueRef idx[] = {LLVMConstInt(LLVMInt32Type(), 0, 0),
                        LLVMConstInt(LLVMInt32Type(), 0, 0)};
                    LLVMValueRef fspec;
                    if(valRef == get_generic_structure_type()){
                        //fspec = LLVMBuildGlobalString(builder, "%g", "genspec");
                        LLVMTypeRef params[] = {get_generic_structure_type()};
                        LLVMTypeRef agvpt = LLVMFunctionType(LLVMVoidType(), params, 1, 0);
                        LLVMValueRef ref;
                        if((ref = LLVMGetNamedFunction(module, "__algi_generic_print")) == NULL){
                            runtime_function_used[ALGI_GENERIC_PRINT] = 1;
                            ref = LLVMAddFunction(module, "__algi_generic_print", agvpt);
                        }
                        LLVMValueRef args[] = {val};
                        return LLVMBuildCall(builder, ref, args, 1, "");
                    }
                    else if(valRef == LLVMInt64Type()){ 
                        if((fspec = LLVMGetNamedGlobal(module, "intSpec")) == NULL)
                            fspec = LLVMBuildGlobalString(builder, "%" PRId64, "intSpec");

                        paramTypes[1] = LLVMInt64Type();
                        params[1] = val; 
                    }

                    else if(valRef == LLVMDoubleType()){
                        if((fspec = LLVMGetNamedGlobal(module, "floatSpec")) == NULL)
                            fspec = LLVMBuildGlobalString(builder, "%.10g", "floatSpec");

                        paramTypes[1] = LLVMDoubleType();
                        params[1] = val; 
                    }
                    else{
                        if((fspec = LLVMGetNamedGlobal(module, "strSpec")) == NULL)
                            fspec = LLVMBuildGlobalString(builder, "%s", "strSpec");

                        paramTypes[1] = LLVMPointerType(LLVMInt8Type(), 0); 


                        if(valRef == LLVMPointerType(LLVMInt8Type(), 0))
                            params[1] = val;
                        else if(valRef == LLVMInt1Type()){
                           
                            params[1] = LLVMBuildSelect(builder, val,
                                LLVMBuildInBoundsGEP(builder, LLVMBuildGlobalString(builder, "True\0", "boolResT"),
                                        idx, 2, "gep"), 
                                LLVMBuildInBoundsGEP(builder, LLVMBuildGlobalString(builder, "False\0", "boolResF"),
                                        idx, 2, "gep"), "boolSelection");
                            //params[1] = LLVMBuildInBoundsGEP(builder, params[1], idx, 2, "str");
                        }
                        else {
                            size_t length;
                            const char *str = LLVMGetAsString(val, &length);
                            params[1] = LLVMBuildGlobalString(builder, str, "pString");

                            params[1] = LLVMBuildInBoundsGEP(builder, params[1], idx, 2, "str");
                        }
                    }

                    params[0] = LLVMBuildInBoundsGEP(builder, fspec, idx, 2, "gep");
                    paramTypes[0] = LLVMPointerType(LLVMInt8Type(), 0);

                    LLVMTypeRef ftype = LLVMFunctionType(LLVMInt32Type(), paramTypes, 1, 1);

                    LLVMValueRef pf;
                    if((pf = LLVMGetNamedFunction(module, "printf")) == NULL)
                        pf = LLVMAddFunction(module, "printf", ftype);

                    //dbg("builder : %p\n");
                    //dbg("params : %p\n", params);
                    //dbg("pf : %p\n", pf);

                    ret = LLVMBuildCall(builder, pf, params, 2, "pftemp");
                }
                return ret;
            }
            break;
        default:
            return LLVMConstInt(LLVMInt1Type(), 1, 0);
    }
}

static LLVMValueRef func;

static LLVMValueRef blockstmt_compile(BlockStatement s, LLVMBuilderRef builder, LLVMModuleRef module, 
        LLVMContextRef context, uint8_t insertBlock){
    LLVMBasicBlockRef bbr;
    LLVMBasicBlockRef bb;
    if(insertBlock){
        bbr = LLVMGetInsertBlock(builder);
        if(bbr != NULL){
            dbg("GetInsertBlock : %p\n", bbr);
            LLVMValueRef bbp = LLVMGetBasicBlockParent(bbr);
            dbg("GetBasicBlockParent : %p\n", bbp); 

            bb = LLVMAppendBasicBlock(
                    bbp, "block");
        }
        else{
            dbg("Creating new block");
            bb = LLVMAppendBasicBlock(func, "block");
        }
        LLVMPositionBuilderAtEnd(builder, bb);
    }
    LLVMValueRef ret;
    for(uint64_t i = 0;i < s.count;i++){
        ret = statement_compile(s.statements[i], builder, module, context);
    }
    if(insertBlock){
        LLVMBuildRetVoid(builder);
        return LLVMBasicBlockAsValue(bb);
    }
    else
        return ret;
}

void codegen_compile(BlockStatement bs){
    LLVMModuleRef module = LLVMModuleCreateWithName("AlgiModule");
    LLVMTypeRef ret_type = LLVMFunctionType(LLVMVoidType(), NULL, 0, 0);

    func = LLVMAddFunction(module, "Main", ret_type);

    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMContextRef context = LLVMContextCreate();

    timer_start("Compilation");

    blockstmt_compile(bs, builder, module, context, 1);

    timer_end();

    char *err = NULL;
    dbg("Compiled code\n");
    LLVMDumpModule(module);
    LLVMVerifyModule(module, LLVMPrintMessageAction, &err);
    LLVMDisposeMessage(err);

    LLVMExecutionEngineRef engine;
    err = NULL;

    timer_start("Initializing JIT");

    LLVMLinkInMCJIT();

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    if(LLVMCreateExecutionEngineForModule(&engine, module, &err) != 0){
        printf("\nFailed to create execution engine!\n");
        abort();
    }
    if(err){
        printf("\nError : %s\n", err);
        LLVMDisposeMessage(err);
        exit(EXIT_FAILURE);
    }

    // Map Algi runtime functions
    for(uint64_t i = 0;i < ALGI_RUNTIME_FUNCTION_COUNT;i++){
        if(runtime_function_used[i]){
            LLVMAddGlobalMapping(engine, LLVMGetNamedFunction(module, algi_runtime_functions[i].name),
                    algi_runtime_functions[i].address);
        }
    }

    LLVMSetModuleDataLayout(module, LLVMGetExecutionEngineTargetData(engine));

    timer_end();

    timer_start("Optimization");
    // Optimization
    LLVMPassManagerBuilderRef pmb = LLVMPassManagerBuilderCreate();
    LLVMPassManagerBuilderSetOptLevel(pmb, 2);
    LLVMPassManagerRef optimizer = LLVMCreatePassManager();

    LLVMPassManagerBuilderPopulateModulePassManager(pmb, optimizer);
    LLVMRunPassManager(optimizer, module);
    timer_end();
    LLVMDisposePassManager(optimizer);
    LLVMPassManagerBuilderDispose(pmb);

    dbg("Optimized code \n");
    LLVMDumpModule(module);

    dbg("Press any key to run the program");
    dbg("Press C to cancel : ");
    char c = getc(stdin);
    if(c != 'c' && c != 'C'){
        timer_start("Execution");
        void(*Main)(void) = (void (*)(void))LLVMGetFunctionAddress(engine, "Main");
        Main();
        timer_end();
    }
    else
        warn("Run cancelled!");

    LLVMDisposeBuilder(builder);
    LLVMDisposeExecutionEngine(engine);
    LLVMContextDispose(context);
    LLVMShutdown();
}
