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
static bool canSwap;
static bool isFloat1;
static bool isFloat2;
extern bool isFloatExpression;
static uint8_t op;
static uint8_t type1;
static uint8_t type2;
static operand_t operand1;
static operand_t operand2;
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
    uint8_t operatorIndex;
    
    outputCurr     = getOutputElement(index);
    outputPrev     = getOutputElement(index - 1);
    outputPrevPrev = getOutputElement(index - 2);
    op             = outputCurr.operand.op.type;
    type1          = outputPrevPrev.type;
    type2          = outputPrev.type;
    isFloat1       = (type1 == TYPE_FLOAT || 
                     (type1 == TYPE_VARIABLE && prescan.variables[outputPrevPrev.operand.var].type == TYPE_FLOAT) || 
                     ((type1 == TYPE_CHAIN_ANS || type1 == TYPE_CHAIN_PUSH) && outputPrevPrev.operand.ansType == TYPE_FLOAT));
    isFloat2       = (type2 == TYPE_FLOAT || 
                     (type2 == TYPE_VARIABLE && prescan.variables[outputPrev.operand.var].type == TYPE_FLOAT) || 
                     ((type2 == TYPE_CHAIN_ANS || type2 == TYPE_CHAIN_PUSH) && outputPrev.operand.ansType == TYPE_FLOAT));
    operand1       = outputPrevPrev.operand;
    operand2       = outputPrev.operand;
    operatorIndex  = getIndexOfOperator(op);
    canSwap        = operatorCanSwap[operatorIndex];
    
    expr.returnRegister = REGISTER_HL;
    isFloatExpression = (isFloat1 || isFloat2);
    
    if ((type1 == TYPE_CHAIN_PUSH || type2 == TYPE_CHAIN_PUSH) && type2 != TYPE_CHAIN_ANS) {
        return E_ICE_ERROR;
    }
    
    // One of the arguments is a float
    if (isFloatExpression) {
        if (type1 == TYPE_CHAIN_PUSH) {
            // Convert the previous Ans and current Ans to floats
            OperatorFloatChainPushChainAns();
        } else {
            // Convert both arguments to floats
            (*operatorsFloatPointers[(type1 - 1) * 4 + type2 - 1])();
        }
        
        // Call the right code to finish the floats
        if (op == tMul) {
            CALL(__fmul);
        } else if (op == tAdd) {
            CALL(__fadd);
        } else if (op == tDiv) {
            CALL(__fdiv);
        } else if (op == tSub) {
            CALL(__fsub);
        }
    }
        
    // It was a CHAIN_PUSH | CHAIN_ANS
    else if (type1 == TYPE_CHAIN_PUSH) {
        // Call the right routine
        (*operatorsChainPushPointers[operatorIndex])();
    }
    
    // Both arguments are integers
    else {
        if (type1 == TYPE_BYTE || 
            type2 == TYPE_BYTE || 
            (op == tStore && type2 != TYPE_VARIABLE) ||
            (isFloatExpression && (op == tDotIcon || op == tCrossIcon || op == tBoxIcon))
        ) {
            return E_SYNTAX;
        }
        
        if (operatorCanSwap[operatorIndex] && (type1 <= TYPE_FLOAT || type2 == TYPE_CHAIN_ANS)) {
            OperatorsSwap();
        }
        
        (*operatorsPointers[operatorIndex * 16 + (type1 - 1) * 4 + type2 - 1])();
    }
    
    if (isFloatExpression) {
        expr.outputRegister = REGISTER_AUBC;
    } else {
        expr.outputRegister = expr.returnRegister;
    }
    
    return VALID;
}

void OperatorsSwap(void) {
    uint8_t temp;
    operand_t temp2;
    
    temp = isFloat1;
    isFloat1 = isFloat2;
    isFloat2 = temp;
    
    temp = type1;
    type1 = type2;
    type2 = type1;
    
    temp2 = operand1;
    operand1 = operand2;
    operand2 = temp2;
}

void OperatorError(void) {
}

/****************************
* All these functions use
* integers as arguments
****************************/

