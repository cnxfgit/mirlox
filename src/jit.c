#include <stdarg.h>

#include "jit.h"
#include "memory.h"
#include "mir-gen.h"

typedef struct LoxFunction {
    const char *name;
    void *func;
} LoxFunction;

static const char LOX_HEADER[];

static LoxFunction LoxFunctions[] = {
    {"runtimeError", runtimeError},
    {"push", push},
    {"pop", pop},
    {"peek", peek},
    {"callValue", callValue},
    {"invokeFromClass", invokeFromClass},
    {"invoke", invoke},
    {"tableGet", tableGet},
    {"tableSet", tableSet},
    {"newClosure", newClosure},
    {"closeUpvalues", closeUpvalues},
    {"printf", printf},
    {"printValue", printValue},
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

static void initBuffer(JitBuffer *buff, size_t capacity) {
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

#define FREE_BUFFER(buff) FREE_ARRAY(char, buff.buffer, buff.capacity)

static void strTobuffer(JitBuffer *buff, const char *str) {
    size_t len = strlen(str);
    size_t newsize = buff->size + len + 2;
    resizeBuffer(buff, newsize);
    strcpy(&buff->buffer[buff->size], str);
    buff->size += len;
    buff->buffer[buff->size++] = '\n';
    buff->buffer[buff->size] = 0;
}

static void fmtStrToBuffer(JitBuffer *buff, const char *fmt, ...) {
    va_list args;
    char localBuffer[1024];
    va_start(args, fmt);
    int n = vsnprintf(localBuffer, sizeof(localBuffer), fmt, args);
    if (n < 0)
        abort();
    va_end(args);
    strTobuffer(buff, localBuffer);
}

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

#define CODE(fmt, ...) fmtStrToBuffer(buff, fmt, ##__VA_ARGS__)

#define OPEN_FUNC(name)                                                        \
    do {                                                                       \
        CODE("int %s (VM *vm, ObjClosure* _closure) {", name);                 \
        CODE("  CallFrame *frame = &vm->frames[vm->frameCount++];");           \
        CODE("  frame->closure = _closure;");                                  \
        CODE("  frame->ip = _closure->function->chunk.code;");                 \
        CODE("  frame->slots = vm->stackTop - %d - 1;", argCount);             \
                                                                               \
        CODE("  ObjString *name;");                                            \
    } while (0)

#define CLOSE_FUNC                                                             \
    do {                                                                       \
        CODE("  return INTERPRET_OK;");                                        \
        CODE("}");                                                             \
    } while (0)

static void setJmps(ObjClosure *closure, uint8_t *isJmps) {
    uint8_t instruction;
    uint8_t *code = closure->function->chunk.code;
    for (int pc = 0; pc < closure->function->chunk.count; pc++) {
        instruction = code[pc];
        switch (instruction) {
        case OP_JUMP:
        case OP_JUMP_IF_FALSE: {
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
    int codeCount = closure->function->chunk.count;
    uint8_t *isJmps = malloc(codeCount * sizeof(uint8_t));
    memset(isJmps, 0, codeCount * sizeof(isJmps[0]));
    OPEN_FUNC(name);

    setJmps(closure, isJmps);

    CallFrame callFrame;
    CallFrame *frame = &callFrame;
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
        CODE("  if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {");           \
        CODE("      runtimeError(\"Operands must be numbers.\");");            \
        CODE("      return INTERPRET_RUNTIME_ERROR; ");                        \
        CODE("  }");                                                           \
        CODE("  double b = AS_NUMBER(pop());");                                \
        CODE("  double a = AS_NUMBER(pop());");                                \
        CODE("  push(%s(a %s b)); ", valueType, op);                           \
    } while (false)

    int pc = frame->ip - closure->function->chunk.code;
    while (pc < codeCount) {
        uint8_t instruction = READ_BYTE();
        pc = frame->ip - closure->function->chunk.code;
        if (pc < codeCount && isJmps[pc]) {
            CODE("Label_%d:", pc);
        }

        switch (instruction) {
        case OP_CONSTANT: {
            Value value = READ_CONSTANT();
            CODE("  Value constant = %luUL;", value);
            CODE("  push(constant);");
            break;
        }
        case OP_NIL:
            CODE("  push(NIL_VAL);");
            break;
        case OP_TRUE:
            CODE("  push(BOOL_VAL(true));");
            break;
        case OP_FALSE:
            CODE("  push(BOOL_VAL(false));");
            break;
        case OP_POP:
            CODE("  pop();");
            break;
        case OP_GET_LOCAL: {
            Value value = READ_BYTE();
            CODE("  uint8_t slot = %u;", (uint8_t)value);
            CODE("  push(frame->slots[slot]);");
            break;
        }
        case OP_SET_LOCAL: {
            Value value = READ_BYTE();
            CODE("  uint8_t slot = %u;", (uint8_t)value);
            CODE("  frame->slots[slot] = peek(0);");
            break;
        }
        case OP_GET_GLOBAL: {
            ObjString *name = READ_STRING();
            CODE("  Value value;");
            CODE("  name = (ObjString *)%p;", name);
            CODE("  if (!tableGet(&vm->globals, name, &value)) {");
            CODE("      runtimeError(\"Undefined variable '%%s'.\", "
                 "name->chars);");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
            CODE("  push(value);");
            break;
        }
        case OP_DEFINE_GLOBAL: {
            ObjString *name = READ_STRING();
            CODE("  name = (ObjString *)%p;", name);
            CODE("  tableSet(&vm->globals, name, peek(0));");
            CODE("  pop();");
            break;
        }
        case OP_SET_GLOBAL: {
            ObjString *name = READ_STRING();
            CODE("  if (tableSet(&vm->globals, name, peek(0))) {");
            CODE("      name = (ObjString *)%p;", name);
            CODE("      tableDelete(&vm->globals, name);");
            CODE("      runtimeError(\"Undefined variable '%%s'.\", "
                 "name->chars);");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
            break;
        }
        case OP_GET_UPVALUE: {
            Value value = READ_BYTE();
            CODE("  uint8_t slot = %u;", (uint8_t)value);
            CODE("  push(*frame->closure->upvalues[slot]->location);");
            break;
        }
        case OP_SET_UPVALUE: {
            Value value = READ_BYTE();
            CODE("  uint8_t slot = %u;", (uint8_t)value);
            CODE("  *frame->closure->upvalues[slot]->location = peek(0);");
            break;
        }
        case OP_GET_PROPERTY: {
            CODE("  if (!IS_INSTANCE(peek(0))) {");
            CODE("      runtimeError(\"Only instances have properties.\");");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");

            CODE("  ObjInstance *instance = AS_INSTANCE(peek(0));");
            ObjString *name = READ_STRING();
            CODE("  name = (ObjString *)%p;", name);

            CODE("  Value value;");
            CODE("  if (tableGet(&instance->fields, name, &value)) {");
            CODE("      pop();");
            CODE("      push(value);");
            CODE("  } else {");
            CODE("      if (!bindMethod(instance->klass, name)) {");
            CODE("          return INTERPRET_RUNTIME_ERROR;");
            CODE("      }");
            CODE("  }");
            break;
        }
        case OP_SET_PROPERTY: {
            CODE("  if (!IS_INSTANCE(peek(1))) {");
            CODE("      runtimeError(\"Only instances have fields.\");");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");

            CODE("  ObjInstance *instance = AS_INSTANCE(peek(1));");
            ObjString *name = READ_STRING();
            CODE("  tableSet(&instance->fields, (ObjString *)%p, peek(0));",
                 name);
            CODE("  Value value = pop();");
            CODE("  pop();");
            CODE("  push(value);");
            break;
        }
        case OP_GET_SUPER: {
            ObjString *name = READ_STRING();
            CODE("  name = (ObjString *)%p;", name);
            CODE("  ObjClass *superclass = AS_CLASS(pop());");

            CODE("  if (!bindMethod(superclass, name)) {");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
            break;
        }
        case OP_EQUAL: {
            CODE("  Value b = pop();");
            CODE("  Value a = pop();");
            CODE("  push(BOOL_VAL(valuesEqual(a, b)));");
            break;
        }
        case OP_GREATER:
            BINARY_OP("BOOL_VAL", ">");
            break;
        case OP_LESS:
            BINARY_OP("BOOL_VAL", "<");
            break;
        case OP_ADD: {
            CODE("  if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {");
            CODE("      concatenate();");
            CODE("  } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {");
            CODE("      double b = AS_NUMBER(pop());");
            CODE("      double a = AS_NUMBER(pop());");
            CODE("      push(NUMBER_VAL(a + b));");
            CODE("  } else {");
            CODE("      runtimeError(\"Operands must be two numbers or two "
                 "strings.\");");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
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
            CODE("  push(BOOL_VAL(isFalsey(pop())));");
            break;
        case OP_NEGATE:
            CODE("  if (!IS_NUMBER(peek(0))) {");
            CODE("      runtimeError(\"Operand must be a number.\");");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
            CODE("  push(NUMBER_VAL(-AS_NUMBER(pop())));");
            break;
        case OP_PRINT: {
            CODE("  printValue(pop());");
            CODE("  printf(\"\\n\");");
            break;
        }
        case OP_JUMP: {
            uint16_t offset = READ_SHORT();
            CODE("  goto Label_%d;", pc + offset);
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            CODE("  if (isFalsey(peek(0)))");
            CODE("      goto Label_%d;", pc + offset);
            break;
        }
        case OP_LOOP: {
            uint16_t offset = READ_SHORT();
            CODE("  goto Label_%d;", pc - offset);
            break;
        }
        case OP_CALL: {
            int argCount = READ_BYTE();
            CODE("  if (!callValue(peek(%d), %d)) {", argCount, argCount);
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
            CODE("  frame = &vm->frames[vm->frameCount - 1];");
            break;
        }
        case OP_INVOKE: {
            ObjString *method = READ_STRING();
            int argCount = READ_BYTE();
            CODE("  if (!invoke(%p, %d)) {", method, argCount);
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
            CODE("  frame = &vm->frames[vm->frameCount - 1];");
            break;
        }
        case OP_SUPER_INVOKE: {
            ObjString *method = READ_STRING();
            int argCount = READ_BYTE();
            CODE("  ObjClass *superclass = AS_CLASS(pop());");
            CODE("  if (!invokeFromClass(superclass, %p, %d)) {", method,
                 argCount);
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");
            CODE("  frame = &vm->frames[vm->frameCount - 1];");
            break;
        }
        case OP_CLOSURE: {
            ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
            CODE("  ObjClosure *closure = newClosure((ObjFunction *) %p);",
                 function);
            CODE("  push(OBJ_VAL(closure));");

            for (size_t i = 0; i < function->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal) {
                    CODE(
                        "  closure->upvalues[%d] = captureUpvalue(frame->slots "
                        "+ %u);",
                        i, index);
                } else {
                    CODE("   closure->upvalues[%d] = "
                         "frame->closure->upvalues[%u];",
                         i, index);
                }
            }
            break;
        }
        case OP_CLOSE_UPVALUE:
            CODE("  closeUpvalues(vm->stackTop - 1);");
            CODE("  pop();");
            break;
        case OP_RETURN: {
            CODE("  Value result = pop();");
            CODE("  closeUpvalues(frame->slots);");
            CODE("  vm->frameCount--;");
            CODE("  if (vm->frameCount == 0) {");
            CODE("      pop();");
            CODE("      return INTERPRET_OK;");
            CODE("  }");

            CODE("  vm->stackTop = frame->slots;");
            CODE("  push(result);");
            CODE("  frame = &vm->frames[vm->frameCount - 1];");
            break;
        }
        case OP_CLASS:
            push(OBJ_VAL(newClass(READ_STRING())));
            break;
        case OP_INHERIT: {
            CODE("  Value superclass = peek(1);");
            CODE("  if (!IS_CLASS(superclass)) {");
            CODE("      runtimeError(\"Superclass must be a class.\");");
            CODE("      return INTERPRET_RUNTIME_ERROR;");
            CODE("  }");

            CODE("  ObjClass *subclass = AS_CLASS(peek(0));");
            CODE("  tableAddAll(&AS_CLASS(superclass)->methods, "
                 "&subclass->methods);");
            CODE("  pop();");
            break;
        }
        case OP_METHOD: {
            ObjString *name = READ_STRING();
            CODE("  defineMethod((OBjString *) %p);", name);
            break;
        }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP

    CLOSE_FUNC;

    free(isJmps);
#ifdef DEBUG_PRINT_CODE
    FILE *f = fopen(name, "w+");
    fprintf(f, "%s\n", buff->buffer);
    fclose(f);
#endif
}

void jitCompile(VM *vm, ObjClosure *closure, int argCount) {
    MIR_context_t ctx = vm->mirContext;
    c2mir_init(ctx);
    MIR_gen_init(ctx, 2);
    JitBuffer buff;
    initBuffer(&buff, strlen(LOX_HEADER) + 4096);

    char name[32];

    snprintf(name, sizeof(name), "jit_func_%ld", ++vm->mirOptions.module_num);
    codeGenerate(vm, &buff, closure, name, argCount);

    if (!c2mir_compile(ctx, &vm->mirOptions, jit_getc, &buff, name, NULL)) {
        runtimeError("jit compiler error!");
        goto CLEANUP;
    }
    /* c2mir_compile will clear the name */
    snprintf(name, sizeof(name), "jit_func_%ld", vm->mirOptions.module_num);

    MIR_module_t module = DLIST_TAIL(MIR_module_t, *MIR_get_module_list(ctx));
    MIR_load_module(ctx, module);
    MIR_link(ctx, MIR_set_gen_interface, import_resolver);
    MIR_item_t func = DLIST_HEAD(MIR_item_t, module->items);
    while (func) {
        if (func->item_type == MIR_func_item &&
            !strcmp(name, func->u.func->name)) {
            break;
        }
        func = DLIST_NEXT(MIR_item_t, func);
    }
    if (func == NULL) {
        runtimeError("jit compiler error!");
        goto CLEANUP;
    }
    int (*fp)(void *, ObjClosure *) = MIR_gen(ctx, 0, func);
    if (fp) {
        closure->jitFunction = fp;
    } else {
        closure->jitFunction = NULL;
        runtimeError("jit gen error!");
        goto CLEANUP;
    }
CLEANUP:
    MIR_gen_finish(ctx);
    c2mir_finish(ctx);
    FREE_BUFFER(buff);
}

static const char LOX_HEADER[] = {
    "#ifdef __MIRC__\n"
    "#endif\n"
    "#define FRAMES_MAX 64\n"
    "#define STACK_MAX (FRAMES_MAX * 256)\n"
    "#define TAG_NIL 1\n"
    "#define TAG_FALSE 2\n"
    "#define TAG_TRUE  3\n"
    "typedef char bool;\n"
    "typedef unsigned long int uint64_t;\n"
    "typedef unsigned long int uintptr_t;\n"
    "typedef uint64_t Value;\n"
    "typedef unsigned char uint8_t;\n"
    "typedef unsigned int uint32_t;\n"
    "typedef unsigned long long size_t;\n"
    "#define SIGN_BIT ((uint64_t)0x8000000000000000)\n"
    "#define QNAN     ((uint64_t)0x7ffc000000000000)\n"
    "#define OBJ_VAL(obj)    (Value)(SIGN_BIT | QNAN | "
    "(uint64_t)(uintptr_t)(obj))\n"
    "#define NIL_VAL         ((Value)(uint64_t)(QNAN | TAG_NIL))\n"
    "#define true 1\n"
    "#define false 0\n"
    "\n"

    "typedef enum {\n"
    "   OBJ_BOUND_METHOD,\n"
    "   OBJ_CLASS,\n"
    "   OBJ_CLOSURE,\n"
    "   OBJ_FUNCTION,\n"
    "   OBJ_INSTANCE,\n"
    "   OBJ_NATIVE,\n"
    "   OBJ_STRING,\n"
    "   OBJ_UPVALUE,\n"
    "} ObjType;\n"
    "\n"
    "typedef enum {\n"
    "   INTERPRET_OK,\n"
    "   INTERPRET_COMPILE_ERROR,\n"
    "   INTERPRET_RUNTIME_ERROR\n"
    "} InterpretResult;\n"
    "\n"
    "typedef struct Obj {\n"
    "   ObjType type;\n"
    "   bool isMarked;\n"
    "   struct Obj *next;\n"
    "} Obj;\n"
    "\n"
    "typedef struct {\n"
    "    int capacity;\n"
    "    int count;\n"
    "    Value* values;\n"
    "} ValueArray;\n"
    "\n"
    "typedef struct {\n"
    "   int count;\n"
    "   int capacity;\n"
    "   uint8_t* code;\n"
    "   int* lines;\n"
    "   ValueArray constants;\n"
    "} Chunk;\n"
    "\n"
    "typedef struct {\n"
    "   Obj obj;\n"
    "   int length;\n"
    "   char *chars;\n"
    "   uint32_t hash;\n"
    "} ObjString;\n"
    "\n"
    "typedef struct {\n"
    "   Obj obj;\n"
    "   int arity;\n"
    "   int upvalueCount;\n"
    "   Chunk chunk;\n"
    "   ObjString *name;\n"
    "} ObjFunction;\n"
    "\n"
    "typedef struct ObjUpvalue {\n"
    "   Obj obj;\n"
    "   Value *location;\n"
    "   Value closed;\n"
    "   struct ObjUpvalue *next;\n"
    "} ObjUpvalue;\n"
    "\n"
    "typedef struct {\n"
    "   Obj obj;\n"
    "   ObjFunction *function;\n"
    "   ObjUpvalue **upvalues;\n"
    "   int upvalueCount;\n"
    "   int(*jitFunction)(void*);\n"
    "   int execCount;\n"
    "} ObjClosure;\n"
    "\n"
    "typedef struct {\n"
    "   ObjClosure* closure;\n"
    "   uint8_t* ip;\n"
    "   Value* slots;\n"
    "} CallFrame;\n"
    "\n"
    "typedef struct {\n"
    "   ObjString *key;\n"
    "   Value value;\n"
    "} Entry;\n"
    "\n"
    "typedef struct {\n"
    "   int count;\n"
    "   int capacity;\n"
    "   Entry *entries;\n"
    "} Table;\n"
    "\n"
    "typedef struct {\n"
    "   CallFrame frames[FRAMES_MAX];\n"
    "   int frameCount;\n"
    "   Value stack[STACK_MAX];\n"
    "   Value* stackTop;\n"
    "   Table globals;\n"
    "   Table strings;\n"
    "   ObjString* initString;\n"
    "   ObjUpvalue* openUpvalues;\n"
    "   size_t bytesAllocated;\n"
    "   size_t nextGC;\n"
    "   Obj* objects;\n"
    "   int grayCount;\n"
    "   int grayCapacity;\n"
    "   Obj** grayStack;\n"
    "} VM;\n"
    "\n"
    "Value pop();\n"
    "void push(Value);\n"
    "Value peek(int distance);\n"
    "void runtimeError(const char *, ...);\n"
    "bool tableGet(Table *, ObjString *, Value *);\n"
    "bool tableSet(Table *, ObjString *, Value);\n"
    "void closeUpvalues(Value *);\n"
    "ObjClosure *newClosure(ObjFunction *);\n"
    "bool callValue(Value, int);\n"
    "\n"};