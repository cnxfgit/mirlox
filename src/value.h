//
// Created by Administrator on 2022/7/18.
//

#ifndef clox_value_h
#define clox_value_h

#include <string.h>
#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING


#define SIGN_BIT ((uint64_t)0x8000000000000000)
// 偏移量在64(0-63)位中是  50-62
#define QNAN     ((uint64_t)0x7ffc000000000000)

// 用固定的几个数来表示三种基本类型
#define TAG_NIL   1 // 01.
#define TAG_FALSE 2 // 10.
#define TAG_TRUE  3 // 11.

typedef uint64_t Value;

#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define IS_OBJ(value)       (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value)    valueToNum(value)
#define AS_OBJ(value)       ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num) numToValue(num)
#define OBJ_VAL(obj)    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else


// 值类型
typedef enum {
    VAL_BOOL,   // 布尔类型
    VAL_NIL,    // 空类型
    VAL_NUMBER, // 数字类型
    VAL_OBJ     // 对象类型
} ValueType;

// 基础值
typedef struct {
    ValueType type;         // 值类型
    union {
        bool boolean;       // 布尔类型
        double number;      // 数字类型
        Obj* obj;           // 对象指针
    } as;                   // 类型联合体
} Value;

// 判断值是否为布尔
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
// 判断值是否为空
#define IS_NIL(value)     ((value).type == VAL_NIL)
// 判断值是否为数字
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
// 判断值是否为对象
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

// 值类型转化c的obj
#define AS_OBJ(value)     ((value).as.obj)
// 值类型转化c的布尔
#define AS_BOOL(value)    ((value).as.boolean)
// 值类型转化c的double
#define AS_NUMBER(value)  ((value).as.number)


// 定义值类型布尔
#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
// 定义值类型空
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
// 定义值类型数字
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
// 定义值对象类型
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#endif

// 常量数组
typedef struct {
    int capacity;   // 常量数组当前容量
    int count;      // 常量数组当前长度
    Value* values;  // 常量数组
} ValueArray;

// 比较两值是否相等
bool valuesEqual(Value a, Value b);

// 初始化常量数组
void initValueArray(ValueArray* array);

// 写入常量数组
void writeValueArray(ValueArray* array, Value value);

// 释放常量数组
void freeValueArray(ValueArray* array);

// 打印值
void printValue(Value value);


#endif