void OperatorStoreIntVariable(void) {
}

void OperatorStoreVariableVariable(void) {
}

void OperatorStoreChainAnsVariable(void) {
}

void OperatorBitAndVariableInt(void) {
}

void OperatorBitAndVariableVariable(void) {
}

void OperatorBitAndChainAnsInt(void) {
}

void OperatorBitAndChainAnsVariable(void) {
}

void OperatorBitOrVariableInt(void) {
}

void OperatorBitOrVariableVariable(void) {
}

void OperatorBitOrChainAnsInt(void) {
}

void OperatorBitOrChainAnsVariable(void) {
}

void OperatorBitXorVariableInt(void) {
}

void OperatorBitXorVariableVariable(void) {
}

void OperatorBitXorChainAnsInt(void) {
}

void OperatorBitXorChainAnsVariable(void) {
}

void AndInsert(void) {
}

void OperatorAndVariableInt(void) {
}

void OperatorAndVariableVariable(void) {
}

void OperatorAndChainAnsInt(void) {
}

void OperatorAndChainAnsVariable(void) {
}

void XorInsert(void) {
}

void OperatorXorVariableInt(void) {
}

void OperatorXorVariableVariable(void) {
}

void OperatorXorChainAnsInt(void) {
}

void OperatorXorChainAnsVariable(void) {
}

void OrInsert(void) {
}

void OperatorOrVariableInt(void) {
}

void OperatorOrVariableVariable(void) {
}

void OperatorOrChainAnsInt(void) {
}

void OperatorOrChainAnsVariable(void) {
}

void EQInsert(void) {
    OR_A_SBC_HL_DE();
    if (op == tEQ) {
        INC_HL();
        JR_Z(3);
        OR_A_SBC_HL_HL();
    } else {
        JR_Z(4);
        LD_HL_IMM(1);
    }
}

void OperatorEQVariableInt(void) {
}

void OperatorEQVariableVariable(void) {
}

void OperatorEQChainAnsInt(void) {
}

void OperatorEQChainAnsVariable(void) {
}

void OperatorLTIntVariable(void) {
}

void OperatorLTIntChainAns(void) {
}

void OperatorLTVariableInt(void) {
}

void OperatorLTVariableVariable(void) {
}

void OperatorLTVariableChainAns(void) {
}

void OperatorLTChainAnsInt(void) {
}

void OperatorLTChainAnsVariable(void) {
}

void OperatorGTIntVariable(void) {
}

void OperatorGTIntChainAns(void) {
}

void OperatorGTVariableInt(void) {
}

void OperatorGTVariableVariable(void) {
}

void OperatorGTVariableChainAns(void) {
}

void OperatorGTChainAnsInt(void) {
}

void OperatorGTChainAnsVariable(void) {
}

void OperatorLEIntVariable(void) {
}

void OperatorLEIntChainAns(void) {
}

void OperatorLEVariableInt(void) {
}

void OperatorLEVariableVariable(void) {
}

void OperatorLEVariableChainAns(void) {
}

void OperatorLEChainAnsInt(void) {
}

void OperatorLEChainAnsVariable(void) {
}

void OperatorGEIntVariable(void) {
}

void OperatorGEIntChainAns(void) {
}

void OperatorGEVariableInt(void) {
}

void OperatorGEVariableVariable(void) {
}

void OperatorGEVariableChainAns(void) {
}

void OperatorGEChainAnsInt(void) {
}

void OperatorGEChainAnsVariable(void) {
}

#define OperatorNEVariableInt       OperatorEQVariableInt
#define OperatorNEVariableVariable  OperatorEQVariableVariable
#define OperatorNEChainAnsInt       OperatorEQChainAnsInt
#define OperatorNEChainAnsVariable  OperatorEQChainAnsVariable

void OperatorMulVariableInt(void) {
}

void OperatorMulVariableVariable(void) {
}

void OperatorMulChainAnsInt(void) {
}

void OperatorMulChainAnsVariable(void) {
}

void OperatorDivIntVariable(void) {
}

void OperatorDivIntChainAns(void) {
}

