//
// Created by Administrator on 2022/7/21.
//

#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

// 获取对象类型
#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

// 是否是方法
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
// 是否为类
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
// 是否为闭包
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
// 是否为函数
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
// 是否为实例
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
// 是否为原生函数
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
// 是否为字符串对象
#define IS_STRING(value)       isObjType(value, OBJ_STRING)

// 转化为方法对象
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
// 转化为类对象
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
// 函数值转化为闭包对象
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
// 函数值转化为函数对象
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
// 转化为的实例对象
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
// 转化为原生函数对象
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)
// c字符创转化成对象字符串
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
// 对象字符创转化为c字符串
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

// 对象类型枚举
typedef enum {
    OBJ_BOUND_METHOD,   // 绑定方法对象
    OBJ_CLASS,          // 类对象
    OBJ_CLOSURE,        // 闭包对象
    OBJ_FUNCTION,       // 函数对象
    OBJ_INSTANCE,       // 实例对象
    OBJ_NATIVE,         // 原生函数对象
    OBJ_STRING,         // 字符串对象
    OBJ_UPVALUE,        // 闭包提升值对象
} ObjType;

// 对象结构体
struct Obj {
    ObjType type;       // 对象类型
    bool isMarked;      // 是否被标记
    struct Obj *next;   // 下一个对象
};

// 函数对象结构体
typedef struct {
    Obj obj;            // 公共对象头
    int arity;          // 参数数
    int upvalueCount;   // 提升值数
    Chunk chunk;        // 函数的字节码块
    ObjString *name;    // 函数名
} ObjFunction;

// 原生函数 函数指针
typedef Value (*NativeFn)(int argCount, Value *args);

// 原生函数对象
typedef struct {
    Obj obj;            // 公共对象头
    NativeFn function;  // 原生函数指针
} ObjNative;

// 字符串对象结构体
struct ObjString {
    Obj obj;        // 公共对象头
    int length;     // 字符串长度
    char *chars;    // 字符串指针
    uint32_t hash;  // 哈希值
};

// 提升值
typedef struct ObjUpvalue {
    Obj obj;                    // 公共对象头
    Value *location;            // 捕获的局部变量
    Value closed;               //
    struct ObjUpvalue *next;    // next指针
} ObjUpvalue;

// 闭包对象
typedef struct {
    Obj obj;                    // 公共对象头
    ObjFunction *function;      // 裸函数
    ObjUpvalue **upvalues;      // 提升值数组
    int upvalueCount;           // 提升值数量
} ObjClosure;

// 类对象
typedef struct {
    Obj obj;                // 公共对象头
    ObjString *name;        // 类名
    Table methods;          // 类方法
} ObjClass;

// 实例对象
typedef struct {
    Obj obj;
    ObjClass *klass;
    Table fields;
} ObjInstance;

// 绑定方法对象
typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure *method;
} ObjBoundMethod;

// 新建方法
ObjBoundMethod *newBoundMethod(Value receiver, ObjClosure *method);

// 新建类对象
ObjClass *newClass(ObjString *name);

// 新建一个闭包对象
ObjClosure *newClosure(ObjFunction *function);

// 新建一个函数对象
ObjFunction *newFunction();

// 新建一个实例对象
ObjInstance *newInstance(ObjClass *klass);

// 新建一个原生函数
ObjNative *newNative(NativeFn function);

// 取c字符串成字符串类型
ObjString *takeString(char *chars, int length);

// 在堆中复制字符创 并返回指针
ObjString *copyString(const char *chars, int length);

// 新建提升值
ObjUpvalue *newUpvalue(Value *slot);

// 打印对象
void printObject(Value value);

// 内联函数判断对象是否为指定类型
static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
