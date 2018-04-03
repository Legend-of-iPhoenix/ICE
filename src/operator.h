#ifndef OPERATOR_H
#define OPERATOR_H

enum {
    REGISTER_HL,
    REGISTER_DE,
    REGISTER_BC,
    REGISTER_HL_DE,
    REGISTER_A,
    REGISTER_AUBC
};

extern const uint8_t operatorPrecedence[17];
extern const uint8_t operatorPrecedence2[17];
extern void (*operatorsPointers[272])(void);
extern void (*operatorsChainPushPointers[17])(void);
extern void (*operatorsFloatPointers[16])(void);

void MultWithNumber(uint24_t num, uint8_t *programPtr, uint8_t ChangeRegisters);
float execOp(uint8_t op, float operand1, float operand2);
uint8_t getIndexOfOperator(uint8_t op);
uint8_t compileOperator(uint24_t index);
void OperatorError(void);
void OperatorsSwap(void);
void AndInsert(void);
void XorInsert(void);
void OrInsert(void);
void EQInsert(void);
void OperatorFloatChainPushChainAns(void);

#endif
