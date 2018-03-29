#ifndef OPERATOR_H
#define OPERATOR_H

enum {
    REGISTER_HL,
    REGISTER_DE,
    REGISTER_BC,
    REGISTER_HL_DE,
    REGISTER_A
};

extern const uint8_t operatorPrecedence[17];
extern const uint8_t operatorPrecedence2[17];

void MultWithNumber(uint24_t num, uint8_t *programPtr, bool ChangeRegisters);
float execOp(uint8_t op, float operand1, float operand2);
uint8_t getIndexOfOperator(uint8_t op);

#endif
