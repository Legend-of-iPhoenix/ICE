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

const uint8_t implementedFunctions[AMOUNT_OF_FUNCTIONS][4] = {
// function / second byte / amount of arguments / disallow arguments as numbers
    {tNot,      0,              1,   1},
    {tMin,      0,              2,   1},
    {tMax,      0,              2,   1},
    {tMean,     0,              2,   1},
    {tSqrt,     0,              1,   1},
    {tDet,      0,              -1,  0},
    {tSum,      0,              -1,  0},
    {tSin,      0,              1,   1},
    {tCos,      0,              1,   1},
    {tRand,     0,              0,   0},
    {tAns,      0,              0,   0},
    {tLParen,   0,              1,   0},
    {tLBrace,   0,              1,   0},
    {tLBrack,   0,              1,   0},
    {tExtTok,   tRemainder,     2,   1},
    {tExtTok,   tCheckTmr,      2,   0},
    {tExtTok,   tStartTmr,      0,   0},
    {t2ByteTok, tSubStrng,      3,   0},
    {t2ByteTok, tLength,        1,   0},
    {t2ByteTok, tRandInt,       2,   0},
    {tVarOut,   tDefineSprite,  -1,  0},
    {tVarOut,   tData,          -1,  0},
    {tVarOut,   tCopy,          -1,  0},
    {tVarOut,   tAlloc,         1,   0},
    {tVarOut,   tDefineTilemap, -1,  0},
    {tVarOut,   tCopyData,      -1,  0},
    {tVarOut,   tLoadData,      3,   0},
    {tVarOut,   tSetBrightness, 1,   0}
};

bool inExpression = false;
bool inFunction = false;
bool canUseMask = true;
bool allowExpression = true;
uint8_t returnToken = 0;
extern uint8_t (*functions[256])(uint8_t tok);
extern uint24_t outputElements;
extern uint24_t stackElements;

uint8_t ParseProgram(void) {
    int token;
    uint8_t ret, currentGoto, currentLbl;
    
    LD_IX_IMM(IX_VARIABLES);
    
    // Eventually seed the rand
    if (prescan.amountOfRandRoutines) {
        ice.programDataPtr -= SIZEOF_RAND_DATA;
        ice.randAddr = (uint24_t)ice.programDataPtr;
        memcpy(ice.programDataPtr, SRandData, SIZEOF_RAND_DATA);
        ice.dataOffsetStack[ice.dataOffsetElements++] = (uint24_t*)(ice.randAddr + 2);
        w24((uint8_t*)(ice.randAddr + 2), ice.randAddr + 102);
        ice.dataOffsetStack[ice.dataOffsetElements++] = (uint24_t*)(ice.randAddr + 6);
        w24((uint8_t*)(ice.randAddr + 6), ice.randAddr + 105);
        ice.dataOffsetStack[ice.dataOffsetElements++] = (uint24_t*)(ice.randAddr + 19);
        w24((uint8_t*)(ice.randAddr + 19), ice.randAddr + 102);

        LD_HL_IND(0xF30044);
        ProgramPtrToOffsetStack();
        CALL((uint24_t)ice.programDataPtr);
    }
    
    if ((ret = ParseUntilEnd()) != VALID) {
        return ret;
    }
    
    if (!ice.lastTokenIsReturn) {
        RET();
    }
    
    // Find all the matching Goto's/Lbl's
    for (currentGoto = 0; currentGoto < prescan.amountOfGotos; currentGoto++) {
        label_t *curGoto = &ice.GotoStack[currentGoto];

        for (currentLbl = 0; currentLbl < prescan.amountOfLbls; currentLbl++) {
            label_t *curLbl = &ice.LblStack[currentLbl];

            if (!memcmp(curLbl->name, curGoto->name, 10)) {
                w24((uint8_t*)(curGoto->addr + 1), curLbl->addr - (uint24_t)ice.programData + PRGM_START);
                goto findNextLabel;
            }
        }

        // Label not found
        displayLabelError(curGoto->name);
        _seek(curGoto->offset, SEEK_SET, ice.inPrgm);
        
        return W_VALID;
findNextLabel:;
    }
    
    return VALID;
}

