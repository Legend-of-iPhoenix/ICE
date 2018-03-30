#include "defines.h"
#include "parse.h"
#include "operator.h"

#include "main.h"
#include "functions.h"
#include "errors.h"
#include "stack.h"
#include "output.h"
#include "routines.h"
#include "prescan.h"

static uint8_t op;
const char operators[18]              = {tStore, tDotIcon, tCrossIcon, tBoxIcon, tAnd, tXor, tOr, tEQ, tLT, tGT, tLE, tGE, tNE, tMul, tDiv, tAdd, tSub, 0};
const uint8_t operatorPrecedence[17]  = {0, 6, 8, 8, 2, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 4, 4};
const uint8_t operatorPrecedence2[17] = {9, 6, 8, 8, 2, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 4, 4};
const uint8_t operatorCanSwap[17]     = {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0}; // Used for operators which can swap the operands, i.e. A*B = B*A
static element_t outputCurr;
static element_t outputPrev;
static element_t outputPrevPrev;

#ifndef CALCULATOR
static uint8_t clz(uint24_t x) {
    uint8_t n = 0;
    if (!x) {
        return 24;
    }
    while (!(x & (1 << 23))) {
        n++;
        x <<= 1;
    }
    return n;
}

void MultWithNumber(uint24_t num, uint8_t *programPtr, bool ChangeRegisters) {
    (void)programPtr;
    uint24_t bit;
    uint8_t po2 = !(num & (num - 1));

    if (24 - clz(num) + __builtin_popcount(num) - 2 * po2 < 10) {
        if(!po2) {
            if (!ChangeRegisters) {
                PUSH_HL();
                POP_DE();
                reg.DEIsNumber = reg.HLIsNumber;
                reg.DEIsVariable = reg.HLIsVariable;
                reg.DEValue = reg.HLValue;
                reg.DEVariable = reg.HLVariable;
            } else {
                PUSH_DE();
                POP_HL();
                reg.HLIsNumber = reg.DEIsNumber;
                reg.HLIsVariable = reg.DEIsVariable;
                reg.HLValue = reg.DEValue;
                reg.HLVariable = reg.DEVariable;
            }
        }
        for (bit = 1 << (22 - clz(num)); bit; bit >>= 1) {
            ADD_HL_HL();
            if(num & bit) {
                ADD_HL_DE();
            }
        }
    } else if (num < 0x100) {
        if (ChangeRegisters) {
            EX_DE_HL();
        }
        LD_A(num);
        CALL(__imul_b);
        ResetReg(REGISTER_HL);
    } else {
        if (ChangeRegisters) {
            EX_DE_HL();
        }
        LD_BC_IMM(num);
        CALL(__imuls);
        ResetReg(REGISTER_HL);
    }
}
#endif

uint8_t getIndexOfOperator(uint8_t op) {
    return strchr(operators, op) - operators;
}

float execOp(uint8_t op, float operand1, float operand2) {
    switch (op) {
        case tDotIcon:
            return (float)((uint24_t)operand1 & (uint24_t)operand2);
        case tCrossIcon:
            return (float)((uint24_t)operand1 | (uint24_t)operand2);
        case tBoxIcon:
            return (float)((uint24_t)operand1 ^ (uint24_t)operand2);
        case tAnd:
            return operand1 && operand2;
        case tXor:
            return !operand1 != !operand2;
        case tOr:
            return operand1 || operand2;
        case tEQ:
            return operand1 == operand2;
        case tLT:
            return operand1 < operand2;
        case tGT:
            return operand1 > operand2;
        case tLE:
            return operand1 <= operand2;
        case tGE:
            return operand1 >= operand2;
        case tNE:
            return operand1 != operand2;
        case tMul:
            return operand1 * operand2;
        case tDiv:
            return operand1 / operand2;
        case tAdd:
            return operand1 + operand2;
        case tSub:
            return operand1 - operand2;
        default:
            return 0;
    }
}

uint8_t compileOperator(uint24_t index) {
    outputCurr     = getOutputElement(index);
    outputPrev     = getOutputElement(index - 1);
    outputPrevPrev = getOutputElement(index - 2);
    op = outputCurr.operand.op.type;
    
    return (*operatorsPointers[getIndexOfOperator(op)])();
}

uint8_t OperatorStore(void) {
	return VALID;
}

uint8_t OperatorBitAnd(void) {
	return VALID;
}

uint8_t OperatorBitOr(void) {
	return VALID;
}

uint8_t OperatorBitXor(void) {
	return VALID;
}

uint8_t OperatorAnd(void) {
	return VALID;
}

uint8_t OperatorXOr(void) {
	return VALID;
}

uint8_t OperatorOr(void) {
	return VALID;
}

uint8_t OperatorEQ(void) {
	return VALID;
}

uint8_t OperatorLT(void) {
	return VALID;
}

uint8_t OperatorGT(void) {
	return VALID;
}

uint8_t OperatorLE(void) {
	return VALID;
}

uint8_t OperatorGE(void) {
	return VALID;
}

uint8_t OperatorNE(void) {
	return VALID;
}

uint8_t OperatorMul(void) {
	return VALID;
}

uint8_t OperatorDiv(void) {
	return VALID;
}

uint8_t OperatorAdd(void) {
	return VALID;
}

uint8_t OperatorSub(void) {
	return VALID;
}


uint8_t (*operatorsPointers[17])(void) = {
    OperatorStore,
    OperatorBitAnd,
    OperatorBitOr,
    OperatorBitXor,
    OperatorAnd,
    OperatorXOr,
    OperatorOr,
    OperatorEQ,
    OperatorLT,
    OperatorGT,
    OperatorLE,
    OperatorGE,
    OperatorNE,
    OperatorMul,
    OperatorDiv,
    OperatorAdd,
    OperatorSub
};
