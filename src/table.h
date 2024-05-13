//
// Created by Administrator on 2022/7/22.
// 开放寻址法 哈希算法为 hashString 逻辑删除
//

#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

// 哈希节点
typedef struct {
    ObjString *key; // 字符串对象做完key值
    Value value;    // 值对象可以是任意元素
} Entry;

// 哈希表
typedef struct {
    int count;      // 当前元素数
    int capacity;   // 最大元素数
    Entry *entries; // 哈希节点数组
} Table;

// 初始化表
void initTable(Table *table);

// 释放表
void freeTable(Table *table);

// 获取key对应值
bool tableGet(Table *table, ObjString *key, Value *value);

// 插入哈希表
bool tableSet(Table *table, ObjString *key, Value value);

// 移除键值对
bool tableDelete(Table *table, ObjString *key);

// 复制表
void tableAddAll(Table *from, Table *to);

// 在表中寻找字符串节点
ObjString *tableFindString(Table *table, const char *chars, int length, uint32_t hash);

// 及时移除不能访问的字符串
void tableRemoveWhite(Table* table);

// 标记表
void markTable(Table* table);

#endif
