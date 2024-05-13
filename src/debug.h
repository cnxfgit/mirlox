//
// Created by Administrator on 2022/7/18.
//

#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

// 反汇编字节码块
void disassembleChunk(Chunk* chunk, const char* name);

// 反汇编说明
int disassembleInstruction(Chunk* chunk, int offset);


#endif