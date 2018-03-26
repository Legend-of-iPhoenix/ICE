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

static uint24_t *p1;
static uint24_t *p2;

uint24_t outputElements;
uint24_t stackElements;
element_t outputStack[500];
element_t stack[250];

void push(uint24_t i) {
    *p1++ = i;
}

uint24_t getNextIndex(void) {
    return *(p2++);
}

uint24_t getIndexOffset(uint24_t offset) {
    return *(p2 + offset);
}

void removeIndexFromStack(uint24_t index) {
    memcpy(ice.stackStart + index, ice.stackStart + index + 1, (STACK_SIZE - index) * 3);
    p2--;
}

uint24_t getCurrentIndex(void) {
    return p2 - ice.stackStart;
}

uint24_t *getStackVar(uint8_t which) {
    if (which) {
        return p2;
    }
    return p1;
}

void setStackValues(uint24_t* val1, uint24_t* val2) {
    p1 = val1;
    p2 = val2;
}

void clearStacks(void) {
    outputElements = stackElements = 0;
}

void outputStackPush(element_t newEntry) {
    outputStack[outputElements++] = newEntry;
}

element_t outputStackPop(void) {
    return outputStack[--outputElements];
}

void stackPush(element_t newEntry) {
    stack[stackElements++] = newEntry;
}

element_t stackPop(void) {
    return stac[--stackElements];
}