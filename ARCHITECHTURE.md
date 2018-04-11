so here's the basic, pre-alpha implementation of the JITed dynamically typed language that I talked about in the previous post

The implementation is not very polished, might generate inefficient JIT code, and I certainly plan to shape it over time, but I think LLVM passes do a good job of optimizing it anyway, so keep an open mind.

The goal of this post is to show that my idea works, and the goal of the language is to have an easy switch between utmost verbosity and Layman's simplicity, with the cost of performance, at the programmer level. Same compiler should produce efficient code when variables are typed, and is free to produce any runtime hooks for variables which are untyped. 

Which brings us to the representation of an untyped variable. We are JITing it, so ultimately every variable has to have a type. In Algi, untyped variables are represented with a structure [AlgiGenericValue](https://github.com/iamsubhranil/Algi/blob/master/runtime.h#L6), which is the most cliche structure you'll find anywhere to hold multiple types of value - a type tag and an union of different types. On the LLVM side, the structure is consisted of [two](https://github.com/iamsubhranil/Algi/blob/master/codegen.c#L36) values - one `LLVMInt8Type` for holding types and another `LLVMInt64Type` for holding the data, since we would atmost store 64bits of data in any variable anyway (mind you, strings are pointers). Although the size is correct, implicit integer consideration of the data brings us to some interesting problems, which I'm going to discuss a bit later. But since there is no concept of an `union` in LLVM (or atleast not one that I know of), I have to do away with it anyway.

Although I discussed the representation of the untyped variables first, they came much later into the scene. The first thing was to get the JIT up and running as fast as possible, and hence the first variables I thought of handling was the typed variables, obviously. After parsing, the AST passes through a [type checker](https://github.com/iamsubhranil/Algi/blob/master/types.c) (or rather, a forgivable type enforcer, because Algi allows some interesting casts at runtime), which labels each node by an appropriate type, if possible. When the declaration of the variable doesn't match the behavior of the operator where the variable is used, based on the context, either an [error](https://github.com/iamsubhranil/Algi/blob/master/types.c#L109) or an [implicit type cast](https://github.com/iamsubhranil/Algi/blob/master/types.c#L450) is generated. For example, if we write,
```
Number n = 0
Integer i = 1
Set n = n + i
```
The variable i is implicitly typecasted to a `Number` on the use of the operator `+`, the datatype of the floating point variables in Algi. We do not, obviously, perform an implicit cast between two variables of different types, like a `Number` a `String`. They are [convertible](https://github.com/iamsubhranil/Algi/blob/master/runtime.c#L83) though, by an explicit typecast which is evaluted at runtime, like the following :
```
String s = "38.382"
Number n = Number(s)
```
In this type of code, we can't really tell if the string is convertible to a floating point number unless we actually run it, and hence the type casting is moved to the runtime by using a hook to the [Algi Runtime Library](https://github.com/iamsubhranil/Algi/blob/master/runtime.c)(or `ARL`).

Since we want to make the untyped variables absolutely transparent with the typed ones, we have to perform some implicit casting to all variables of the generic type, and the type  checker enforces those too. The type checkers further [validates](https://github.com/iamsubhranil/Algi/blob/master/types.c#L656) the assignment statements and variable scope and visibility, and reports any and all errors to the user. When the AST returns from the type checker, it is guaranteed to have a specific type to all of its nodes, and all operators are guaranteed to have operands of correct types, which basically converts the dynamic language to a static one.

After enforcing the types, the AST is passed to the [code generator](https://github.com/iamsubhranil/Algi/blob/master/codegen.c), which uses the LLVM framework to generate machine code. Here's where the actual catch of the whole design lies. Consider the following lines of code,
```
Set var1 = 821 // a AlgiGenericValue variable
Integer i = 37 // an integer variable
If (i < var1){
Print "\nYou got this!"
}
```
Since var1 is of generic type, what the type checker does is it enforces an [implicit integer cast](https://github.com/iamsubhranil/Algi/blob/master/types.c#L479) of the generic variable before the expression, which converts the expression as if it were written like :
```
Set var1 = 821
Integer i  = 37
If(i < Integer(var1){
Print "\nYou got this"
}
```
Now notice carefully, for an relational operator, the code generator still generates code for an [integer relational operation](https://github.com/iamsubhranil/Algi/blob/master/codegen.c#L314), with an [additional call](https://github.com/iamsubhranil/Algi/blob/master/codegen.c#L235) to the `ARL` for the implicit type conversion of the generic value. If you specify the type, the cast goes away, and code for the relational operator is still generated in the same way. This is exactly where Algi mixes the flexibility of an untyped language and the performance of a typed one.

Every access to a generic variable passes through an ARL call, which performs necessary modifications to the variable and returns the value, if required. If you use generic variables all over the code, Algi will behave more like an interpreter than a compiler, whereas, if you specify all types, Algi will behave exactly like a statically typed compiler.

There's a huge catch though. Since on the LLVM side, we are talking only about a 64 bit integer to store our data, how do we store and retrieve a value of floating point type to and from a generic variable? Well, the answer is not so simple (or is it?). Each store to a generic variable passes through [`__algi_generic_store`](https://github.com/iamsubhranil/Algi/blob/master/runtime.c#L253) which has the following signature :
```
void __algi_generic_store(AlgiGenericValue *val, int32_t type, ...)
```
First argument is the generic variable to store the data into, second is the type of the data that is going to be stored and the third is the data itself. Since we can't tell the type of the value that is going to be stored compile time, we make the function varargs. To store the data, based on the type, we [cast](https://github.com/iamsubhranil/Algi/blob/master/runtime.c#L256) the target value, and store it in the union, like the following :
```
switch(type){
    case VALUE_NUM: // Numeric type
        {
            double d = va_arg(args, double);
            val->dnum = d;
            break;
        }
```
Since all members of the union share same memory location, even if we access the memory location as a `double` and write a `double` value there, in reality, the bitmap of the `LLVMInt64Type` gets rewritten by this operation. The accessing part is a bit more difficult. Especially when we want to access a `double` from that `LLVMInt64Type`. But let's start by the process of extraction itself. Since, as shown above, the generic values go through an implicit typecast everytime they are used, the `build_cast` module, which is responsible for generating code for all types of runtime casting, generates code for extracion of the type and the value of a generic variable, and [pass them directly](https://github.com/iamsubhranil/Algi/blob/master/codegen.c#L176), instead of passing the generic variable to various cast calls. The [cast calls](https://github.com/iamsubhranil/Algi/blob/master/runtime.h#L25) have the following signature :
```
TARGETTYPE __algi_to_TARGETTYPE(int32_t sourceType, ...); // The vararg is casted to appropiate type as required
```
No let's get back to the point of getting a `double` value from an `LLVMInt64Type`. I mean no, that is not so difficult, but what's really difficult is the distinction between these two things :

1. An `int64_t` which is actually int, which is the case for all typed variables
2. An `int64_t` which is actually double, which is the case for all untyped variables

How will the `ARL` know which time the call is the effect for an implicit cast of a generic variable and which time is it the usual?

As it is turned out, and as it is implemented(not yet pushed), when a value is to be extracted an passed from a generic variable, the code generator increments the type like the following :
```
passType = actualType + HIGHEST_TYPE
```
As the highest type in Algi is `VALUE_GEN`, the type of generic value, what's done is `(int)VALUE_GEN` is added with the actual type, whenever a generic variable is implicitly or explicitly typecasted, and then it's value is extracted and passed to the cast calls. The cast methods in `ARL` in turn, checks whether the `sourceType` is greater than `VALUE_GEN`, and if it is, always treats the argument as an `int64_t` and converts it to `double` if required using an union. That way, the bit representation is retained, and any implicit compiler optimization is skipped.

There is still much more to be done, implementing function calls being the foremost. In the meantime, I'm very eager to get your feedbacks. 
