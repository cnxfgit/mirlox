#ifndef clox_jit_h
#define clox_jit_h


#include "common.h"
#include "object.h"
#include "vm.h"




void jitCompile(VM *vm, ObjClosure *closure, int argCount);

#endif