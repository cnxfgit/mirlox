#ifndef clox_jit_h
#define clox_jit_h


#include "common.h"

typedef enum {
    JIT_NOT_COMPILE,
    JIT_CANT_COMPILE,
    JIT_MUST_COMPILE,
    JIT_IS_COMPILED
} JitStatus;

int add();

#endif