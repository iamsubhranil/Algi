#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <llvm-c/Types.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
//#include <llvm-c/ErrorHandling.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/OrcBindings.h>

// Optimizations
#include <llvm-c/Transforms/IPO.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/Transforms/Vectorize.h>

#include <string.h>

#include "expr.h"
#include "stmt.h"
#include "types.h"
#include "display.h"
#include "codegen.h"

typedef struct{
    LLVMValueRef ref;
    uint64_t hash;
} VariableRef;

static uint64_t variableRefPointer = 0;
static VariableRef *variables = NULL;

static LLVMValueRef get_generic_structure(){
    LLVMValueRef type = LLVMConstInt(LLVMInt64Type(), 0, 0);
    LLVMValueRef storage = LLVMConstInt(LLVMInt64Type(), 0, 0);
    LLVMValueRef values[] = {type, storage};
    return LLVMConstStruct(values, 2, 0);
}

//static LLVMValueRef get_container_structure(){
//    LLVMValueRef 
//}

static LLVMValueRef get_variable_ref(Expression *varE, uint8_t declareIfNotfound, LLVMBuilderRef builder){
    //varE->hash = hash(varE->token.string, varE->token.length);
    dbg("Searching for hash %ld", varE->hash);
    for(uint64_t i = 0;i < variableRefPointer;i++){

        if(variables[i].hash == varE->hash){
            return variables[i].ref;
        }
    }
    if(declareIfNotfound){
        dbg("Declaring ");
        lexer_print_token(varE->token, 0);
        variableRefPointer++;
        variables = (VariableRef *)realloc(variables, sizeof(VariableRef) * variableRefPointer);
        LLVMValueRef ref;
        switch(varE->valueType){
            case 6:
                ref = get_generic_structure();
                break;
            case 5:
                ref = LLVMBuildAlloca(builder, LLVMPointerType(LLVMInt8Type(), 0), "localString");
                break;
            case 4:
                ref = LLVMBuildAlloca(builder, LLVMDoubleType(), "localDouble");
                break;
            case 3:
                ref = LLVMBuildAlloca(builder, LLVMInt64Type(), "localInt");
                break;
            case 2:
                ref = get_generic_structure();
                break;
            case 1:
                ref = LLVMBuildAlloca(builder, LLVMInt1Type(), "localBool");
                break;
        }
        variables[variableRefPointer - 1].ref = ref;
        variables[variableRefPointer - 1].hash = varE->hash;
        return ref;
    }
    return LLVMConstNull(LLVMInt1Type());
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
                        return LLVMConstString(e->consex.sval, e->token.length, 0);
                    case TOKEN_True:
                        return LLVMConstInt(LLVMInt1Type(), 1, 0);
                    case TOKEN_False:
                        return LLVMConstInt(LLVMInt1Type(), 0, 0);
                    default:
                        return LLVMConstNull(LLVMInt1Type());
                }
            }
        case EXPR_UNARY:
            {
                LLVMValueRef expVal = expr_compile(e->unex.right, context, builder, module);
                switch(e->token.type){
                    case TOKEN_minus:
                        return LLVMBuildNeg(builder, expVal, "negtmp"); 
                    case TOKEN_not:
                        return LLVMBuildNot(builder, expVal, "nottmp");
                    case TOKEN_Integer:
                        return LLVMBuildIntCast(builder, expVal, LLVMInt64Type(), "intcasttmp");
                    case TOKEN_Number:
                        return LLVMBuildFPCast(builder, expVal, LLVMDoubleType(), "doublecasttmp");
                    case TOKEN_Structure:
                        return LLVMBuildPointerCast(builder, expVal, LLVMInt64Type(), "pointercasttmp");
                    case TOKEN_Boolean:
                        return LLVMBuildIntCast(builder, expVal, LLVMInt1Type(), "boolcasttmp");
                    case TOKEN_String:
                        return LLVMBuildPointerCast(builder, expVal, LLVMInt8Type(), "strcasttmp");
                    default:
                        // TODO: Handle this properly
                        return LLVMConstNull(LLVMInt1Type());
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
                    if(leftType == LLVMInt64Type() && rightType == LLVMInt64Type()) \
                            return LLVMBuild##insIfInt(builder, left, right, "i" #tempName); \
                    return LLVMBuildF##insIfInt(builder, left, right, "f" #tempName);
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
                            LLVMValueRef fn = LLVMGetNamedFunction(module, "pow");
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

static char* format_string(const char *str, size_t length){
    char *ret = strdup(str);
    for(size_t i = 0;i < length;i++){
        if(str[i] == '\\'){
            if(str[i+1] == 'n')
                ret[i] = ' ', ret[i+1] = '\n';
            else if(str[i+1] == 't')
                ret[i] = ' ', ret[i+1] = '\t';
        }
    }
    return ret;
}

static LLVMValueRef statement_compile(Statement *s, LLVMBuilderRef builder, LLVMModuleRef module, LLVMContextRef context){
    switch(s->type){
        case STATEMENT_SET:
            {
                LLVMValueRef target = expr_compile(s->sets.target, context, builder, module);
                LLVMValueRef value = expr_compile(s->sets.value, context, builder, module);
                if(s->sets.value->valueType == VALUE_STR){
                    size_t length;
                    LLVMValueRef vRef = LLVMBuildGlobalString(builder, LLVMGetAsString(value, &length), "gString");
                    LLVMValueRef idx[] = {LLVMConstInt(LLVMInt32Type(), 0, 0), LLVMConstInt(LLVMInt32Type(), 0, 0)};
                    return LLVMBuildStore(builder, LLVMBuildGEP(builder, vRef, idx, 2, "gStringGEP"), target);
                }
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

                thenBB = LLVMGetInsertBlock(builder);

                //LLVMInsertBasicBlock(elseBB, "else");
                LLVMPositionBuilderAtEnd(builder, elseBB);

                LLVMValueRef elseV = NULL;
                if(s->ifs.elseIf == NULL && s->ifs.elseBlock.count > 0)
                    elseV = blockstmt_compile(s->ifs.elseBlock, builder, module, context, 0);
                else if(s->ifs.elseIf != NULL)
                    elseV = statement_compile(s->ifs.elseIf, builder, module, context);

                LLVMBuildBr(builder, mergeBB);

                elseBB = LLVMGetInsertBlock(builder);

                //LLVMInsertBasicBlock(mergeBB, "merge");
                LLVMPositionBuilderAtEnd(builder, mergeBB);

                //LLVMValueRef phi = LLVMBuildPhi(builder, LLVMDoubleType(), "ifphi");
                //LLVMAddIncoming(phi, &thenV, &thenBB, 1);
                //if(elseV != NULL)
                //    LLVMAddIncoming(phi, &elseV, &elseBB, 1);
                return LLVMConstInt(LLVMInt1Type(), 0, 0);
            }
            break;
        case STATEMENT_PRINT:
            {
                LLVMValueRef params[2];
                LLVMValueRef ret;
                for(uint64_t i = 0;i < s->prints.count;i++){
                    LLVMValueRef val = expr_compile(s->prints.args[i], context, builder, module);
                    LLVMTypeRef valRef = LLVMVoidType();
                    if(LLVMIsAAllocaInst(val)){
                        dbg("It is alloca dude!\n");
                        valRef = LLVMGetAllocatedType(val);
                        val = LLVMBuildLoad(builder, val, "valueLoad");
                    }
                    else
                        valRef = LLVMTypeOf(val);
                    LLVMTypeRef paramTypes[2];
                    LLVMValueRef idx[] = {LLVMConstInt(LLVMInt32Type(), 0, 0),
                        LLVMConstInt(LLVMInt32Type(), 0, 0)};
                    LLVMValueRef fspec;
                    if(valRef == LLVMInt64Type()){ 
                        if((fspec = LLVMGetNamedGlobal(module, "intSpec")) == NULL)
                            fspec = LLVMBuildGlobalString(builder, "%ld", "intSpec");

                        paramTypes[1] = LLVMInt64Type();
                        params[1] = val; 
                    }
                    
                    else if(valRef == LLVMDoubleType()){
                        if((fspec = LLVMGetNamedGlobal(module, "floatSpec")) == NULL)
                            fspec = LLVMBuildGlobalString(builder, "%g", "floatSpec");

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
                            unsigned long long lval = LLVMConstIntGetZExtValue(val);
                            if(lval && (params[1] = LLVMGetNamedGlobal(module, "trueString")) == NULL)
                                params[1] = LLVMBuildGlobalString(builder, "True\0", "trueString");
                            if(!lval && (params[1] = LLVMGetNamedGlobal(module, "falseString")) == NULL)
                                params[1] = LLVMBuildGlobalString(builder, "False\0", "falseString");

                            params[1] = LLVMBuildInBoundsGEP(builder, params[1], idx, 2, "str");
                        }
                        else {
                            size_t length;
                            const char *str = LLVMGetAsString(val, &length);
                            char *formattedString = format_string(str, length);
                            params[1] = LLVMBuildGlobalString(builder, formattedString, "pString");

                            params[1] = LLVMBuildInBoundsGEP(builder, params[1], idx, 2, "str");
                        }
                    }

                    params[0] = LLVMBuildInBoundsGEP(builder, fspec, idx, 2, "gep");
                    paramTypes[0] = LLVMPointerType(LLVMInt8Type(), 0);

                    LLVMTypeRef ftype = LLVMFunctionType(LLVMInt32Type(), paramTypes, 1, 1);

                    LLVMValueRef pf;
                    if((pf = LLVMGetNamedFunction(module, "printf")) == NULL)
                        pf = LLVMAddFunction(module, "printf", ftype);

                    dbg("builder : %p\n");
                    dbg("params : %p\n", params);
                    dbg("pf : %p\n", pf);

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
        else
            bb = LLVMAppendBasicBlock(func, "block");
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

    // blockstmt_compile(bs, builder, module, context);

    //LLVMBasicBlockRef coreBB = LLVMAppendBasicBlock(entry, "entry");
    // LLVMPositionBuilderAtEnd(builder, coreBB);

    blockstmt_compile(bs, builder, module, context, 1);
    //LLVMBuildRetVoid(builder);

    char *err = NULL;
    LLVMDumpModule(module);
    LLVMVerifyModule(module, LLVMPrintMessageAction, &err);
    LLVMDisposeMessage(err);

    LLVMExecutionEngineRef engine;
    err = NULL;
    LLVMLinkInMCJIT();

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();
        
    //LLVMViewFunctionCFG(func);

    if(LLVMCreateExecutionEngineForModule(&engine, module, &err) != 0){
        printf("\nFailed to create execution engine!\n");
        abort();
    }
    if(err){
        printf("\nError : %s\n", err);
        LLVMDisposeMessage(err);
        exit(EXIT_FAILURE);
    }

    LLVMSetModuleDataLayout(module, LLVMGetExecutionEngineTargetData(engine));
      
    // Optimization
    LLVMPassManagerBuilderRef pmb = LLVMPassManagerBuilderCreate();
    LLVMPassManagerBuilderSetOptLevel(pmb, 3);
    LLVMPassManagerRef optimizer = LLVMCreatePassManager();
    //LLVMTargetMachineRef machine = LLVMGetExecutionEngineTargetMachine(engine);
   
    //LLVMAddMemCpyOptPass(optimizer);
    //LLVMAddCFGSimplificationPass(optimizer);
    //LLVMAddConstantMergePass(optimizer);
    //LLVMAddBasicAliasAnalysisPass(optimizer);
    //LLVMAddInstructionCombiningPass(optimizer);
    //LLVMAddGVNPass(optimizer);
    //LLVMAddPromoteMemoryToRegisterPass(optimizer);

    LLVMPassManagerBuilderPopulateModulePassManager(pmb, optimizer);
    LLVMRunPassManager(optimizer, module);
    LLVMDisposePassManager(optimizer);
    LLVMPassManagerBuilderDispose(pmb);
    //LLVMAddAnalysisPasses(machine, optimizer);

    dbg("Optimized code \n");
    LLVMDumpModule(module);

    dbg("Running program\n");
    void(*Main)(void) = (void (*)(void))LLVMGetFunctionAddress(engine, "Main");
    Main();

    LLVMDisposeBuilder(builder);
    LLVMDisposeExecutionEngine(engine);
    LLVMContextDispose(context);
    LLVMShutdown();
}

int main2(int argc, char *argv[]){
    LLVMModuleRef mod = LLVMModuleCreateWithName("MyModule");
    LLVMTypeRef params[] = {LLVMInt32Type(), LLVMInt32Type()};
    LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), params, 2, 0);
    LLVMValueRef sum = LLVMAddFunction(mod, "sum", ret_type);
    LLVMBasicBlockRef entry = LLVMAppendBasicBlock(sum, "entry");
    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMPositionBuilderAtEnd(builder, entry);
    LLVMValueRef tmp = LLVMBuildAdd(builder, LLVMGetParam(sum, 0), LLVMGetParam(sum, 1), "tmp");
    LLVMBuildRet(builder, tmp);

    char *err = NULL;
    LLVMVerifyModule(mod, LLVMAbortProcessAction, &err);
    LLVMDisposeMessage(err);

    LLVMExecutionEngineRef engine;
    err = NULL;
    LLVMLinkInMCJIT();

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmParser();
    LLVMInitializeNativeAsmPrinter();

    if(LLVMCreateExecutionEngineForModule(&engine, mod, &err) != 0){
        printf("\nFailed to create execution engine!\n");
        abort();
    }
    if(err){
        printf("\nError : %s\n", err);
        LLVMDisposeMessage(err);
        exit(EXIT_FAILURE);
    }
    if(argc < 3){
        printf("\nError!\nUsage : %s x y\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    long long x = strtoll(argv[1], NULL, 10);
    long long y = strtoll(argv[2], NULL, 10);

    /*LLVMGenericValueRef args[] = {
      LLVMCreateGenericValueOfInt(LLVMInt32Type(), x, 0),
      LLVMCreateGenericValueOfInt(LLVMInt32Type(), y, 0)
      };*/

    int(*function)(int, int) = (int (*)(int, int))LLVMGetFunctionAddress(engine, "sum");

    printf("\nResult : %d\n", function(x, y));

    LLVMDisposeBuilder(builder);
    LLVMDisposeExecutionEngine(engine);
    return 0;
}
