//
// Created by Administrator on 2022/7/20.
//

#ifndef clox_scanner_h
#define clox_scanner_h

// 令牌类型枚举
typedef enum {
    // 单字符标记
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    // 一个或者两个的字符标记
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    // 字面量
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // 关键字
    TOKEN_AND, TOKEN_CLASS, TOKEN_ELSE, TOKEN_FALSE,
    TOKEN_FOR, TOKEN_FUN, TOKEN_IF, TOKEN_NIL, TOKEN_OR,
    TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER, TOKEN_THIS,
    TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,
    // 错误令牌或者结束符
    TOKEN_ERROR, TOKEN_EOF
} TokenType;


// 词法令牌
typedef struct {
    TokenType type;     // 令牌类型
    const char* start;  // 起点指针
    int length;         // 长度
    int line;           // 行号
} Token;

// 初始化扫描仪
void initScanner(const char* source);

// 扫描令牌
Token scanToken();

#endif
