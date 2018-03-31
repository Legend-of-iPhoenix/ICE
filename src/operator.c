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

const char operators[18]              = {tStore, tDotIcon, tCrossIcon, tBoxIcon, tAnd, tXor, tOr, tEQ, tLT, tGT, tLE, tGE, tNE, tMul, tDiv, tAdd, tSub, 0};
const uint8_t operatorPrecedence[17]  = {0, 6, 8, 8, 2, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 4, 4};
const uint8_t operatorPrecedence2[17] = {9, 6, 8, 8, 2, 1, 1, 3, 3, 3, 3, 3, 3, 5, 5, 4, 4};
const uint8_t operatorCanSwap[17]     = {0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 0}; // Used for operators which can swap the operands, i.e. A*B = B*A
static bool isFloat1;
static bool isFloat2;
static uint8_t op;
static uint8_t type1;
static uint8_t type2;
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
    type1 = outputPrevPrev.type;
    type2 = outputPrev.type;
    isFloat1 = (type1 == TYPE_FLOAT || (type1 == TYPE_VARIABLE && prescan.variables[outputPrevPrev.operand.var].type == TYPE_FLOAT));
    isFloat2 = (type2 == TYPE_FLOAT || (type2 == TYPE_VARIABLE && prescan.variables[outputPrev.operand.var].type == TYPE_FLOAT));
    
    if (type1 == TYPE_CHAIN_PUSH || type2 == TYPE_CHAIN_PUSH) {
        if (type2 != TYPE_CHAIN_ANS) {
            return E_ICE_ERROR;
        }
        
        // Call chain push function
    } else {
        if (type1 == type2 && (type1 <= TYPE_FLOAT || type1 == TYPE_CHAIN_ANS)) {
            return E_ICE_ERROR;
        }
        
        if (type1 == TYPE_BYTE || type2 == TYPE_BYTE) {
            return E_SYNTAX;
        }
        
        (*operatorsPointers[getIndexOfOperator(op)])();
    }
    
    return VALID;
}

void OperatorError(void) {
}

void OperatorStoreIntVariable(void) {
}

void OperatorStoreIntChainAns(void) {
}

void OperatorStoreFloatVariable(void) {
}

void OperatorStoreFloatChainAns(void) {
}

void OperatorStoreVariableInt(void) {
}

void OperatorStoreVariableFloat(void) {
}

void OperatorStoreVariableVariable(void) {
}

void OperatorStoreVariableChainAns(void) {
}

void OperatorStoreChainAnsInt(void) {
}

void OperatorStoreChainAnsFloat(void) {
}

void OperatorStoreChainAnsVariable(void) {
}

void OperatorBitAndIntVariable(void) {
}

void OperatorBitAndIntChainAns(void) {
}

void OperatorBitAndFloatVariable(void) {
}

void OperatorBitAndFloatChainAns(void) {
}

void OperatorBitAndVariableInt(void) {
}

void OperatorBitAndVariableFloat(void) {
}

void OperatorBitAndVariableVariable(void) {
}

void OperatorBitAndVariableChainAns(void) {
}

void OperatorBitAndChainAnsInt(void) {
}

void OperatorBitAndChainAnsFloat(void) {
}

void OperatorBitAndChainAnsVariable(void) {
}

void OperatorBitOrIntVariable(void) {
}

void OperatorBitOrIntChainAns(void) {
}

void OperatorBitOrFloatVariable(void) {
}

void OperatorBitOrFloatChainAns(void) {
}

void OperatorBitOrVariableInt(void) {
}

void OperatorBitOrVariableFloat(void) {
}

void OperatorBitOrVariableVariable(void) {
}

void OperatorBitOrVariableChainAns(void) {
}

void OperatorBitOrChainAnsInt(void) {
}

void OperatorBitOrChainAnsFloat(void) {
}

void OperatorBitOrChainAnsVariable(void) {
}

void OperatorBitXorIntVariable(void) {
}

