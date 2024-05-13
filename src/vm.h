//
// Created by Administrator on 2022/7/19.
//

#ifndef clox_vm_h
#define clox_vm_h

#include <stdio.h>

#include "object.h"
#include "table.h"
#include "value.h"
#ifdef OPEN_JIT
#include "mir.h"
#include "c2mir.h"
#endif

// 栈数组最大值
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// 调用帧
typedef struct {
    ObjClosure* closure;        // 调用的函数闭包
    uint8_t* ip;                // 指向字节码数组的指针 指函数执行到哪了
    Value* slots;               // 指向vm栈中该函数使用的第一个局部变量
} CallFrame;

// 虚拟机
typedef struct {
    CallFrame frames[FRAMES_MAX];   // 栈帧数组 所有函数调用的执行点
    int frameCount;                 // 当前调用栈数

    Value stack[STACK_MAX];         // 虚拟机栈
    Value* stackTop;                // 栈顶指针 总是指向栈顶
    Table globals;                  // 全局变量表
    Table strings;                  // 全局字符串表
    ObjString* initString;          // 构造器名称
    ObjUpvalue* openUpvalues;       // 全局提升值

    size_t bytesAllocated;          // 已经分配的内存
    size_t nextGC;                  // 出发下一次gc的阈值

    Obj* objects;                   // 对象根链表
    int grayCount;                  // 灰色对象数量
    int grayCapacity;               // 灰色对象容量
    Obj** grayStack;                // 灰色对象栈

#ifdef OPEN_JIT
    MIR_context_t mirContext;
    struct c2mir_options mirOptions;
#endif
} VM;

typedef enum {
    INTERPRET_OK,               // 解释执行成功
    INTERPRET_COMPILE_ERROR,    // 编译期异常
    INTERPRET_RUNTIME_ERROR     // 运行时异常
} InterpretResult;

extern VM vm;

// 初始化虚拟机啊
void initVM();

// 释放虚拟机
void freeVM();

// 解释字节码块
InterpretResult interpret(const char* source);

// 压入虚拟机栈
void push(Value value);

// 弹出虚拟机栈
Value pop();

#endif