void OperatorDivVariableInt(void) {
}

void OperatorDivVariableVariable(void) {
}

void OperatorDivVariableChainAns(void) {
}

void OperatorDivChainAnsInt(void) {
}

void OperatorDivChainAnsVariable(void) {
}

void OperatorAddChainAnsInt(void) {
    uint8_t a;
    uint24_t num = operand2.num;
    
    if (num < 5) {
        for (a = 0; a < num; a++) {
            if (expr.outputRegister == REGISTER_HL) {
                INC_HL();
            } else {
                INC_DE();
            }
        }
        expr.returnRegister = expr.outputRegister;
    } else if (num > 0x1000000 - 5) {
        for (a = 0; a < -num; a++) {
            if (expr.outputRegister == REGISTER_HL) {
                DEC_HL();
            } else {
                DEC_DE();
            }
        }
        expr.returnRegister = expr.outputRegister;
    } else {
        if (expr.outputRegister == REGISTER_HL) {
            LD_DE_IMM(num2);
        } else {
            LD_HL_IMM(num2);
        }
        ADD_HL_DE();
    }
}

void OperatorAddChainAnsVariable(void) {
    uint8_t offset = prescan.variables[operand2.var].offset;
    
    MaybeAToHL();
    if (expr.outputRegister == REGISTER_DE) {
        LD_HL_IND_IX_OFF(offset);
    } else {
        LD_DE_IND_IX-OFF(offset);
    }
    ADD_HL_DE();
}

void OperatorAddVariableInt(void) {
    LD_HL_IND_IX_OFF(prescan.variables[operand1.var].offset);
    OperatorAddChainAnsint(void);
}

void OperatorAddVariableVariable(void) {
    LD_HL_IND_IX_OFF(prescan.variables[operand1.var].offset);
    OperatorAddChainAnsVariable(void);
}

void OperatorSubChainAnsInt(void) {
    uint8_t a;
    uint24_t num = operand2.num;
    
    if (num < 5) {
        for (a = 0; a < num; a++) {
            if (expr.outputRegister == REGISTER_HL) {
                DEC_HL();
            } else {
                DEC_DE();
            }
        }
        expr.returnRegister = expr.outputRegister;
    } else if (num > 0x1000000 - 5) {
        for (a = 0; a < -num; a++) {
            if (expr.outputRegister == REGISTER_HL) {
                INC_HL();
            } else {
                INC_DE();
            }
        }
        expr.returnRegister = expr.outputRegister;
    } else {
        AnsToHL();
        LD_DE_IMM(num2);
        OR_A_SBC_HL_DE();
    }
}

void OperatorSubChainAnsVariable(void) {
    AnsToHL();
    LD_DE_IND_IX_OFF(prescan.variables[operand2.var].offset);
    OR_A_SBC_HL_DE();
}

void OperatorSubIntVariable(void) {
    LD_HL_IMM(operand1.num);
    OperatorSubChainAnsVariable(void) {
    }
}

void OperatorSubIntChainAns(void) {
    AnsToDE();
    LD_HL_IMM(operand1.num);
    OR_A_SBC_HL_DE();
}

void OperatorSubVariableInt(void) {
    LD_HL_IND_IX_OFF(prescan.variables[operand1.var].offset);
    OperatorSubChainAnsInt();
}

void OperatorSubVariableVariable(void) {
    LD_HL_IND_IX_OFF(prescan.variables[operand1.var].offset);
    OperatorSubChainAnsVariable();
}

void OperatorSubVariableChainAns(void) {
    AnsToDE();
    LD_HL_IND_IX_OFF(prescan.variables[operand1.var].offset);
    OR_A_SBC_HL_DE();
}

