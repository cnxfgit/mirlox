#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

//  字节操作码
typedef enum {
    OP_CONSTANT,        // 写入常量
    OP_NIL,             // 空指令 nil
    OP_TRUE,            // true指令
    OP_FALSE,           // false指令
    OP_POP,             // 弹出指令
    OP_GET_LOCAL,       // 获取局部变量
    OP_SET_LOCAL,       // 赋值局部变量
    OP_GET_GLOBAL,      // 获取全局变量
    OP_DEFINE_GLOBAL,   // 定义全局变量
    OP_SET_GLOBAL,      // 赋值全局变量
    OP_GET_UPVALUE,     // 获取升值指令
    OP_SET_UPVALUE,     // 赋值升值指令
    OP_GET_PROPERTY,    // 获取属性指令
    OP_SET_PROPERTY,    // 赋值属性指令
    OP_GET_SUPER,       // 获取父类指令
    OP_EQUAL,           // 赋值指令 =
    OP_GREATER,         // 大于指令 >
    OP_LESS,            // 小于指令 <
    OP_ADD,             // 加指令 +
    OP_SUBTRACT,        // 减指令 -
    OP_MULTIPLY,        // 乘指令 *
    OP_DIVIDE,          // 除指令 /
    OP_NOT,             // 非指令 !
    OP_NEGATE,          // 负指令 -
    OP_PRINT,           // 打印指令
    OP_JUMP,            // 分支跳转指令
    OP_JUMP_IF_FALSE,   // if false分支跳转指令
    OP_LOOP,            // 循环指令
    OP_CALL,            // 调用指令
    OP_INVOKE,          // 执行指令
    OP_SUPER_INVOKE,    // 父类执行指令
    OP_CLOSURE,         // 闭包指令
    OP_CLOSE_UPVALUE,   // 关闭提升值
    OP_RETURN,          // 返回指令
    OP_CLASS,           // 类指令
    OP_INHERIT,         // 继承指令
    OP_METHOD           // 方法指令
} OpCode;

// 字节码块
typedef struct {
    int count;              // 字节码数组当前长度
    int capacity;           // 字节码数组当前总容量
    uint8_t* code;          // 字节码数组
    int* lines;             // 源码行号
    ValueArray constants;   // 字节码块常量数组
} Chunk;

// 初始化字节码块
void initChunk(Chunk* chunk);

// 写入一个字节操作码到字节码块
void writeChunk(Chunk* chunk, uint8_t byte, int line);

// 往字节码块写入一个常量
int addConstant(Chunk* chunk, Value value);

// 释放字节码块
void freeChunk(Chunk* chunk);

#endif