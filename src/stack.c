#include "defines.h"
#include "stack.h"

#include "parse.h"
#include "main.h"
#include "errors.h"
#include "output.h"
#include "operator.h"
#include "functions.h"
#include "routines.h"
#include "prescan.h"

uint24_t outputElements;
uint24_t stackElements;
static element_t outputStack[500];
static element_t stack[250];

void clearStacks(void) {
    outputElements = stackElements = 0;
}

void outputStackPush(element_t newEntry) {
    outputStack[outputElements++] = newEntry;
}

element_t outputStackPop(void) {
    return outputStack[--outputElements];
}

element_t getOutputElement(int index) {
    return outputStack[index];
}

void setOutputElement(element_t newEntry, int index) {
    outputStack[index] = newEntry;
}

void removeOutputElement(int index) {
    if (index != outputElements - 1) {
        memcpy(&outputStack[index], &outputStack[index + 1], sizeof(element_t) * (outputElements - index));
    }
    outputElements--;
}

void stackPush(element_t newEntry) {
    stack[stackElements++] = newEntry;
}

element_t stackPop(void) {
    return stack[--stackElements];
}