void (*operatorsPointers[272])(void) = {
    OperatorError,
    OperatorError,
    OperatorStoreIntVariable,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorStoreVariableVariable,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorStoreChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorBitAndVariableInt,
    OperatorError,
    OperatorBitAndVariableVariable,
    OperatorError,
    OperatorBitAndChainAnsInt,
    OperatorError,
    OperatorBitAndChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorBitOrVariableInt,
    OperatorError,
    OperatorBitOrVariableVariable,
    OperatorError,
    OperatorBitOrChainAnsInt,
    OperatorError,
    OperatorBitOrChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorBitXorVariableInt,
    OperatorError,
    OperatorBitXorVariableVariable,
    OperatorError,
    OperatorBitXorChainAnsInt,
    OperatorError,
    OperatorBitXorChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorAndVariableInt,
    OperatorError,
    OperatorAndVariableVariable,
    OperatorError,
    OperatorAndChainAnsInt,
    OperatorError,
    OperatorAndChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorXorVariableInt,
    OperatorError,
    OperatorXorVariableVariable,
    OperatorError,
    OperatorXorChainAnsInt,
    OperatorError,
    OperatorXorChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorOrVariableInt,
    OperatorError,
    OperatorOrVariableVariable,
    OperatorError,
    OperatorOrChainAnsInt,
    OperatorError,
    OperatorOrChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorEQVariableInt,
    OperatorError,
    OperatorEQVariableVariable,
    OperatorError,
    OperatorEQChainAnsInt,
    OperatorError,
    OperatorEQChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorLTIntVariable,
    OperatorLTIntChainAns,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorLTVariableInt,
    OperatorError,
    OperatorLTVariableVariable,
    OperatorLTVariableChainAns,
    OperatorLTChainAnsInt,
    OperatorError,
    OperatorLTChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorGTIntVariable,
    OperatorGTIntChainAns,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorGTVariableInt,
    OperatorError,
    OperatorGTVariableVariable,
    OperatorGTVariableChainAns,
    OperatorGTChainAnsInt,
    OperatorError,
    OperatorGTChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorLEIntVariable,
    OperatorLEIntChainAns,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorLEVariableInt,
    OperatorError,
    OperatorLEVariableVariable,
    OperatorLEVariableChainAns,
    OperatorLEChainAnsInt,
    OperatorError,
    OperatorLEChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorGEIntVariable,
    OperatorGEIntChainAns,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorGEVariableInt,
    OperatorError,
    OperatorGEVariableVariable,
    OperatorGEVariableChainAns,
    OperatorGEChainAnsInt,
    OperatorError,
    OperatorGEChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorNEVariableInt,
    OperatorError,
    OperatorNEVariableVariable,
    OperatorError,
    OperatorNEChainAnsInt,
    OperatorError,
    OperatorNEChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorMulVariableInt,
    OperatorError,
    OperatorMulVariableVariable,
    OperatorError,
    OperatorMulChainAnsInt,
    OperatorError,
    OperatorMulChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorDivIntVariable,
    OperatorDivIntChainAns,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorDivVariableInt,
    OperatorError,
    OperatorDivVariableVariable,
    OperatorDivVariableChainAns,
    OperatorDivChainAnsInt,
    OperatorError,
    OperatorDivChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorAddVariableInt,
    OperatorError,
    OperatorAddVariableVariable,
    OperatorError,
    OperatorAddChainAnsInt,
    OperatorError,
    OperatorAddChainAnsVariable,
    OperatorError,
    
    OperatorError,
    OperatorError,
    OperatorSubIntVariable,
    OperatorSubIntChainAns,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorError,
    OperatorSubVariableInt,
    OperatorError,
    OperatorSubVariableVariable,
    OperatorSubVariableChainAns,
    OperatorSubChainAnsInt,
    OperatorError,
    OperatorSubChainAnsVariable,
    OperatorError
};

void OperatorBitAndChainPushChainAns(void) {
    AnsToHL();
    POP_BC();
    if (op == tDotIcon) {
        CALL(__iand);
    } else if (op == tBoxIcon) {
        CALL(__ixor);
    } else {
        CALL(__ior);
    }
}

#define OperatorBitOrChainPushChainAns  OperatorBitAndChainPushChainAns
#define OperatorBitXorChainPushChainAns OperatorBitAndChainPushChainAns