uint8_t ParseUntilEnd(void) {
    int token;
    uint8_t ret;
    
    // Do things based on the token
    while ((token = _getc()) != EOF) {
        if ((ret = (*functions[token])(token)) != VALID || !returnToken) {
            return ret;
        }
    }
    
    // Parse the last expression
    ParseNewLine(tEnter);
    
    return VALID;
}

uint8_t ParseNumber(uint8_t tok) {
    char input[100]= {0};
    uint8_t a = 1, amountOfPts = 0;
    element_t newElement = {0};
    
    inExpression = true;
    canUseMask = false;
    
    // Fetch number
    input[0] = tok;
    while ((((tok = _getc()) >= t0 && tok <= t9) || tok == tDecPt) && a < 100) {
        if (tok == tDecPt) {
            amountOfPts++;
        }
        input[a++] = tok;
    }
    SeekMinus1();
    
    // Only 1 decimal point is allowed
    if (amountOfPts > 1) {
        return E_SYNTAX;
    }
    
    // Push to the stack
    newElement.needRelocate = newElement.allowStoreTo = false;
    newElement.type = TYPE_NUMBER;
    newElement.operand.num = atof(input);
    outputStackPush(newElement);
    
    return VALID;
}

uint8_t ParseEE(uint8_t tok) {
    uint24_t output = tok, token;
    element_t newElement = {0};
    
    inExpression = true;
    canUseMask = false;

    // Fetch hexadecimal
    while ((tok = IsHexadecimal(token = _getc())) != 16) {
        output = (output << 4) + tok;
    }
    SeekMinus1();
    
    // Push to the stack
    newElement.needRelocate = newElement.allowStoreTo = false;
    newElement.type = TYPE_NUMBER;
    newElement.operand.num = output;
    outputStackPush(newElement);
    
    return VALID;
}

uint8_t ParsePi(uint8_t tok) {
    uint24_t output = tok, token;
    element_t newElement = {0};
    
    inExpression = true;
    canUseMask = false;

    // Fetch binary number
    while ((tok = (token = _getc())) >= t0 && tok <= t1) {
        output = (output << 1) + tok - t0;
    }
    SeekMinus1();
    
    // Push to the stack
    newElement.needRelocate = newElement.allowStoreTo = false;
    newElement.type = TYPE_NUMBER;
    newElement.operand.num = output;
    outputStackPush(newElement);
    
    return VALID;
}

uint8_t ParseChs(uint8_t tok) {
    inExpression = true;
    canUseMask = false;
    
    tok = _getc();
    if (tok >= t0 && tok <= t9) {
        SeekMinus1();
        
        return ParseNumber(tChs);
    } else {
        element_t newElement = {0};
        
        // Push to the stack
        newElement.needRelocate = newElement.allowStoreTo = false;
        newElement.type = TYPE_NUMBER;
        newElement.operand.num = -1;
        outputStackPush(newElement);
        
        return ParseOperator(tMul);
    }
}

uint8_t ParseDegree(uint8_t tok) {
    element_t newElement = {0};
    
    inExpression = true;
    canUseMask = false;
    
    tok = _getc();
    newElement.needRelocate = newElement.allowStoreTo = false;
    newElement.type = TYPE_NUMBER;
    if (tok >= tA && tok <= tTheta) {
        newElement.operand.num = IX_VARIABLES + prescan.variables[GetVariableOffset(tok)].offset;
    } else if (tok == tVarLst) {
        newElement.operand.num = prescan.OSLists[_getc()];
    } else if (tok == tVarStrng) {
        newElement.operand.num = prescan.OSStrings[_getc()];
    } else {
        return E_SYNTAX;
    }
    outputStackPush(newElement);
    
    return VALID;
}

uint8_t ParseOSList(uint8_t tok) {
    element_t newElement = {0};
    
    newElement.needRelocate = false;
    tok = _getc();
    
    // Element of list
    if (_getc() == tLParen) {
        newElement.allowStoreTo = true;
        newElement.type = TYPE_FUNCTION;
        newElement.operand.func.function = tVarLst;
        newElement.operand.func.function2 = tok;
        newElement.operand.func.mask = TYPE_MASK_U24;
        newElement.operand.func.amountOfArgs = 1;
        stackPush(newElement);
    } else {
        SeekMinus1();
        newElement.allowStoreTo = false;
        newElement.type = TYPE_NUMBER;
        newElement.operand.num = prescan.OSLists[tok];
        outputStackPush(newElement);
    }
    
    return VALID;
}

