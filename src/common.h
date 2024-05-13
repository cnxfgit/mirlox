//
// Created by Administrator on 2022/7/18.
//

#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// NAN 装箱
#define NAN_BOXING

// 无异常反汇编当前字节码块
//#define DEBUG_PRINT_CODE
// 打印虚拟机栈和反汇编说明
// #define DEBUG_TRACE_EXECUTION

// 频繁调用垃圾回收
//#define DEBUG_STRESS_GC

// 输出gc日志
// #define DEBUG_LOG_GC

// 局部变量最多值
#define UINT8_COUNT (UINT8_MAX + 1)

// 是否开启JIT功能
//#define OPEN_JIT

#ifdef OPEN_JIT
// 是否执行jit的阈值
#define JIT_FACTOR 1000

// 无视阈值总是执行jit
#define JIT_ALWAYS
#endif

#endif