void OperatorAndChainPushChainAns(void) {
    MaybeAToHL();
    if (expr.outputRegister == REGISTER_DE) {
        POP_HL();
    } else {
        POP_DE();
    }
    if (op == tAnd) {
        AndInsert();
    } else if (op == tOr) {
        OrInsert();
    } else if (op == tXor) {
        XorInsert();
    } else {
        EQInsert();
    }
}

#define OperatorXorChainPushChainAns OperatorAndChainPushChainAns
#define OperatorOrChainPushChainAns  OperatorAndChainPushChainAns
#define OperatorEQChainPushChainAns  OperatorAndChainPushChainAns

void OperatorLTChainPushChainAns(void) {
    if (op == tLT || op == tLE) {
        AnsToHL();
        POP_DE();
        SCF();
    } else {
        AnsToDE();
        POP_HL();
        OR_A_A();
    }
    SBC_HL_DE();
    SBC_HL_HL_INC_HL();
}

#define OperatorGTChainPushChainAns OperatorLTChainPushChainAns
#define OperatorLEChainPushChainAns OperatorLTChainPushChainAns
#define OperatorGEChainPushChainAns OperatorLTChainPushChainAns
#define OperatorNEChainPushChainAns OperatorEQChainPushChainAns

void OperatorMulChainPushChainAns(void) {
    AnsToHL();
    POP_BC();
    CALL(__imuls);
}

void OperatorDivChainPushChainAns(void) {
    AnsToBC();
    POP_HL();
    CALL(__idvrmu);
}

void OperatorAddChainPushChainAns(void) {
    MaybeAToHL();
    if (expr.outputRegister == REGISTER_DE) {
        POP_HL();
    } else {
        POP_DE();
    }
    ADD_HL_DE();
}

void OperatorSubChainPushChainAns(void) {
    AnsToDE();
    POP_HL();
    OR_A_SBC_HL_DE();
}

void (*operatorsChainPushPointers[17])(void) = {
    OperatorError,
    OperatorBitAndChainPushChainAns,
    OperatorBitOrChainPushChainAns,
    OperatorBitXorChainPushChainAns,
    OperatorAndChainPushChainAns,
    OperatorXorChainPushChainAns,
    OperatorOrChainPushChainAns,
    OperatorEQChainPushChainAns,
    OperatorLTChainPushChainAns,
    OperatorGTChainPushChainAns,
    OperatorLEChainPushChainAns,
    OperatorGEChainPushChainAns,
    OperatorNEChainPushChainAns,
    OperatorMulChainPushChainAns,
    OperatorDivChainPushChainAns,
    OperatorAddChainPushChainAns,
    OperatorSubChainPushChainAns
};

void OperatorFloatIntChainAns(void) {
    float num = operand1.num;
    
    if (canSwap) {
        LD_E_HL_FLOAT(num);
    } else {
        FloatAnsToEUHL();
        LD_A_BC_FLOAT(num);
    }
}

void OperatorFloatIntVariable(void) {
    LD_E_HL_IND_IX_OFF(prescan.variables[operand2.var].offset);
    LD_A_BC_FLOAT(operand1.num);
}

void OperatorFloatFloatVariable(void) {
    uint8_t offset = prescan.variables[operand2.var].offset;
    
    if (isFloat2) {
        LD_E_HL_IND_IX_OFF(offset);
    } else {
        LD_BC_IND_IX_OFF(offset);
        CALL(__ultof);
        FloatAnsToEUHL();
    }
    LD_A_BC_FLOAT(operand1.num);
}

void OperatorFloatFloatChainAns(void) {
    float num = operand1.num;
    
    if (isFloat2) {
        if (canSwap) {
            LD_E_HL_FLOAT(num);
        } else {
            FloatAnsToEUHL();
            LD_A_BC_FLOAT(num);
        }
    } else {
        AnsToBC();
        CALL(__ultof);
        isFloat2 = true;
        OperatorFloatFloatChainAns();
    }
}

void OperatorFloatVariableInt(void) {
    LD_A_BC_IND_IX_OFF(prescan.variables[operand1.var].offset);
    LD_E_HL_FLOAT(operand2.num);
}