void OperatorBitXorIntChainAns(void) {
}

void OperatorBitXorFloatVariable(void) {
}

void OperatorBitXorFloatChainAns(void) {
}

void OperatorBitXorVariableInt(void) {
}

void OperatorBitXorVariableFloat(void) {
}

void OperatorBitXorVariableVariable(void) {
}

void OperatorBitXorVariableChainAns(void) {
}

void OperatorBitXorChainAnsInt(void) {
}

void OperatorBitXorChainAnsFloat(void) {
}

void OperatorBitXorChainAnsVariable(void) {
}

void OperatorAndIntVariable(void) {
}

void OperatorAndIntChainAns(void) {
}

void OperatorAndFloatVariable(void) {
}

void OperatorAndFloatChainAns(void) {
}

void OperatorAndVariableInt(void) {
}

void OperatorAndVariableFloat(void) {
}

void OperatorAndVariableVariable(void) {
}

void OperatorAndVariableChainAns(void) {
}

void OperatorAndChainAnsInt(void) {
}

void OperatorAndChainAnsFloat(void) {
}

void OperatorAndChainAnsVariable(void) {
}

void OperatorXorIntVariable(void) {
}

void OperatorXorIntChainAns(void) {
}

void OperatorXorFloatVariable(void) {
}

void OperatorXorFloatChainAns(void) {
}

void OperatorXorVariableInt(void) {
}

void OperatorXorVariableFloat(void) {
}

void OperatorXorVariableVariable(void) {
}

void OperatorXorVariableChainAns(void) {
}

void OperatorXorChainAnsInt(void) {
}

void OperatorXorChainAnsFloat(void) {
}

void OperatorXorChainAnsVariable(void) {
}

void OperatorOrIntVariable(void) {
}

void OperatorOrIntChainAns(void) {
}

void OperatorOrFloatVariable(void) {
}

void OperatorOrFloatChainAns(void) {
}

void OperatorOrVariableInt(void) {
}

void OperatorOrVariableFloat(void) {
}

void OperatorOrVariableVariable(void) {
}

void OperatorOrVariableChainAns(void) {
}

void OperatorOrChainAnsInt(void) {
}

void OperatorOrChainAnsFloat(void) {
}

void OperatorOrChainAnsVariable(void) {
}

void OperatorEQIntVariable(void) {
}

void OperatorEQIntChainAns(void) {
}

void OperatorEQFloatVariable(void) {
}

void OperatorEQFloatChainAns(void) {
}

void OperatorEQVariableInt(void) {
}

void OperatorEQVariableFloat(void) {
}

void OperatorEQVariableVariable(void) {
}

void OperatorEQVariableChainAns(void) {
}

void OperatorEQChainAnsInt(void) {
}

void OperatorEQChainAnsFloat(void) {
}

void OperatorEQChainAnsVariable(void) {
}

void OperatorLTIntVariable(void) {
}

void OperatorLTIntChainAns(void) {
}

void OperatorLTFloatVariable(void) {
}

void OperatorLTFloatChainAns(void) {
}

void OperatorLTVariableInt(void) {
}

void OperatorLTVariableFloat(void) {
}

void OperatorLTVariableVariable(void) {
}

void OperatorLTVariableChainAns(void) {
}

void OperatorLTChainAnsInt(void) {
}

void OperatorLTChainAnsFloat(void) {
}

void OperatorLTChainAnsVariable(void) {
}

void OperatorGTIntVariable(void) {
}

void OperatorGTIntChainAns(void) {
}

void OperatorGTFloatVariable(void) {
}

void OperatorGTFloatChainAns(void) {
}

void OperatorGTVariableInt(void) {
}

void OperatorGTVariableFloat(void) {
}

void OperatorGTVariableVariable(void) {
}

void OperatorGTVariableChainAns(void) {
}

void OperatorGTChainAnsInt(void) {
}

void OperatorGTChainAnsFloat(void) {
}

void OperatorGTChainAnsVariable(void) {
}

