//
// Created by Administrator on 2022/7/19.
//

#ifndef clox_compiler_h
#define clox_compiler_h

#include "vm.h"

// 编译
ObjFunction* compile(const char* source);

// 标记编译根对象
void markCompilerRoots();

#endif