void OperatorFloatVariableFloat(void) {
    uint8_t offset = prescan.variables[operand1.var].offset;
    
    if (isFloat1) {
        LD_A_BC_IND_IX_OFF(offset);
    } else {
        LD_BC_IND_IX_OFF(offset);
        CALL(__ultof);
    }
    LD_E_HL_FLOAT(operand2.num);
}

void OperatorFloatVariableVariable(void) {
    uint8_t offset1 = prescan.variables[operand1.var].offset;
    uint8_t offset2 = prescan.variables[operand2.var].offset;
    
    if (isFloat1) {
        if (isFloat2) {
            LD_A_BC_IND_IX_OFF(offset1);
            LD_E_HL_IND_IX_OFF(offset2);
        } else {
            LD_BC_IND_IX_OFF(offset2);
            CALL(__ultof);
            if (canSwap) {
                LD_E_HL_IND_IX_OFF(offset1);
            } else {
                FloatAnsToEUHL();
                LD_A_BC_IND_IX_OFF(offset1);
            }
        }
    } else {
        LD_BC_IND_IX_OFF(offset1);
        CALL(__ultof);
        LD_E_HL_IND_IX_OFF(offset2);
    }
}

void OperatorFloatVariableChainAns(void) {
    uint8_t offset = prescan.variables[operand1.var].offset;
    
    if (isFloat1) {
        if (isFloat2) {
            FloatAnsToEUHL();
            LD_A_BC_IND_IX_OFF(offset);
        } else {
            AnsToBC();
            CALL(__ultof);
            if (canSwap) {
                LD_E_HL_IND_IX_OFF(offset);
            } else {
                FloatAnsToEUHL();
                LD_A_BC_IND_IX_OFF(offset);
            }
        }
    } else {
        FloatAnsToEUHL();
        LD_BC_IND_IX_OFF(offset);
        CALL(__ultof);
    }
}

void OperatorFloatChainAnsInt(void) {
    LD_E_HL_FLOAT(operand2.num);
}

void OperatorFloatChainAnsFloat(void) {
    if (!isFloat1) {
        AnsToBC();
        CALL(__ultof);
    }
    LD_E_HL_FLOAT(operand2.num);
}

void OperatorFloatChainAnsVariable(void) {
    uint8_t offset = prescan.variables[operand2.var].offset;
    
    if (isFloat1) {
        if (isFloat2) {
            LD_E_HL_IND_IX_OFF(offset);
        } else {
            if (canSwap) {
                FloatAnsToEUHL();
                LD_BC_IND_IX_OFF(offset);
                CALL(__ultof);
            } else {
                PUSH_BC();
                PUSH_AF();
                LD_BC_IND_IX_OFF(offset);
                CALL(__ultof);
                FloatAnsToEUHL();
                POP_AF();
                POP_BC();
            }
        }
    } else {
        AnsToBC();
        CALL(__ultof);
        LD_E_HL_IND_IX_OFF(offset);
    }
}

void OperatorFloatChainPushChainAns(void) {
    if (isFloat1) {
        if (!isFloat2) {
            AnsToBC();
            CALL(__ultof);
        }
        if (canSwap) {
            POP_HL();
            POP_DE();
        } else {
            FloatAnsToEUHL();
            POP_BC();
            POP_AF();
        }
    } else {
        FloatAnsToEUHL();
        POP_BC();
        CALL(__ultof);
    }
}

void (*operatorsFloatPointers[16])(void) = {
    OperatorError,
    OperatorError,
    OperatorFloatIntVariable,
    OperatorFloatIntChainAns,
    OperatorError,
    OperatorError,
    OperatorFloatFloatVariable,
    OperatorFloatFloatChainAns,
    OperatorFloatVariableInt,
    OperatorFloatVariableFloat,
    OperatorFloatVariableVariable,
    OperatorFloatVariableChainAns,
    OperatorFloatChainAnsInt,
    OperatorFloatChainAnsFloat,
    OperatorFloatChainAnsVariable,
    OperatorError
};
