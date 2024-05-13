//
// Created by Administrator on 2022/7/18.
//

#ifndef clox_memory_h
#define clox_memory_h

#include "common.h"
#include "object.h"


// 初始分配内存
#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

// 动态数组扩容 小于8则初始化为8 否则则容量乘2
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

// 释放数组
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

// 释放对象
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

// 动态数组容量调整 类型 void指针 旧长度 新长度
#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

// 重新分配内存 扩容或者缩容 取决于新旧长度的大小
void *reallocate(void *pointer, size_t oldSize, size_t newSize);

// 标记对象
void markObject(Obj* object);

// 标记值
void markValue(Value value);

// 执行一次垃圾回收
void collectGarbage();

// 释放虚拟机根链的对象
void freeObjects();

#endif
