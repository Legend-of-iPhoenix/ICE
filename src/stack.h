#ifndef STACK_H
#define STACK_H

void clearStacks(void);
void outputStackPush(element_t newEntry);
void setOutputElement(element_t newEntry, uint24_t index);
void removeOutputElement(uint24_t index);
void stackPush(element_t newEntry);
element_t outputStackPop(void);
element_t getOutputElement(uint24_t index);
element_t stackPop(void);

#endif