uint8_t ParseOSString(uint8_t tok) {
    element_t newElement = {0};
    
    inExpression = true;
    canUseMask = false;
    
    // Push to the stack
    newElement.needRelocate = false;
    newElement.allowStoreTo = true;
    newElement.type = TYPE_NUMBER;
    newElement.operand.num = prescan.OSStrings[_getc()];
    outputStackPush(newElement);
    
    return VALID;
}

uint8_t ParseString(uint8_t tok) {
    element_t newElement = {0};
    
    inExpression = true;
    canUseMask = false;
    
    newElement.needRelocate = true;
    newElement.allowStoreTo = false;
    newElement.type = TYPE_NUMBER;
    
    return VALID;
}

uint8_t ParseVariable(uint8_t tok) {
    element_t newElement = {0};
    
    inExpression = true;
    canUseMask = false;
    
    // Push to the stack
    newElement.needRelocate = false;
    newElement.allowStoreTo = true;
    newElement.type = TYPE_VARIABLE;
    newElement.operand.var = GetVariableOffset(tok);
    outputStackPush(newElement);
    
    return VALID;
}

uint8_t ParseOperator(uint8_t tok) {
    uint8_t precedence = operatorPrecedence[getIndexOfOperator(tok) - 1];
    element_t newElement = {0};
    
    if (tok == tStore) {
        EntireStackToOutput();
    }
    
    if (tok == tMul && canUseMask) {
        uint8_t mask = 0;
        
        while ((tok = _getc()) == tMul) {
            mask++;
        }
        if (mask > 3 || tok != tLBrace) {
            return E_SYNTAX;
        }
        
        newElement.needRelocate = false;
        newElement.allowStoreTo = true;
        newElement.type = TYPE_FUNCTION;
        newElement.operand.func.function = tLBrace;
        newElement.operand.func.mask = TYPE_MASK_U8 + mask;
        newElement.operand.func.amountOfArgs = 1;
    } else {
        while (stackElements) {
            element_t prevStackElement = stackPop();
            
            if (prevStackElement.type != TYPE_OPERATOR || precedence > prevStackElement.operand.op.precedence) {
                stackPush(prevStackElement);
                break;
            }
            
            outputStackPush(prevStackElement);
        }
        
        newElement.needRelocate = newElement.allowStoreTo = false;
        newElement.type = TYPE_OPERATOR;
        newElement.operand.op.type = tok;
        newElement.operand.op.precedence = precedence;
    }
    
    stackPush(newElement);
    
    inExpression = canUseMask = true;
    
    return VALID;
}

uint8_t ParseGetkey(uint8_t tok) {
    element_t newElement = {0};
    
    inExpression = true;
    canUseMask = false;
    
    newElement.needRelocate = newElement.allowStoreTo = false;
    newElement.type = TYPE_FUNCTION;
    newElement.operand.func.function = tGetKey;
    newElement.operand.func.mask = TYPE_MASK_U24;
    newElement.operand.func.amountOfArgs = 1;
    if (_getc() == tLParen) {
        stackPush(newElement);
    } else {
        SeekMinus1();
        newElement.operand.func.amountOfArgs = 0;
        outputStackPush(newElement);
    }
    
    return VALID;
}

uint8_t ParseDetSum(uint8_t tok) {
    element_t newElement = {0};
    
    newElement.allowStoreTo = false;
    newElement.type = TYPE_FUNCTION_START;
    outputStackPush(newElement);
    
    return ParseFunction(tok);
}

uint8_t ParseFunction(uint8_t tok) {
    uint8_t a, function2 = 0;
    element_t newElement = {0};
    
    inExpression = canUseMask = true;
    
    if (IsA2ByteTok(tok)) {
        function2 = _getc();
    }
    
    newElement.needRelocate = newElement.allowStoreTo = false;
    newElement.type = TYPE_FUNCTION;
    newElement.operand.func.function = tok;
    newElement.operand.func.function2 = function2;
    newElement.operand.func.mask = TYPE_MASK_U24;
    
    for (a = 0; a < AMOUNT_OF_FUNCTIONS; a++) {
        if (implementedFunctions[a][0] == tok && implementedFunctions[a][1] == function2) {
            if (implementedFunctions[a][2]) {
                newElement.operand.func.amountOfArgs = 1;
                stackPush(newElement);
            } else {
                newElement.operand.func.amountOfArgs = 0;
                outputStackPush(newElement);
            }
            
            return VALID;
        }
    }
    
    return E_UNIMPLEMENTED;
}

