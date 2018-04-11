# Algi
### An algorithmic language

Building
===========
    
    1. Clone the repo

    2. Install LLVM library and headers (check your distro specific packages)
    
    3. `cd Algi`

    4. `clang *.c $(llvm-config --libs)`

Running Tests
================

All the tests in `tests/type2` folder is presently supported. Ideally, when the implementation is complete, Algi should run all tests inside the `test` folder. But as I'm implementing it one bit at a time, only a subset of the desired functionality is supported.

Bear in mind, when running any performance test using a generic variable, like `tests/type2/optionalperf.algi`, the performance will largely depend on the compilation optimization level of Algi itself, since it calls a lot of runtime hooks directly into the `Algi Runtime Library`, which is compiled with Algi.
