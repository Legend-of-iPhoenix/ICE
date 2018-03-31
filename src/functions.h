#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define RET_A         (1<<7)
#define UN            (1<<6)
#define RET_HLs       (1<<5)
#define RET_NONE      (0)
#define RET_HL        (0)
#define ARG_NORM      (0)
#define SMALL_1       (1<<7)
#define SMALL_2       (1<<6)
#define SMALL_3       (1<<5)
#define SMALL_4       (1<<4)
#define SMALL_5       (1<<3)
#define SMALL_12      (SMALL_1 | SMALL_2)
#define SMALL_123     (SMALL_1 | SMALL_2 | SMALL_3)
#define SMALL_13      (SMALL_1 | SMALL_3)
#define SMALL_23      (SMALL_2 | SMALL_3)
#define SMALL_14      (SMALL_1 | SMALL_4)
#define SMALL_24      (SMALL_2 | SMALL_4)
#define SMALL_45      (SMALL_4 | SMALL_5)
#define SMALL_34      (SMALL_3 | SMALL_4)
#define SMALL_345     (SMALL_3 | SMALL_45)

#define AMOUNT_OF_FUNCTIONS 33

typedef struct {
    uint8_t function;
    uint8_t function2;
    uint8_t amountOfArgs;
    uint8_t disallowNumArgs;
    uint8_t (*functionPtr)(uint8_t);
} function_t;

typedef struct {
    uint8_t returnRegister;
    uint8_t amountOfArgs;
    uint8_t smallArgs;
} C_function_t;

extern const function_t functions[AMOUNT_OF_FUNCTIONS];

float execFunc(uint8_t func, float operand1, float operand2);
uint8_t compileFunction(uint24_t index);

#endif