uint8_t ParseCloseFunction(uint8_t tok) {
    element_t prevElement = {0};
    
    while (stackElements) {
        prevElement = stackPop();
        if (prevElement.type == TYPE_FUNCTION) {
            break;
        }
        outputStackPush(prevElement);
    }
    
    if (!stackElements) {
        if (inFunction) {
            ParseExpression();
            returnToken = tok;
            
            return VALID;
        } else {
            return E_EXTRA_PAREN;
        }
    } else {
        uint8_t openingFunction = prevElement.operand.func.function;
        
        if ((tok == tRBrace && openingFunction != tLBrace) || 
            (tok == tRBrack && openingFunction != tLBrack) || 
            (tok == tRParen && (openingFunction == tLBrace || openingFunction == tLBrack))
        ) {
            return E_SYNTAX;
        }
        
        if (tok == tComma) {
            prevElement.operand.func.amountOfArgs++;
            canUseMask = true;
            stackPush(prevElement);
            if (openingFunction == tDet || openingFunction == tSum) {
                prevElement.type = TYPE_ARG_DELIMITER;
                outputStackPush(prevElement);
            }
        } else {
            canUseMask = false;
            outputStackPush(prevElement);
        }
    }
    
    return VALID;
}

uint8_t ParseCustomTokens(uint8_t tok) {
    return VALID;
}

uint8_t ParseBB(uint8_t tok) {
    return VALID;
}

uint8_t ParseUnimplemented(uint8_t tok) {
    return E_UNIMPLEMENTED;
}

uint8_t ParseNewLine(uint8_t tok) {
    // Expression after function without arguments
    if ((inExpression && !allowExpression) || (inFunction && !inExpression)) {
        return E_SYNTAX;
    }
    
    canUseMask = true;
    
    // Compile the expression
    if (inExpression) {
        ParseExpression();
        
        // Return to function if in function
        if (inFunction) {
            returnToken = tEnter;
            
            return VALID;
        }
    }
    
    // Reset some standard variables
    inExpression = inFunction = false;
    allowExpression = true;
    
    return VALID;
}

