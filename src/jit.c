#include "jit.h"
#include "common.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

typedef struct LoxFunction {
    const char *name;
    void *func;
} LoxFunction;

static LoxFunction LoxFunctions[] = {
    {"runtimeError", runtimeError},
    {"push", push},
    {"pop", pop},
    {"peek", peek},
    {"callValue", callValue},
    {"invokeFromClass", invokeFromClass},
    {"invoke", invoke},
    {NULL, NULL},
};

static void *import_resolver(const char *name) {
    for (int i = 0; LoxFunctions[i].name; i++) {
        if (!strcmp(name, LoxFunctions[i].name)) {
            return LoxFunctions[i].func;
        }
    }
    return NULL;
}

typedef struct JitBuffer {
    char *buffer;
    size_t p;
    size_t size;
    size_t capacity;
} JitBuffer;

static int jit_getc(void *data) {
    JitBuffer *buff = data;
    if (buff->p >= buff->size)
        return EOF;
    int c = buff->buffer[buff->p];
    if (c == 0) {
        c = EOF;
    } else {
        buff->p++;
    }
    return c;
}

static void initBuffer(VM *vm, JitBuffer *buff, size_t capacity) {
    buff->size = buff->p = 0;
    if (capacity <= 0) {
        buff->buffer = NULL;
        buff->capacity = 0;
        return;
    }
    buff->buffer = reallocate(NULL, 0, capacity * sizeof(char));
    memset(buff->buffer, 0, capacity * sizeof(char));
    buff->capacity = capacity;
}

static void resizeBuffer(JitBuffer *buff, size_t buffSize) {
    if (buff->capacity >= buffSize)
        return;
    size_t newsize = buffSize * 2;
    buff->buffer = reallocate(buff->buffer, buff->capacity * sizeof(char),
                              newsize * sizeof(char));
    buff->capacity = newsize;
}

#define FREE_BUFFER(buff) FREE_ARRAY(char, buff->buffer)

static void strTobuffer(JitBuffer *buff, const char *str) {
    size_t len = strlen(str);
    size_t newsize = buff->size + len + 2;
    resizeBuffer(buff, newsize);
    strcpy(&buff->buffer[buff->size], str);
    buff->size += len;
    buff->buffer[buff->size++] = '\n';
    buff->buffer[buff->size] = 0;
}

static void fmtStrTobuffer(JitBuffer *buff, const char *fmt, ...) {
    va_list args;
    char localBuffer[1024];
    va_start(args, fmt);
    int n = vsnprintf(localBuffer, sizeof(localBuffer), fmt, args);
    if (n < 0)
        abort();
    va_end(args);
    strTobuffer(buff, localBuffer);
}

#define CODE(fmt, ...) fmtStrToBuff(buff, fmt, ##__VA_ARGS__)

#define OPEN_FUNC(name)                                                        \
    do {                                                                       \
        CODE("int %s (VM *vm) {", name);                                       \
    } while (0)

#define CLOSE_FUNC                                                             \
    do {                                                                       \
        CODE("return 0;");                                                     \
        CODE("}");                                                             \
    } while (0)

static void setJmps(ObjClosure *closure, uint8_t *isJmps) {
    uint8_t instruction;
    uint8_t *code = closure->function->chunk.code;
    for (int pc = 0; pc < closure->function->chunk.count; pc++) {
        instruction = code[pc];
        switch (instruction) {
        case OP_JUMP | OP_JUMP_IF_FALSE: {
            pc += 2;
            uint16_t offset = (uint16_t)((code[pc - 2] << 8) | code[pc - 1]);
            isJmps[pc + offset] = 1;
            break;
        }
        case OP_LOOP: {
            pc += 2;
            uint16_t offset = (uint16_t)((code[pc - 2] << 8) | code[pc - 1]);
            isJmps[pc - offset] = 1;
            break;
        }
        default:
            break;
        }
    }
}

