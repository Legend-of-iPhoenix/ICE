#ifndef STACK_H
#define STACK_H

void clearStacks(void);
void outputStackPush(element_t newEntry);
void setOutputElement(element_t newEntry, int index);
void removeOutputElement(int index);
void stackPush(element_t newEntry);
element_t outputStackPop(void);
element_t getOutputElement(int index);
element_t stackPop(void);

#endif