uint8_t ParseExpression(void) {
    int index;
    
    EntireStackToOutput();
    
    for (index = 1; index < outputElements; index++) {
        element_t outputPrevPrev = getOutputElement(index - 2);
        element_t outputPrev     = getOutputElement(index - 1);
        element_t outputCurr     = getOutputElement(index);
        
        // Check for number | number | operator
        if (index > 1) {
            if (outputPrevPrev.type == TYPE_NUMBER && outputPrev.type == TYPE_NUMBER && outputCurr.type == TYPE_OPERATOR && outputCurr.operand.op.type != tStore) {
                // Replace the value and remove the other + operator
                outputPrevPrev.operand.num = execOp(outputCurr.operand.op.type, outputPrevPrev.operand.num, outputPrev.operand.num);
                setOutputElement(outputPrevPrev, index - 2);
                removeOutputElement(index);
                removeOutputElement(index - 1);
                
                index -= 2;
                continue;
            }
        }
        
        // Check for number | number | ... | function
        if (outputCurr.type == TYPE_FUNCTION) {
            uint8_t amountOfArgs = outputCurr.operand.func.amountOfArgs, a;
            uint8_t function = outputCurr.operand.func.function;
            
            // Enough arguments
            if (amountOfArgs <= index) {
                for (a = 0; a < AMOUNT_OF_FUNCTIONS; a++) {
                    if (implementedFunctions[a][0] == function && implementedFunctions[a][1] == outputCurr.operand.func.function2) {
                        // Right amount of arguments
                        if (implementedFunctions[a][2] != -1 && implementedFunctions[a][2] != amountOfArgs) {
                            return E_ARGUMENTS;
                        }
                        
                        // Allowed to parse the function
                        if (implementedFunctions[a][3]) {
                            // Both arguments should be a number
                            if (outputPrev.type == TYPE_NUMBER && (amountOfArgs == 1 || outputPrevPrev.type == TYPE_NUMBER)) {
                                removeOutputElement(index);
                                
                                // Parse and store
                                if (amountOfArgs == 1) {
                                    outputPrev.operand.num = execFunc(function, 0, outputPrev.operand.num);
                                    index--;
                                } else {
                                    outputPrevPrev.operand.num = execFunc(function, outputPrevPrev.operand.num, outputPrev.operand.num);
                                    removeOutputElement(index - 1);
                                    index -= 2;
                                }
                                
                                continue;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // More stuff
    
    return VALID;
}

uint8_t ParseClrHome(uint8_t tok) {
    if (inExpression || inFunction) {
        return E_SYNTAX;
    }
    
    allowExpression = false;
    CALL(_HomeUp);
    CALL(_ClrLCDFull);
    
    return VALID;
}

uint8_t ParseI(uint8_t tok) {
    return VALID;
}

uint8_t ParsePrgm(uint8_t tok) {
    return VALID;
}

uint8_t ParseIf(uint8_t tok) {
    return VALID;
}

uint8_t ParseElse(uint8_t tok) {
    return VALID;
}

uint8_t ParseWhile(uint8_t tok) {
    return VALID;
}

uint8_t ParseRepeat(uint8_t tok) {
    return VALID;
}

uint8_t ParseFor(uint8_t tok) {
    return VALID;
}

uint8_t ParseEnd(uint8_t tok) {
    return VALID;
}

uint8_t ParseReturn(uint8_t tok) {
    return VALID;
}

uint8_t ParseLbl(uint8_t tok) {
    return VALID;
}

uint8_t ParseGoto(uint8_t tok) {
    return VALID;
}

uint8_t ParsePause(uint8_t tok) {
    return VALID;
}

uint8_t ParseInput(uint8_t tok) {
    return VALID;
}

uint8_t ParseDisp(uint8_t tok) {
    return VALID;
}

uint8_t ParseOutput(uint8_t tok) {
    return VALID;
}

void EntireStackToOutput(void) {
    while (stackElements) {
        outputStackPush(stackPop());
    }
}

void skipLine(void) {
}

uint8_t (*functions[256])(uint8_t) = {
    ParseUnimplemented, // 0x00
    ParseUnimplemented, // 0x01
    ParseUnimplemented, // 0x02
    ParseUnimplemented, // 0x03
    ParseOperator,      // 0x04
    ParseUnimplemented, // 0x05
    ParseFunction,      // 0x06
    ParseCloseFunction, // 0x07
    ParseFunction,      // 0x08
    ParseCloseFunction, // 0x09
    ParseUnimplemented, // 0x0A
    ParseDegree,        // 0x0B
    ParseUnimplemented, // 0x0C
    ParseUnimplemented, // 0x0D
    ParseUnimplemented, // 0x0E
    ParseUnimplemented, // 0x0F
    ParseFunction,      // 0x10
    ParseCloseFunction, // 0x11
    ParseUnimplemented, // 0x12
    ParseUnimplemented, // 0x13
    ParseUnimplemented, // 0x14
    ParseUnimplemented, // 0x15
    ParseUnimplemented, // 0x16
    ParseUnimplemented, // 0x17
    ParseUnimplemented, // 0x18
    ParseFunction,      // 0x19
    ParseFunction,      // 0x1A
    ParseUnimplemented, // 0x1B
    ParseUnimplemented, // 0x1C
    ParseUnimplemented, // 0x1D
    ParseUnimplemented, // 0x1E
    ParseUnimplemented, // 0x1F
    ParseUnimplemented, // 0x20
    ParseFunction,      // 0x21
    ParseUnimplemented, // 0x22
    ParseUnimplemented, // 0x23
    ParseUnimplemented, // 0x24
    ParseUnimplemented, // 0x25
    ParseUnimplemented, // 0x26
    ParseUnimplemented, // 0x27
    ParseUnimplemented, // 0x28
    ParseUnimplemented, // 0x29
    ParseString,        // 0x2A
    ParseUnimplemented, // 0x2B
    ParseI,             // 0x2C
    ParseUnimplemented, // 0x2D
    ParseUnimplemented, // 0x2E
    ParseUnimplemented, // 0x2F
    ParseNumber,        // 0x30
    ParseNumber,        // 0x31
    ParseNumber,        // 0x32
    ParseNumber,        // 0x33
    ParseNumber,        // 0x34
    ParseNumber,        // 0x35
    ParseNumber,        // 0x36
    ParseNumber,        // 0x37
    ParseNumber,        // 0x38
    ParseNumber,        // 0x39
    ParseNumber,        // 0x3A
    ParseEE,            // 0x3B
    ParseOperator,      // 0x3C
    ParseOperator,      // 0x3D
    ParseNewLine,       // 0x3E
    ParseNewLine,       // 0x3F
    ParseOperator,      // 0x40
    ParseVariable,      // 0x41
    ParseVariable,      // 0x42
    ParseVariable,      // 0x43
    ParseVariable,      // 0x44
    ParseVariable,      // 0x45
    ParseVariable,      // 0x46
    ParseVariable,      // 0x47
    ParseVariable,      // 0x48
    ParseVariable,      // 0x49
    ParseVariable,      // 0x4A
    ParseVariable,      // 0x4B
    ParseVariable,      // 0x4C
    ParseVariable,      // 0x4D
    ParseVariable,      // 0x4E
    ParseVariable,      // 0x4F
    ParseVariable,      // 0x50
    ParseVariable,      // 0x51
    ParseVariable,      // 0x52
    ParseVariable,      // 0x53
    ParseVariable,      // 0x54
    ParseVariable,      // 0x55
    ParseVariable,      // 0x56
    ParseVariable,      // 0x57
    ParseVariable,      // 0x58
    ParseVariable,      // 0x59
    ParseVariable,      // 0x5A
    ParseVariable,      // 0x5B
    ParseUnimplemented, // 0x5C
    ParseOSList,        // 0x5D
    ParseUnimplemented, // 0x5E
    ParsePrgm,          // 0x5F
    ParseUnimplemented, // 0x60
    ParseUnimplemented, // 0x61
    ParseCustomTokens,  // 0x62
    ParseUnimplemented, // 0x63
    ParseUnimplemented, // 0x64
    ParseUnimplemented, // 0x65
    ParseUnimplemented, // 0x66
    ParseUnimplemented, // 0x67
    ParseUnimplemented, // 0x68
    ParseUnimplemented, // 0x69
    ParseOperator,      // 0x6A
    ParseOperator,      // 0x6B
    ParseOperator,      // 0x6C
    ParseOperator,      // 0X6D
    ParseOperator,      // 0x6E
    ParseOperator,      // 0x6F
    ParseOperator,      // 0x70
    ParseOperator,      // 0x71
    ParseFunction,      // 0x72
    ParseUnimplemented, // 0x73
    ParseUnimplemented, // 0x74
    ParseUnimplemented, // 0x75
    ParseUnimplemented, // 0x76
    ParseUnimplemented, // 0x77
    ParseUnimplemented, // 0x78
    ParseUnimplemented, // 0x79
    ParseUnimplemented, // 0x7A
    ParseUnimplemented, // 0x7B
    ParseUnimplemented, // 0x7C
    ParseUnimplemented, // 0x7D
    ParseUnimplemented, // 0x7E
    ParseOperator,      // 0x7F
    ParseOperator,      // 0x80
    ParseOperator,      // 0x81
    ParseOperator,      // 0x82
    ParseOperator,      // 0x83
    ParseUnimplemented, // 0x84
    ParseUnimplemented, // 0x85
    ParseUnimplemented, // 0x86
    ParseUnimplemented, // 0x87
    ParseUnimplemented, // 0x88
    ParseUnimplemented, // 0x89
    ParseUnimplemented, // 0x8A
    ParseUnimplemented, // 0x8B
    ParseUnimplemented, // 0x8C
    ParseUnimplemented, // 0x8D
    ParseUnimplemented, // 0x8E
    ParseUnimplemented, // 0x8F
    ParseUnimplemented, // 0x90
    ParseUnimplemented, // 0x91
    ParseUnimplemented, // 0x92
    ParseUnimplemented, // 0x93
    ParseUnimplemented, // 0x94
    ParseUnimplemented, // 0x95
    ParseUnimplemented, // 0x96
    ParseUnimplemented, // 0x97
    ParseUnimplemented, // 0x98
    ParseUnimplemented, // 0x99
    ParseUnimplemented, // 0x9A
    ParseUnimplemented, // 0x9B
    ParseUnimplemented, // 0x9C
    ParseUnimplemented, // 0x9D
    ParseUnimplemented, // 0x9E
    ParseUnimplemented, // 0x9F
    ParseUnimplemented, // 0xA0
    ParseUnimplemented, // 0xA1
    ParseUnimplemented, // 0xA2
    ParseUnimplemented, // 0xA3
    ParseUnimplemented, // 0xA4
    ParseUnimplemented, // 0xA5
    ParseUnimplemented, // 0xA6
    ParseUnimplemented, // 0xA7
    ParseUnimplemented, // 0xA8
    ParseUnimplemented, // 0xA9
    ParseOSString,      // 0xAA
    ParseFunction,      // 0xAB
    ParsePi,            // 0xAC
    ParseGetkey,        // 0xAD
    ParseUnimplemented, // 0xAE
    ParseUnimplemented, // 0xAF
    ParseChs,           // 0xB0
    ParseUnimplemented, // 0xB1
    ParseUnimplemented, // 0xB2
    ParseDetSum,        // 0xB3
    ParseUnimplemented, // 0xB4
    ParseUnimplemented, // 0xB5
    ParseDetSum,        // 0xB6
    ParseUnimplemented, // 0xB7
    ParseFunction,      // 0xB8
    ParseUnimplemented, // 0xB9
    ParseUnimplemented, // 0xBA
    ParseBB,            // 0xBB
    ParseFunction,      // 0xBC
    ParseUnimplemented, // 0xBD
    ParseUnimplemented, // 0xBE
    ParseUnimplemented, // 0xBF
    ParseUnimplemented, // 0xC0
    ParseUnimplemented, // 0xC1
    ParseFunction,      // 0xC2
    ParseUnimplemented, // 0xC3
    ParseFunction,      // 0xC4
    ParseUnimplemented, // 0xC5
    ParseUnimplemented, // 0xC6
    ParseUnimplemented, // 0xC7
    ParseUnimplemented, // 0xC8
    ParseUnimplemented, // 0xC9
    ParseUnimplemented, // 0xCA
    ParseUnimplemented, // 0xCB
    ParseUnimplemented, // 0xCC
    ParseUnimplemented, // 0xCD
    ParseIf,            // 0xCE
    ParseUnimplemented, // 0xCF
    ParseElse,          // 0xD0
    ParseWhile,         // 0xD1
    ParseRepeat,        // 0xD2
    ParseFor,           // 0xD3
    ParseEnd,           // 0xD4
    ParseReturn,        // 0xD5
    ParseLbl,           // 0xD6
    ParseGoto,          // 0xD7
    ParsePause,         // 0xD8
    ParseUnimplemented, // 0xD9
    ParseUnimplemented, // 0xDA
    ParseUnimplemented, // 0xDB
    ParseInput,         // 0xDC
    ParseUnimplemented, // 0xDD
    ParseDisp,          // 0xDE
    ParseUnimplemented, // 0xDF
    ParseOutput,        // 0xE0
    ParseClrHome,       // 0xE1
    ParseUnimplemented, // 0xE2
    ParseUnimplemented, // 0xE3
    ParseUnimplemented, // 0xE4
    ParseUnimplemented, // 0xE5
    ParseUnimplemented, // 0xE6
    ParseUnimplemented, // 0xE7
    ParseUnimplemented, // 0xE8
    ParseUnimplemented, // 0xE9
    ParseUnimplemented, // 0xEA
    ParseUnimplemented, // 0xEB
    ParseUnimplemented, // 0xEC
    ParseUnimplemented, // 0xED
    ParseUnimplemented, // 0xEE
    ParseFunction,      // 0xEF
    ParseUnimplemented, // 0xF0
    ParseUnimplemented, // 0xF1
    ParseUnimplemented, // 0xF2
    ParseUnimplemented, // 0xF3
    ParseUnimplemented, // 0xF4
    ParseUnimplemented, // 0xF5
    ParseUnimplemented, // 0xF6
    ParseUnimplemented, // 0xF7
    ParseUnimplemented, // 0xF8
    ParseUnimplemented, // 0xF9
    ParseUnimplemented, // 0xFA
    ParseUnimplemented, // 0xFB
    ParseUnimplemented, // 0xFC
    ParseUnimplemented, // 0xFD
    ParseUnimplemented, // 0xFE
    ParseUnimplemented  // 0xFF
};