static void codeGenerate(VM *vm, JitBuffer *buff, ObjClosure *closure,
                         char *name, int argCount) {
    strTobuffer(buff, LOX_HEADER);
    uint8_t *isJmps = reallocate(NULL, 0, closure->function->chunk.count);
    memset(isJmps, 0, closure->function->chunk.count * sizeof(isJmps[0]));
    OPEN_FUNC(name);

    setJmps(closure, isJmps);

    CallFrame *frame = &vm->frames[vm->frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm->stackTop - argCount - 1;

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT()                                                           \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT()                                                        \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                                               \
    do {                                                                       \
        CODE("if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {");             \
        CODE("  runtimeError(\"Operands must be numbers.\");");                \
        CODE("  return INTERPRET_RUNTIME_ERROR; ");                            \
        CODE("}");                                                             \
        CODE("double b = AS_NUMBER(pop());");                                  \
        CODE("double a = AS_NUMBER(pop());");                                  \
        CODE("push(%s(a %s b)); ", valueType, op);                             \
    } while (false)

    for (;;) {
        uint8_t instruction = READ_BYTE();
        int pc = frame->ip - closure->function->chunk.code;
        if (isJmps[pc]) {
            CODE("Label_%d:", pc);
        }

        switch (instruction) {
        case OP_CONSTANT: {
            Value value = READ_CONSTANT();
            CODE("Value constant = %llu;", value);
            CODE("push(constant);");
            break;
        }
        case OP_NIL:
            CODE("push(NIL_VAL);");
            break;
        case OP_TRUE:
            CODE("push(BOOL_VAL(true));");
            break;
        case OP_FALSE:
            CODE("push(BOOL_VAL(false));");
            break;
        case OP_POP:
            CODE("pop();");
            break;
        case OP_GET_LOCAL: {
            Value value = READ_BYTE();
            CODE("uint8_t slot = %u;", (uint8_t)value);
            CODE("push(frame->slots[slot]);");
            break;
        }
        case OP_SET_LOCAL: {
            Value value = READ_BYTE();
            CODE("uint8_t slot = %u;", (uint8_t)value);
            CODE("frame->slots[slot] = peek(0);");
            break;
        }
        case OP_GET_GLOBAL: {
            ObjString *name = READ_STRING();
            CODE("Value value;");
            CODE("if (!tableGet(&vm->globals, name, &value)) {");
            CODE("  ObjString *name = (ObjString *)%p", name);
            CODE("  runtimeError(\"Undefined variable '%%s'.\", name->chars);");
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");
            CODE("push(value);");
            break;
        }
        case OP_DEFINE_GLOBAL: {
            ObjString *name = READ_STRING();
            CODE("ObjString *name = (ObjString *)%p", name);
            CODE("tableSet(&vm->globals, name, peek(0));");
            CODE("pop();");
            break;
        }
        case OP_SET_GLOBAL: {
            ObjString *name = READ_STRING();
            CODE("if (tableSet(&vm->globals, name, peek(0))) {");
            CODE("  ObjString *name = (ObjString *)%p", name);
            CODE("  tableDelete(&vm->globals, name);");
            CODE("  runtimeError(\"Undefined variable '%%s'.\", name->chars);");
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");
            break;
        }
        case OP_GET_UPVALUE: {
            Value value = READ_BYTE();
            CODE("uint8_t slot = %u;", (uint8_t)value);
            CODE("push(*frame->closure->upvalues[slot]->location);");
            break;
        }
        case OP_SET_UPVALUE: {
            Value value = READ_BYTE();
            CODE("uint8_t slot = %u;", (uint8_t)value);
            CODE("*frame->closure->upvalues[slot]->location = peek(0);");
            break;
        }
        case OP_GET_PROPERTY: {
            CODE("if (!IS_INSTANCE(peek(0))) {");
            CODE("  runtimeError(\"Only instances have properties.\");");
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");

            CODE("ObjInstance *instance = AS_INSTANCE(peek(0));");
            ObjString *name = READ_STRING();
            CODE("ObjString *name = (ObjString *)%p;", name);

            CODE("Value value;");
            CODE("if (tableGet(&instance->fields, name, &value)) {");
            CODE("  pop();");
            CODE("  push(value);");
            CODE("} else {");
            CODE("  if (!bindMethod(instance->klass, name)) {");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
            CODE("}");
            break;
        }
        case OP_SET_PROPERTY: {
            CODE("if (!IS_INSTANCE(peek(1))) {");
            CODE("  runtimeError(\"Only instances have fields.\");");
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");

            CODE("ObjInstance *instance = AS_INSTANCE(peek(1));");
            ObjString *name = READ_STRING();
            CODE("tableSet(&instance->fields, (ObjString *)%p, peek(0));",
                 name);
            CODE("Value value = pop();");
            CODE("pop();");
            CODE("push(value);");
            break;
        }
        case OP_GET_SUPER: {
            ObjString *name = READ_STRING();
            CODE("ObjString *name = (ObjString *)%p;", name);
            CODE("ObjClass *superclass = AS_CLASS(pop());");

            CODE("if (!bindMethod(superclass, name)) {");
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");
            break;
        }
        case OP_EQUAL: {
            CODE("Value b = pop();");
            CODE("Value a = pop();");
            CODE("push(BOOL_VAL(valuesEqual(a, b)));");
            break;
        }
        case OP_GREATER:
            BINARY_OP("BOOL_VAL", ">");
            break;
        case OP_LESS:
            BINARY_OP("BOOL_VAL", "<");
            break;
        case OP_ADD: {
            CODE("if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {");
            CODE("  concatenate();");
            CODE("} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {");
            CODE("  double b = AS_NUMBER(pop());");
            CODE("  double a = AS_NUMBER(pop());");
            CODE("  push(NUMBER_VAL(a + b));");
            CODE("} else {");
            CODE("  runtimeError(\"Operands must be two numbers or two "
                 "strings.\");");
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");
            break;
        }
        case OP_SUBTRACT:
            BINARY_OP("NUMBER_VAL", "-");
            break;
        case OP_MULTIPLY:
            BINARY_OP("NUMBER_VAL", "*");
            break;
        case OP_DIVIDE:
            BINARY_OP("NUMBER_VAL", "/");
            break;
        case OP_NOT:
            CODE("push(BOOL_VAL(isFalsey(pop())));");
            break;
        case OP_NEGATE:
            CODE("if (!IS_NUMBER(peek(0))) {");
            CODE("  runtimeError(\"Operand must be a number.\");");
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");
            CODE("push(NUMBER_VAL(-AS_NUMBER(pop())));");
            break;
        case OP_PRINT: {
            CODE("printValue(pop());");
            CODE("printf(\"\\n\");");
            break;
        }
        case OP_JUMP: {
            uint16_t offset = READ_SHORT();
            CODE("goto Label_%d;", pc + offset);
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            CODE("if (isFalsey(peek(0)))");
            CODE("  goto Label_%d;", pc + offset);
            break;
        }
        case OP_LOOP: {
            uint16_t offset = READ_SHORT();
            CODE("goto Label_%d;", pc - offset);
            break;
        }
        case OP_CALL: {
            int argCount = READ_BYTE();
            CODE("if (!callValue(peek(%d), %d)) {", argCount, argCount);
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");
            CODE("frame = &vm->frames[vm->frameCount - 1];");
            break;
        }
        case OP_INVOKE: {
            ObjString *method = READ_STRING();
            int argCount = READ_BYTE();
            CODE("if (!invoke(%p, %d)) {", method, argCount);
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");
            CODE("frame = &vm->frames[vm->frameCount - 1];");
            break;
        }
        case OP_SUPER_INVOKE: {
            ObjString *method = READ_STRING();
            int argCount = READ_BYTE();
            CODE("ObjClass *superclass = AS_CLASS(pop());");
            CODE("if (!invokeFromClass(superclass, %p, %d)) {", method,
                 argCount);
            CODE("  return INTERPRET_RUNTIME_ERROR;");
            CODE("}");
            CODE("frame = &vm->frames[vm->frameCount - 1];");
            break;
        }
        case OP_CLOSURE: {
            ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
            CODE("ObjClosure *closure = newClosure((ObjFunction *) %p));",
                 function);
            CODE("push(OBJ_VAL(closure));");

            for (size_t i = 0; i < function->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal) {
                    CODE("closure->upvalues[%d] = captureUpvalue(frame->slots "
                         "+ %u);",
                         i, index);
                } else {
                    CODE(
                        "closure->upvalues[%d] = frame->closure->upvalues[%u];",
                        i, index);
                }
            }
            break;
        }
        case OP_CLOSE_UPVALUE:
            CODE("closeUpvalues(vm->stackTop - 1);");
            CODE("pop();");
            break;
        case OP_RETURN: {
            CODE("Value result = pop();");
            CODE("closeUpvalues(frame->slots);");
            CODE("vm->frameCount--;");
            CODE("if (vm->frameCount == 0) {");
            CODE("  pop();");
            CODE("  return INTERPRET_OK;");
            CODE("}");
                
            CODE("vm->stackTop = frame->slots;"); 
            CODE("push(result);");
            CODE("frame = &vm->frames[vm->frameCount - 1];");
            break;
        }
        case OP_CLASS:
            push(OBJ_VAL(newClass(READ_STRING())));
            break;
        case OP_INHERIT: {
            Value superclass = peek(1);
            if (!IS_CLASS(superclass)) {
                runtimeError("Superclass must be a class.");
                return INTERPRET_RUNTIME_ERROR;
            }

            ObjClass *subclass = AS_CLASS(peek(0));
            tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
            pop(); // Subclass.
            break;
        }
        case OP_METHOD:
            ObjString *name = READ_STRING();
            CODE("defineMethod((OBjString *) %p);", name);
            break;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP

    CLOSE_FUNC;
}

static const char LOX_HEADER[] = {""};