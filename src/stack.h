#ifndef STACK_H
#define STACK_H

void push(uint24_t);
uint24_t pop(void);
uint24_t getStackSize(void);
uint24_t getNextIndex(void);
uint24_t getCurrentIndex(void);
uint24_t getIndexOffset(uint24_t);
void removeIndexFromStack(uint24_t);
void getNextFreeStack(void);
void removeStack(void);
uint24_t *getStackVar(uint8_t);
void setStackValues(uint24_t*, uint24_t*);

#endif