void OperatorLEIntVariable(void) {
}

void OperatorLEIntChainAns(void) {
}

void OperatorLEFloatVariable(void) {
}

void OperatorLEFloatChainAns(void) {
}

void OperatorLEVariableInt(void) {
}

void OperatorLEVariableFloat(void) {
}

void OperatorLEVariableVariable(void) {
}

void OperatorLEVariableChainAns(void) {
}

void OperatorLEChainAnsInt(void) {
}

void OperatorLEChainAnsFloat(void) {
}

void OperatorLEChainAnsVariable(void) {
}

void OperatorGEIntVariable(void) {
}

void OperatorGEIntChainAns(void) {
}

void OperatorGEFloatVariable(void) {
}

void OperatorGEFloatChainAns(void) {
}

void OperatorGEVariableInt(void) {
}

void OperatorGEVariableFloat(void) {
}

void OperatorGEVariableVariable(void) {
}

void OperatorGEVariableChainAns(void) {
}

void OperatorGEChainAnsInt(void) {
}

void OperatorGEChainAnsFloat(void) {
}

void OperatorGEChainAnsVariable(void) {
}

void OperatorNEIntVariable(void) {
}

void OperatorNEIntChainAns(void) {
}

void OperatorNEFloatVariable(void) {
}

void OperatorNEFloatChainAns(void) {
}

void OperatorNEVariableInt(void) {
}

void OperatorNEVariableFloat(void) {
}

void OperatorNEVariableVariable(void) {
}

void OperatorNEVariableChainAns(void) {
}

void OperatorNEChainAnsInt(void) {
}

void OperatorNEChainAnsFloat(void) {
}

void OperatorNEChainAnsVariable(void) {
}

void OperatorMulIntVariable(void) {
}

void OperatorMulIntChainAns(void) {
}

void OperatorMulFloatVariable(void) {
}

void OperatorMulFloatChainAns(void) {
}

void OperatorMulVariableInt(void) {
}

void OperatorMulVariableFloat(void) {
}

void OperatorMulVariableVariable(void) {
}

void OperatorMulVariableChainAns(void) {
}

void OperatorMulChainAnsInt(void) {
}

void OperatorMulChainAnsFloat(void) {
}

void OperatorMulChainAnsVariable(void) {
}

void OperatorDivIntVariable(void) {
}

void OperatorDivIntChainAns(void) {
}

void OperatorDivFloatVariable(void) {
}

void OperatorDivFloatChainAns(void) {
}

void OperatorDivVariableInt(void) {
}

void OperatorDivVariableFloat(void) {
}

void OperatorDivVariableVariable(void) {
}

void OperatorDivVariableChainAns(void) {
}

void OperatorDivChainAnsInt(void) {
}

void OperatorDivChainAnsFloat(void) {
}

void OperatorDivChainAnsVariable(void) {
}

void OperatorAddIntVariable(void) {
}

void OperatorAddIntChainAns(void) {
}

void OperatorAddFloatVariable(void) {
}

void OperatorAddFloatChainAns(void) {
}

void OperatorAddVariableInt(void) {
}

void OperatorAddVariableFloat(void) {
}

void OperatorAddVariableVariable(void) {
}

void OperatorAddVariableChainAns(void) {
}

void OperatorAddChainAnsInt(void) {
}

void OperatorAddChainAnsFloat(void) {
}

void OperatorAddChainAnsVariable(void) {
}

void OperatorSubIntVariable(void) {
}

void OperatorSubIntChainAns(void) {
}

void OperatorSubFloatVariable(void) {
}

void OperatorSubFloatChainAns(void) {
}

void OperatorSubVariableInt(void) {
}

void OperatorSubVariableFloat(void) {
}

void OperatorSubVariableVariable(void) {
}

void OperatorSubVariableChainAns(void) {
}

void OperatorSubChainAnsInt(void) {
}

void OperatorSubChainAnsFloat(void) {
}

void OperatorSubChainAnsVariable(void) {
}

void (*operatorsPointers[272])(void) = {
    OperatorError,
    OperatorError,
    OperatorStoreIntVariable,
    OperatorStoreIntChainAns,
    OperatorError,
    OperatorError,
    OperatorStoreFloatVariable,
    OperatorStoreFloatChainAns,
    OperatorStoreVariableInt,
    OperatorStoreVariableFloat,
    OperatorStoreVariableVariable,
    OperatorStoreVariableChainAns,
    OperatorStoreChainAnsInt,
    OperatorStoreChainAnsFloat,
    OperatorStoreChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorBitAndIntVariable,
    OperatorBitAndIntChainAns,
    OperatorError,
    OperatorError,
    OperatorBitAndFloatVariable,
    OperatorBitAndFloatChainAns,
    OperatorBitAndVariableInt,
    OperatorBitAndVariableFloat,
    OperatorBitAndVariableVariable,
    OperatorBitAndVariableChainAns,
    OperatorBitAndChainAnsInt,
    OperatorBitAndChainAnsFloat,
    OperatorBitAndChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorBitOrIntVariable,
    OperatorBitOrIntChainAns,
    OperatorError,
    OperatorError,
    OperatorBitOrFloatVariable,
    OperatorBitOrFloatChainAns,
    OperatorBitOrVariableInt,
    OperatorBitOrVariableFloat,
    OperatorBitOrVariableVariable,
    OperatorBitOrVariableChainAns,
    OperatorBitOrChainAnsInt,
    OperatorBitOrChainAnsFloat,
    OperatorBitOrChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorBitXorIntVariable,
    OperatorBitXorIntChainAns,
    OperatorError,
    OperatorError,
    OperatorBitXorFloatVariable,
    OperatorBitXorFloatChainAns,
    OperatorBitXorVariableInt,
    OperatorBitXorVariableFloat,
    OperatorBitXorVariableVariable,
    OperatorBitXorVariableChainAns,
    OperatorBitXorChainAnsInt,
    OperatorBitXorChainAnsFloat,
    OperatorBitXorChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorAndIntVariable,
    OperatorAndIntChainAns,
    OperatorError,
    OperatorError,
    OperatorAndFloatVariable,
    OperatorAndFloatChainAns,
    OperatorAndVariableInt,
    OperatorAndVariableFloat,
    OperatorAndVariableVariable,
    OperatorAndVariableChainAns,
    OperatorAndChainAnsInt,
    OperatorAndChainAnsFloat,
    OperatorAndChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorXorIntVariable,
    OperatorXorIntChainAns,
    OperatorError,
    OperatorError,
    OperatorXorFloatVariable,
    OperatorXorFloatChainAns,
    OperatorXorVariableInt,
    OperatorXorVariableFloat,
    OperatorXorVariableVariable,
    OperatorXorVariableChainAns,
    OperatorXorChainAnsInt,
    OperatorXorChainAnsFloat,
    OperatorXorChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorOrIntVariable,
    OperatorOrIntChainAns,
    OperatorError,
    OperatorError,
    OperatorOrFloatVariable,
    OperatorOrFloatChainAns,
    OperatorOrVariableInt,
    OperatorOrVariableFloat,
    OperatorOrVariableVariable,
    OperatorOrVariableChainAns,
    OperatorOrChainAnsInt,
    OperatorOrChainAnsFloat,
    OperatorOrChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorEQIntVariable,
    OperatorEQIntChainAns,
    OperatorError,
    OperatorError,
    OperatorEQFloatVariable,
    OperatorEQFloatChainAns,
    OperatorEQVariableInt,
    OperatorEQVariableFloat,
    OperatorEQVariableVariable,
    OperatorEQVariableChainAns,
    OperatorEQChainAnsInt,
    OperatorEQChainAnsFloat,
    OperatorEQChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorLTIntVariable,
    OperatorLTIntChainAns,
    OperatorError,
    OperatorError,
    OperatorLTFloatVariable,
    OperatorLTFloatChainAns,
    OperatorLTVariableInt,
    OperatorLTVariableFloat,
    OperatorLTVariableVariable,
    OperatorLTVariableChainAns,
    OperatorLTChainAnsInt,
    OperatorLTChainAnsFloat,
    OperatorLTChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorGTIntVariable,
    OperatorGTIntChainAns,
    OperatorError,
    OperatorError,
    OperatorGTFloatVariable,
    OperatorGTFloatChainAns,
    OperatorGTVariableInt,
    OperatorGTVariableFloat,
    OperatorGTVariableVariable,
    OperatorGTVariableChainAns,
    OperatorGTChainAnsInt,
    OperatorGTChainAnsFloat,
    OperatorGTChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorLEIntVariable,
    OperatorLEIntChainAns,
    OperatorError,
    OperatorError,
    OperatorLEFloatVariable,
    OperatorLEFloatChainAns,
    OperatorLEVariableInt,
    OperatorLEVariableFloat,
    OperatorLEVariableVariable,
    OperatorLEVariableChainAns,
    OperatorLEChainAnsInt,
    OperatorLEChainAnsFloat,
    OperatorLEChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorGEIntVariable,
    OperatorGEIntChainAns,
    OperatorError,
    OperatorError,
    OperatorGEFloatVariable,
    OperatorGEFloatChainAns,
    OperatorGEVariableInt,
    OperatorGEVariableFloat,
    OperatorGEVariableVariable,
    OperatorGEVariableChainAns,
    OperatorGEChainAnsInt,
    OperatorGEChainAnsFloat,
    OperatorGEChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorNEIntVariable,
    OperatorNEIntChainAns,
    OperatorError,
    OperatorError,
    OperatorNEFloatVariable,
    OperatorNEFloatChainAns,
    OperatorNEVariableInt,
    OperatorNEVariableFloat,
    OperatorNEVariableVariable,
    OperatorNEVariableChainAns,
    OperatorNEChainAnsInt,
    OperatorNEChainAnsFloat,
    OperatorNEChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorMulIntVariable,
    OperatorMulIntChainAns,
    OperatorError,
    OperatorError,
    OperatorMulFloatVariable,
    OperatorMulFloatChainAns,
    OperatorMulVariableInt,
    OperatorMulVariableFloat,
    OperatorMulVariableVariable,
    OperatorMulVariableChainAns,
    OperatorMulChainAnsInt,
    OperatorMulChainAnsFloat,
    OperatorMulChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorDivIntVariable,
    OperatorDivIntChainAns,
    OperatorError,
    OperatorError,
    OperatorDivFloatVariable,
    OperatorDivFloatChainAns,
    OperatorDivVariableInt,
    OperatorDivVariableFloat,
    OperatorDivVariableVariable,
    OperatorDivVariableChainAns,
    OperatorDivChainAnsInt,
    OperatorDivChainAnsFloat,
    OperatorDivChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorAddIntVariable,
    OperatorAddIntChainAns,
    OperatorError,
    OperatorError,
    OperatorAddFloatVariable,
    OperatorAddFloatChainAns,
    OperatorAddVariableInt,
    OperatorAddVariableFloat,
    OperatorAddVariableVariable,
    OperatorAddVariableChainAns,
    OperatorAddChainAnsInt,
    OperatorAddChainAnsFloat,
    OperatorAddChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorSubIntVariable,
    OperatorSubIntChainAns,
    OperatorError,
    OperatorError,
    OperatorSubFloatVariable,
    OperatorSubFloatChainAns,
    OperatorSubVariableInt,
    OperatorSubVariableFloat,
    OperatorSubVariableVariable,
    OperatorSubVariableChainAns,
    OperatorSubChainAnsInt,
    OperatorSubChainAnsFloat,
    OperatorSubChainAnsVariable,
    OperatorError
};
