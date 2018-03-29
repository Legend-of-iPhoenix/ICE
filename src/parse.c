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

extern uint8_t (*functions[256])(uint8_t tok);
const uint8_t implementedFunctions[AMOUNT_OF_FUNCTIONS][4] = {
// function / second byte / amount of arguments / allow arguments as numbers
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
    element_t newElement;
    uint8_t a = 1, amountOfPts = 0;
    char input[100]= {0};
    
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
    uint24_t output = tok;
    element_t newElement;
    
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
    uint24_t output = tok;
    element_t newElement;
    
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
        element_t newElement;
        
        // Push to the stack
        newElement.needRelocate = newElement.allowStoreTo = false;
        newElement.type = TYPE_NUMBER;
        newElement.operand.num = -1;
        outputStackPush(newElement);
        
        return ParseOperator(tMul);
    }
}

uint8_t ParseDegree(uint8_t tok) {
    element_t newElement;
    
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
    element_t newElement;
    
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
}

uint8_t ParseOSString(uint8_t tok) {
    element_t newElement;
    
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
    element_t newElement;
    
    inExpression = true;
    canUseMask = false;
    
    newElement.needRelocate = true;
    newElement.allowStoreTo = false;
    newElement.type = TYPE_NUMBER;
}

uint8_t ParseVariable(uint8_t tok) {
    element_t newElement;
    
    inExpression = true;
    canUseMask = false;
    
    // Push to the stack
    newElement.needRelocate = false;
    newElement.allowStoreTo = true;
    newElement.type = TYPE_VARIABLE;
    newElement.operand.variable.variable = GetVariableOffset(tok);
    outputStackPush(newElement);
    
    return VALID;
}

uint8_t ParseOperator(uint8_t tok) {
    uint8_t precendence = operatorPrecedence[getIndexOfOperator(tok) - 1];
    element_t newElement;
    
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
        newElement.operand.function.function = tLBrace;
        newElement.operand.function.mask = TYPE_MASK_U8 + mask;
        newElement.operand.function.amountOfArgs = 1;
    } else {
        while (stackElements) {
            element_t prevStackElement = stackPop();
            
            if (prevStackElement.type != TYPE_OPERATOR || precendence > prevStackElement.operand.op.precedence) {
                stackPush(prevStackElement);
                break;
            }
            
            outputStackPush(prevStackElement);
        }
        
        newElement.needRelocate = newElement.allowStoreTo = false;
        newElement.type = TYPE_OPERATOR;
        newElement.operand.op.op = tok;
        newElement.operand.op.precedence = precedence;
    }
    
    stackPush(newElement);
    
    inExpression = canUseMask = true;
    
    return VALID;
}

uint8_t ParseGetkey(uint8_t tok) {
    element_t newElement;
    
    inExpression = true;
    canUseMask = false;
    
    newElement.needRelocate = newElement.allowStoreTo = false;
    newElement.type = TYPE_FUNCTION;
    newElement.operand.function.function = tGetKey;
    newElement.operand.function.mask = TYPE_MASK_U24;
    newElement.operand.function.amountOfArgs = 1;
    if (_getc() == tLParen) {
        stackPush(newElement);
    } else {
        SeekMinus1();
        newElement.operand.function.amountOfArgs = 0;
        outputStackPush(newElement);
    }
    
    return VALID;
}

uint8_t ParseDetSum(uint8_t tok) {
    element_t newElement;
    
    newElement.allowStoreTo = false;
    newElement.type = TYPE_FUNCTION_START;
    outputStackPush(newElement);
    
    return ParseFunction(tok);
}

uint8_t ParseFunction(uint8_t tok) {
    uint8_t a, function2 = 0;
    element_t newElement;
    
    inExpression = canUseMask = true;
    
    if (IsA2ByteTok(tok)) {
        function2 = _getc();
    }
    
    newElement.needRelocate = newElement.allowStoreTo = false;
    newElement.type = TYPE_FUNCTION;
    newElement.operand.function.function = tok;
    newElement.operand.function.function2 = function2;
    newElement.operand.function.mask = TYPE_MASK_U24;
    
    for (a = 0; a < AMOUNT_OF_FUNCTIONS; a++) {
        if (implementedFunctions[a][0] == tok && implementedFunctions[a][1] == function2) {
            if (implementedFunctions[a][2]) {
                newElement.operand.function.amountOfArgs = 1;
                stackPush(newElement);
            } else {
                newElement.operand.function.amountOfArgs = 0;
                outputStackPush(newElement);
            }
            
            return VALID;
        }
    }
    
    return E_UNIMPLEMENTED;
}

uint8_t ParseCloseFunction(uint8_t tok) {
    element_t prevElement;
    
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
        uint8_t openingFunction = prevElement.operand.function.function;
        
        if ((tok == tRBrace && openingFunction != tLBrace) || 
            (tok == tRBrack && openingFunction != tLBrack) || 
            (tok == tRParen && (openingFunction == tLBrace || openingFunction == tLBrack))
        ) {
            return E_SYNTAX;
        }
        
        if (tok == tComma) {
            prevElement.operand.function.amountOfArgs++;
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
}

uint8_t ParseCustomTokens(uint8_t tok) {
}

uint8_t ParseBB(uint8_t tok) {
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
    EntireStackToOutput();
    
    // More stuff
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

void EntireStackToOutput(void) {
    while (stackElements) {
        outputStackPush(stackPop());
    }
}



















































































uint8_t parseExpression(int token) {
    uint24_t stackElements = 0, outputElements = 0;
    uint24_t loopIndex, temp;
    uint8_t amountOfArgumentsStack[20];
    uint8_t index = 0, a, stackToOutputReturn, mask = TYPE_MASK_U24, tok, storeDepth = 0;
    uint8_t *amountOfArgumentsStackPtr = amountOfArgumentsStack, canUseMask = 2, prevTokenWasDetOrSum = 0;

    // Setup pointers
    element_t *outputPtr = outputStack;
    element_t *stackPtr  = stack;
    element_t *outputCurr, *outputPrev, *outputPrevPrev;
    element_t *stackCurr, *stackPrev = NULL;


        // Parse a string of tokens
        else if (tok == tAPost) {
            uint8_t *tempProgramPtr = ice.programPtr;
            uint24_t length;

            outputCurr->isString = 1;
            outputCurr->type = TYPE_STRING;
            outputElements++;
            mask = TYPE_MASK_U24;

            while ((token = _getc()) != EOF && (uint8_t)token != tEnter && (uint8_t)token != tStore && (uint8_t)token != tAPost) {
                *ice.programPtr++ = token;

                if (IsA2ByteTok(token)) {
                    *ice.programPtr++ = _getc();
                }
            }

            *ice.programPtr++ = 0;

            length = ice.programPtr - tempProgramPtr;
            ice.programDataPtr -= length;
            memcpy(ice.programDataPtr, tempProgramPtr, length);
            ice.programPtr = tempProgramPtr;

            outputCurr->operand = (uint24_t)ice.programDataPtr;

            if ((uint8_t)token == tStore || (uint8_t)token == tEnter) {
                continue;
            }
        }

        // Parse a string of characters
        else if (tok == tString) {
            uint24_t length;
            uint8_t *tempDataPtr = ice.programPtr, *a;
            uint8_t amountOfHexadecimals = 0;
            bool needWarning = true;

            outputCurr->isString = 1;
            outputCurr->type = TYPE_STRING;
            outputElements++;
            mask = TYPE_MASK_U24;
            stackPrev = &stackPtr[stackElements-1];

            token = grabString(&ice.programPtr, true, true);
            if ((uint8_t)stackPrev->operand == tVarOut && (uint8_t)(stackPrev->operand >> 16) == tDefineSprite) {
                needWarning = false;
            }

            for (a = tempDataPtr; a < ice.programPtr; a++) {
                if (IsHexadecimal(*a) == 16) {
                    goto noSquishing;
                }
                amountOfHexadecimals++;
            }
            if (!(amountOfHexadecimals & 1)) {
                uint8_t *prevDataPtr = tempDataPtr;
                uint8_t *prevDataPtr2 = tempDataPtr;

                while (prevDataPtr != ice.programPtr) {
                    uint8_t tok1 = IsHexadecimal(*prevDataPtr++);
                    uint8_t tok2 = IsHexadecimal(*prevDataPtr++);

                    *prevDataPtr2++ = (tok1 << 4) + tok2;
                }

                ice.programPtr = prevDataPtr2;

                if (needWarning) {
                    displayError(W_SQUISHED);
                }
            }

noSquishing:
            *ice.programPtr++ = 0;

            length = ice.programPtr - tempDataPtr;
            ice.programDataPtr -= length;
            memcpy(ice.programDataPtr, tempDataPtr, length);
            ice.programPtr = tempDataPtr;

            outputCurr->operand = (uint24_t)ice.programDataPtr;

            if ((uint8_t)token == tStore || (uint8_t)token == tEnter) {
                continue;
            }
        }
    }

    // If the expression quits normally, rather than an argument seperator
    ice.tempToken = tEnter;

stopParsing:
    // Move entire stack to output
    stackToOutputReturn = 2;
    goto stackToOutput;
stackToOutputReturn2:

    // Remove stupid things like 2+5, and not(1, max(2,3
    for (loopIndex = 1; loopIndex < outputElements; loopIndex++) {
        outputPrevPrev = &outputPtr[loopIndex - 2];
        outputPrev = &outputPtr[loopIndex - 1];
        outputCurr = &outputPtr[loopIndex];
        index = outputCurr->operand >> 8;

        // Check if the types are number | number | operator
        if (loopIndex > 1 && outputPrevPrev->type == TYPE_NUMBER && outputPrev->type == TYPE_NUMBER &&
               outputCurr->type == TYPE_OPERATOR && (uint8_t)outputCurr->operand != tStore) {
            // If yes, execute the operator, and store it in the first entry, and remove the other 2
            outputPrevPrev->operand = executeOperator(outputPrevPrev->operand, outputPrev->operand, (uint8_t)outputCurr->operand);
            memcpy(outputPrev, &outputPtr[loopIndex + 1], (outputElements - 1) * sizeof(element_t));
            outputElements -= 2;
            loopIndex -= 2;
            continue;
        }

        // Check if the types are number | number | ... | function (specific function or pointer)
        if (loopIndex >= index && outputCurr->type == TYPE_FUNCTION) {
            uint8_t a, function = (uint8_t)outputCurr->operand, function2 = (uint8_t)(outputCurr->operand >> 16);

            for (a = 0; a < AMOUNT_OF_FUNCTIONS; a++) {
                if (function == implementedFunctions[a][0] && function2 == implementedFunctions[a][1] && implementedFunctions[a][3]) {
                    uint24_t outputPrevOperand = outputPrev->operand, outputPrevPrevOperand = outputPrevPrev->operand;

                    for (a = 1; a <= index; a++) {
                        if ((&outputPtr[loopIndex-a])->type != TYPE_NUMBER) {
                            goto DontDeleteFunction;
                        }
                    }

                    switch (function) {
                        case tNot:
                            temp = !outputPrevOperand;
                            break;
                        case tMin:
                            temp = (outputPrevOperand < outputPrevPrevOperand) ? outputPrevOperand : outputPrevPrevOperand;
                            break;
                        case tMax:
                            temp = (outputPrevOperand > outputPrevPrevOperand) ? outputPrevOperand : outputPrevPrevOperand;
                            break;
                        case tMean:
                            // I can't simply add, and divide by 2, because then it *might* overflow in case that A + B > 0xFFFFFF
                            temp = ((long)outputPrevOperand + (long)outputPrevPrevOperand) / 2;
                            break;
                        case tSqrt:
                            temp = sqrt(outputPrevOperand);
                            break;
                        case tExtTok:
                            if ((uint8_t)(outputCurr->operand >> 16) != tRemainder) {
                                return E_ICE_ERROR;
                            }
                            temp = outputPrevOperand % outputPrevPrevOperand;
                            break;
                        case tSin:
                            temp = 255*sin((double)outputPrevOperand * (2 * M_PI / 256));
                            break;
                        case tCos:
                            temp = 255*cos((double)outputPrevOperand * (2 * M_PI / 256));
                            break;
                        default:
                            return E_ICE_ERROR;
                    }

                    // And remove everything
                    (&outputPtr[loopIndex - index])->operand = temp;
                    memcpy(&outputPtr[loopIndex - index + 1], &outputPtr[loopIndex + 1], (outputElements - 1) * sizeof(element_t));
                    outputElements -= index;
                    loopIndex -= index - 1;
                }
            }
        }
DontDeleteFunction:;
    }

    // Check if the expression is valid
    if (!outputElements) {
        return E_SYNTAX;
    }

    return parsePostFixFromIndexToIndex(0, outputElements - 1);

    // Duplicated function opt
stackToOutput:
    // Move entire stack to output
    while (stackElements) {
        outputCurr = &outputPtr[outputElements++];
        stackPrev = &stackPtr[--stackElements];

        temp = stackPrev->operand;
        if ((uint8_t)temp == 0x0F) {
            // :D
            temp = (temp & 0xFF0000) + tLBrace;
        }

        // If it's a function, add the amount of arguments as well
        if (stackPrev->type == TYPE_FUNCTION) {
            temp += (*amountOfArgumentsStackPtr--) << 8;
        }

        // Don't move the left paren...
        if (stackPrev->type == TYPE_FUNCTION && (uint8_t)stackPrev->operand == tLParen) {
            outputElements--;
            continue;
        }

        outputCurr->type = stackPrev->type;
        outputCurr->mask = stackPrev->mask;
        outputCurr->operand = temp;
    }

    // Select correct return location
    if (stackToOutputReturn == 2) {
        goto stackToOutputReturn2;
    }

    goto stackToOutputReturn1;
}

uint8_t parsePostFixFromIndexToIndex(uint24_t startIndex, uint24_t endIndex) {
    element_t *outputCurr;
    element_t *outputPtr = (element_t*)outputStack;
    uint8_t outputType, temp, AnsDepth = 0;
    uint24_t outputOperand, loopIndex, tempIndex = 0, amountOfStackElements;

    // Set some variables
    outputCurr = &outputPtr[startIndex];
    outputType = outputCurr->type;
    outputOperand = outputCurr->operand;
    ice.stackStart = (uint24_t*)(ice.stackDepth * STACK_SIZE + ice.stack);
    setStackValues(ice.stackStart, ice.stackStart);
    reg.allowedToOptimize = true;

    // Clean the expr struct
    memset(&expr, 0, sizeof expr);

    // Get all the indexes of the expression
    temp = 0;
    amountOfStackElements = 0;
    for (loopIndex = startIndex; loopIndex <= endIndex; loopIndex++) {
        outputCurr = &outputPtr[loopIndex];

        // If it's the start of a det( or sum(, increment the amount of nested det(/sum(
        if (outputCurr->type == TYPE_C_START) {
            temp++;
        }
        // If it's a det( or sum(, decrement the amount of nested dets
        if (outputCurr->type == TYPE_FUNCTION && ((uint8_t)outputCurr->operand == tDet || (uint8_t)outputCurr->operand == tSum)) {
            temp--;
            amountOfStackElements++;
        }

        // If not in a nested det( or sum(, push the index
        if (!temp) {
            push(loopIndex);
            amountOfStackElements++;
        }
    }

    // Empty argument
    if (!amountOfStackElements) {
        return E_SYNTAX;
    }

    // It's a single entry
    if (amountOfStackElements == 1) {
        // Expression is a string
        if (outputCurr->isString) {
            expr.outputIsString = true;
            LD_HL_STRING(outputOperand, outputType);
        }
        
        // Expression is only a single number
         else if (outputType == TYPE_NUMBER) {
            // This boolean is set, because loops may be optimized when the condition is a number
            expr.outputIsNumber = true;
            expr.outputNumber = outputOperand;
            LD_HL_IMM(outputOperand);
        }

        // Expression is only a variable
        else if (outputType == TYPE_VARIABLE) {
            expr.outputIsVariable = true;
            output(uint16_t, 0x27DD);
            output(uint8_t, outputOperand);
            reg.HLIsNumber = false;
            reg.HLIsVariable = true;
            reg.HLVariable = outputOperand;
        }

        // Expression is an empty function or operator, i.e. not(, +
        else {
            return E_SYNTAX;
        }

        return VALID;
    }

    // 3 or more entries, full expression
    do {
        element_t *outputPrevPrevPrev;

        outputCurr = &outputPtr[loopIndex = getNextIndex()];
        outputPrevPrevPrev = &outputPtr[getIndexOffset(-4)];
        outputType = outputCurr->type;

        // Set some vars
        expr.outputReturnRegister = REGISTER_HL;
        expr.outputIsString = false;

        if (outputType == TYPE_OPERATOR) {
            element_t *outputPrev, *outputPrevPrev, *outputNext, *outputNextNext;
            bool canOptimizeConcatenateStrings;

            // Wait, invalid operator?!
            if (loopIndex < startIndex + 2) {
                return E_SYNTAX;
            }

            if (AnsDepth > 3 && (uint8_t)outputCurr->operand != tStore) {
                // We need to push HL since it isn't used in the next operator/function
                (&outputPtr[tempIndex])->type = TYPE_CHAIN_PUSH;
                PushHLDE();
                expr.outputRegister = REGISTER_HL;
            }

            // Get the previous entries, -2 is the previous one, -3 is the one before etc
            outputPrev     = &outputPtr[getIndexOffset(-2)];
            outputPrevPrev = &outputPtr[getIndexOffset(-3)];
            outputNext     = &outputPtr[getIndexOffset(0)];
            outputNextNext = &outputPtr[getIndexOffset(1)];

            // Check if we can optimize StrX + "..." -> StrX
            canOptimizeConcatenateStrings = (
                (uint8_t)(outputCurr->operand) == tAdd &&
                outputPrevPrev->isString && outputPrevPrev->type == TYPE_NUMBER &&
                outputNext->isString && outputNext->type == TYPE_NUMBER &&
                outputNext->operand == outputPrevPrev->operand &&
                outputNextNext->type == TYPE_OPERATOR &&
                (uint8_t)(outputNextNext->operand) == tStore
            );

            // Parse the operator with the 2 latest operands of the stack!
            if ((temp = parseOperator(outputPrevPrevPrev, outputPrevPrev, outputPrev, outputCurr, canOptimizeConcatenateStrings)) != VALID) {
                return temp;
            }

            // Remove the index of the first and the second argument, the index of the operator will be the chain
            removeIndexFromStack(getCurrentIndex() - 2);
            removeIndexFromStack(getCurrentIndex() - 2);
            AnsDepth = 0;

            // Eventually remove the ->StrX too
            if (canOptimizeConcatenateStrings) {
                loopIndex = getIndexOffset(1);
                removeIndexFromStack(getCurrentIndex());
                removeIndexFromStack(getCurrentIndex() + 1);

                outputCurr->isString = 1;
                outputCurr->type = TYPE_NUMBER;
                outputCurr->operand = outputPrevPrev->operand;
                expr.outputIsString = true;
            } else {
                // Check if it was a command with 2 strings, then the output is a string, not Ans
                if ((uint8_t)outputCurr->operand == tAdd && outputPrevPrev->isString && outputPrev->isString) {
                    outputCurr->isString = 1;
                    outputCurr->type = TYPE_STRING;
                    if (outputPrevPrev->operand == prescan.tempStrings[TempString2] || outputPrev->operand == prescan.tempStrings[TempString1]) {
                        outputCurr->operand = prescan.tempStrings[TempString2];
                    } else {
                        outputCurr->operand = prescan.tempStrings[TempString1];
                    }
                    expr.outputIsString = true;
                } else {
                    AnsDepth = 1;
                    outputCurr->type = TYPE_CHAIN_ANS;
                }
                tempIndex = loopIndex;
            }
        }

        else if (outputType == TYPE_FUNCTION) {
            // Use this to cleanup the function after parsing
            uint8_t amountOfArguments = outputCurr->operand >> 8;
            uint8_t function2 = outputCurr->operand >> 16;

            // Only execute when it's not a pointer directly after a ->
            if (outputCurr->operand != 0x010108) {
                // Check if we need to push Ans
                if (AnsDepth > 1 + amountOfArguments || (AnsDepth && ((uint8_t)outputCurr->operand == tDet || (uint8_t)outputCurr->operand == tSum))) {
                    // We need to push HL since it isn't used in the next operator/function
                    (&outputPtr[tempIndex])->type = TYPE_CHAIN_PUSH;
                    PushHLDE();
                    expr.outputRegister = REGISTER_HL;
                }

                for (temp = 0; temp < AMOUNT_OF_FUNCTIONS; temp++) {
                    if (implementedFunctions[temp][0] == (uint8_t)outputCurr->operand && implementedFunctions[temp][1] == function2) {
                        if (amountOfArguments != implementedFunctions[temp][2] && implementedFunctions[temp][2] != 255) {
                            return E_ARGUMENTS;
                        }
                    }
                }

                if ((temp = parseFunction(loopIndex)) != VALID) {
                    return temp;
                }

                // Cleanup, if it's not a det(
                if ((uint8_t)outputCurr->operand != tDet && (uint8_t)outputCurr->operand != tSum) {
                    for (temp = 0; temp < amountOfArguments; temp++) {
                        removeIndexFromStack(getCurrentIndex() - 2);
                    }
                }

                // I don't care that this will be ignored when it's a pointer, because I know there is a -> directly after
                // If it's a sub(, the output should be a string, not Ans
                if ((uint8_t)outputCurr->operand == t2ByteTok && function2 == tSubStrng) {
                    outputCurr->isString = 1;
                    outputCurr->type = TYPE_STRING;
                    if (outputPrevPrevPrev->operand == prescan.tempStrings[TempString1]) {
                        outputCurr->operand = prescan.tempStrings[TempString2];
                    } else {
                        outputCurr->operand = prescan.tempStrings[TempString1];
                    }
                    expr.outputIsString = true;
                    AnsDepth = 0;
                }

                // Check chain push/ans
                else {
                    AnsDepth = 1;
                    tempIndex = loopIndex;
                    outputCurr->type = TYPE_CHAIN_ANS;
                }
            }
        }

        if (AnsDepth) {
            AnsDepth++;
        }
    } while (loopIndex != endIndex);

    return VALID;
}

static uint8_t functionI(int token) {
    skipLine();

    return VALID;
}

static uint8_t functionIf(int token) {
    uint8_t *IfElseAddr = NULL;
    uint8_t tempGotoElements = prescan.amountOfGotos;
    uint8_t tempLblElements = prescan.amountOfLbls;

    if ((token = _getc()) != EOF && token != tEnter) {
        uint8_t *IfStartAddr, res;
        uint24_t tempDataOffsetElements;

        // Parse the argument
        if ((res = parseExpression(token)) != VALID) {
            return res;
        }

        if (expr.outputIsString) {
            return E_SYNTAX;
        }

        //Check if we can optimize stuff :D
        optimizeZeroCarryFlagOutput();

        // Backup stuff
        IfStartAddr = ice.programPtr;
        tempDataOffsetElements = ice.dataOffsetElements;

        if (expr.AnsSetCarryFlag || expr.AnsSetCarryFlagReversed) {
            if (expr.AnsSetCarryFlagReversed) {
                JP_NC(0);
            } else {
                JP_C(0);
            }
        } else {
            if (expr.AnsSetZeroFlagReversed) {
                JP_NZ(0);
            } else {
                JP_Z(0);
            }
        }
        res = parseProgram();

        // Check if we quit the program with an 'Else'
        if (res == E_ELSE) {
            bool shortElseCode;
            uint8_t tempGotoElements2 = prescan.amountOfGotos;
            uint8_t tempLblElements2 = prescan.amountOfLbls;
            uint24_t tempDataOffsetElements2;;

            // Backup stuff
            ResetAllRegs();
            IfElseAddr = ice.programPtr;
            tempDataOffsetElements2 = ice.dataOffsetElements;

            JP(0);
            if ((res = parseProgram()) != E_END && res != VALID) {
                return res;
            }

            shortElseCode = JumpForward(IfElseAddr, ice.programPtr, tempDataOffsetElements2, tempGotoElements2, tempLblElements2);
            JumpForward(IfStartAddr, IfElseAddr + (shortElseCode ? 2 : 4), tempDataOffsetElements, tempGotoElements, tempLblElements);
        }

        // Check if we quit the program with an 'End' or at the end of the input program
        else if (res == E_END || res == VALID) {
            JumpForward(IfStartAddr, ice.programPtr, tempDataOffsetElements, tempGotoElements, tempLblElements);
        } else {
            return res;
        }

        ResetAllRegs();

        return VALID;
    } else {
        return E_NO_CONDITION;
    }
}

static uint8_t functionElse(int token) {
    if (!CheckEOL()) {
        return E_SYNTAX;
    }
    return E_ELSE;
}

static uint8_t functionEnd(int token) {
    if (!CheckEOL()) {
        return E_SYNTAX;
    }
    return E_END;
}

static uint8_t dummyReturn(int token) {
    return VALID;
}

uint8_t JumpForward(uint8_t *startAddr, uint8_t *endAddr, uint24_t tempDataOffsetElements, uint8_t tempGotoElements, uint8_t tempLblElements) {
    if (endAddr - startAddr <= 0x80) {
        uint8_t *tempPtr = startAddr;
        uint8_t opcode = *startAddr;
        uint24_t tempForLoopSMCElements = ice.ForLoopSMCElements;
        label_t *labelPtr = ice.LblStack;
        label_t *gotoPtr = ice.GotoStack;

        *startAddr++ = opcode - 0xA2 - (opcode == 0xC3 ? 9 : 0);
        *startAddr++ = endAddr - tempPtr - 4;

        // Update pointers to data, decrease them all with 2, except pointers from data to data!
        while (ice.dataOffsetElements != tempDataOffsetElements) {
            uint8_t *tempDataOffsetStackAddr = (uint8_t*)ice.dataOffsetStack[tempDataOffsetElements];

            // Check if the pointer is in the program, not in the program data
            if (tempDataOffsetStackAddr >= ice.programData && tempDataOffsetStackAddr <= ice.programPtr) {
                ice.dataOffsetStack[tempDataOffsetElements] = (uint24_t*)(tempDataOffsetStackAddr - 2);
            }
            tempDataOffsetElements++;
        }

        // Update Goto and Lbl addresses, decrease them all with 2
        while (prescan.amountOfGotos != tempGotoElements) {
            (&gotoPtr[tempGotoElements])->addr -= 2;
            tempGotoElements++;
        }
        while (prescan.amountOfLbls != tempLblElements) {
            (&labelPtr[tempLblElements])->addr -= 2;
            tempLblElements++;
        }

        // Update all the For loop SMC addresses
        while (tempForLoopSMCElements--) {
            if ((uint24_t)ice.ForLoopSMCStack[tempForLoopSMCElements] > (uint24_t)startAddr) {
                *ice.ForLoopSMCStack[tempForLoopSMCElements] -= 2;
                ice.ForLoopSMCStack[tempForLoopSMCElements] = (uint24_t*)(((uint8_t*)ice.ForLoopSMCStack[tempForLoopSMCElements]) - 2);
            }
        }

        if (ice.programPtr != startAddr) {
            memcpy(startAddr, startAddr + 2, ice.programPtr - startAddr);
        }
        ice.programPtr -= 2;
        return true;
    } else {
        w24(startAddr + 1, endAddr - ice.programData + PRGM_START);
        return false;
    }
}

uint8_t JumpBackwards(uint8_t *startAddr, uint8_t whichOpcode) {
    if (ice.programPtr + 2 - startAddr <= 0x80) {
        uint8_t *tempPtr = ice.programPtr;

        *ice.programPtr++ = whichOpcode;
        *ice.programPtr++ = startAddr - 2 - tempPtr;

        return true;
    } else {
        // JR cc to JP cc
        *ice.programPtr++ = whichOpcode + 0xA2 + (whichOpcode == 0x18 ? 9 : 0);
        output(uint24_t, startAddr - ice.programData + PRGM_START);

        return false;
    }
}

uint8_t *WhileRepeatCondStart = NULL;
bool WhileJumpBackwardsLarge;

static uint8_t functionWhile(int token) {
    uint24_t tempDataOffsetElements = ice.dataOffsetElements;
    uint8_t tempGotoElements = prescan.amountOfGotos;
    uint8_t tempLblElements = prescan.amountOfLbls;
    uint8_t *WhileStartAddr = ice.programPtr, res;
    uint8_t *WhileRepeatCondStartTemp = WhileRepeatCondStart;
    bool WhileJumpForwardSmall;

    // Basically the same as "Repeat", but jump to condition checking first
    JP(0);
    if ((res = functionRepeat(token)) != VALID) {
        return res;
    }
    WhileJumpForwardSmall = JumpForward(WhileStartAddr, WhileRepeatCondStart, tempDataOffsetElements, tempGotoElements, tempLblElements);
    WhileRepeatCondStart = WhileRepeatCondStartTemp;

    if (WhileJumpForwardSmall && WhileJumpBackwardsLarge) {
        // Now the JP at the condition points to the 2nd byte after the JR to the condition, so update that too
        w24(ice.programPtr - 3, r24(ice.programPtr - 3) - 2);
    }

    return VALID;
}

uint8_t functionRepeat(int token) {
    uint24_t tempCurrentLine, tempCurrentLine2;
    uint16_t RepeatCondStart, RepeatProgEnd;
    uint8_t *RepeatCodeStart, res;

    RepeatCondStart = _tell(ice.inPrgm);
    RepeatCodeStart = ice.programPtr;
    tempCurrentLine = ice.currentLine;

    // Skip the condition for now
    skipLine();
    ResetAllRegs();

    // Parse the code inside the loop
    if ((res = parseProgram()) != E_END && res != VALID) {
        return res;
    }

    ResetAllRegs();

    // Remind where the "End" is
    RepeatProgEnd = _tell(ice.inPrgm);
    if (token == tWhile) {
        WhileRepeatCondStart = ice.programPtr;
    }

    // Parse the condition
    _seek(RepeatCondStart, SEEK_SET, ice.inPrgm);
    tempCurrentLine2 = ice.currentLine;
    ice.currentLine = tempCurrentLine;

    if ((res = parseExpression(_getc())) != VALID) {
        return res;
    }
    ice.currentLine = tempCurrentLine2;

    // And set the pointer after the "End"
    _seek(RepeatProgEnd, SEEK_SET, ice.inPrgm);

    if (expr.outputIsNumber) {
        ice.programPtr -= expr.SizeOfOutputNumber;
        if ((expr.outputNumber && (uint8_t)token == tWhile) || (!expr.outputNumber && (uint8_t)token == tRepeat)) {
            WhileJumpBackwardsLarge = !JumpBackwards(RepeatCodeStart, OP_JR);
        }
        return VALID;
    }

    if (expr.outputIsString) {
        return E_SYNTAX;
    }

    optimizeZeroCarryFlagOutput();

    if ((uint8_t)token == tWhile) {
        // Switch the flags
        bool a = expr.AnsSetZeroFlag;

        expr.AnsSetZeroFlag = expr.AnsSetZeroFlagReversed;
        expr.AnsSetZeroFlagReversed = a;
        a = expr.AnsSetCarryFlag;
        expr.AnsSetCarryFlag = expr.AnsSetCarryFlagReversed;
        expr.AnsSetCarryFlagReversed = a;
    }

    WhileJumpBackwardsLarge = !JumpBackwards(RepeatCodeStart, expr.AnsSetCarryFlag || expr.AnsSetCarryFlagReversed ?
        (expr.AnsSetCarryFlagReversed ? OP_JR_NC : OP_JR_C) :
        (expr.AnsSetZeroFlagReversed  ? OP_JR_NZ : OP_JR_Z));
    return VALID;
}

static uint8_t functionReturn(int token) {
    uint8_t res;

    if ((token = _getc()) == EOF || (uint8_t)token == tEnter) {
        RET();
        ice.lastTokenIsReturn = true;
    } else if (token == tIf) {
        if ((res = parseExpression(_getc())) != VALID) {
            return res;
        }
        if (expr.outputIsString) {
            return E_SYNTAX;
        }

        //Check if we can optimize stuff :D
        optimizeZeroCarryFlagOutput();

        *ice.programPtr++ = (expr.AnsSetCarryFlag || expr.AnsSetCarryFlagReversed ?
            (expr.AnsSetCarryFlagReversed ? OP_RET_C : OP_RET_NC) :
            (expr.AnsSetZeroFlagReversed ? OP_RET_Z : OP_RET_NZ));
    } else {
        return E_SYNTAX;
    }
    return VALID;
}

static uint8_t functionDisp(int token) {
    do {
        uint8_t res;

        if ((uint8_t)(token = _getc()) == tii) {
            if ((token = _getc()) == EOF) {
                ice.tempToken = tEnter;
            } else {
                ice.tempToken = (uint8_t)token;
            }
            CALL(_NewLine);
            goto checkArgument;
        }

        // Get the argument, and display it, based on whether it's a string or the outcome of an expression
        expr.inFunction = true;
        if ((res = parseExpression(token)) != VALID) {
            return res;
        }

        AnsToHL();
        MaybeLDIYFlags();
        if (expr.outputIsString) {
            CALL(_PutS);
        } else {
            AnsToHL();
            CALL(_DispHL);
        }

checkArgument:
        ResetAllRegs();

        // Oops, there was a ")" after the expression
        if (ice.tempToken == tRParen) {
            return E_SYNTAX;
        }
    } while (ice.tempToken != tEnter);

    return VALID;
}

static uint8_t functionOutput(int token) {
    uint8_t res;

    // Get the first argument = column
    expr.inFunction = true;
    if ((res = parseExpression(_getc())) != VALID) {
        return res;
    }

    // Return syntax error if the expression was a string or the token after the expression wasn't a comma
    if (expr.outputIsString || ice.tempToken != tComma) {
        return E_SYNTAX;
    }

    if (expr.outputIsNumber) {
        uint8_t outputNumber = expr.outputNumber;

        ice.programPtr -= expr.SizeOfOutputNumber;
        LD_A(outputNumber);
        LD_IMM_A(curRow);

        // Get the second argument = row
        expr.inFunction = true;
        if ((res = parseExpression(_getc())) != VALID) {
            return res;
        }
        if (expr.outputIsString) {
            return E_SYNTAX;
        }

        // Yay, we can optimize things!
        if (expr.outputIsNumber) {
            // Output coordinates in H and L, 6 = sizeof(ld a, X \ ld (curRow), a)
            ice.programPtr -= 6 + expr.SizeOfOutputNumber;
            LD_SIS_HL((expr.outputNumber << 8) + outputNumber);
            LD_SIS_IMM_HL(curRow & 0xFFFF);
        } else {
            if (expr.outputIsVariable) {
                *(ice.programPtr - 2) = OP_LD_A_HL;
            } else if (expr.outputRegister == REGISTER_HL) {
                LD_A_L();
            } else if (expr.outputRegister == REGISTER_DE) {
                LD_A_E();
            }
            LD_IMM_A(curCol);
        }
    } else {
        if (expr.outputIsVariable) {
            *(ice.programPtr - 2) = OP_LD_A_HL;
        } else if (expr.outputRegister == REGISTER_HL) {
            LD_A_L();
        } else if (expr.outputRegister == REGISTER_DE) {
            LD_A_E();
        }
        LD_IMM_A(curRow);

        // Get the second argument = row
        expr.inFunction = true;
        if ((res = parseExpression(_getc())) != VALID) {
            return res;
        }
        if (expr.outputIsString) {
            return E_SYNTAX;
        }

        if (expr.outputIsNumber) {
            *(ice.programPtr - 4) = OP_LD_A;
            ice.programPtr -= 2;
        } else if (expr.outputIsVariable) {
            *(ice.programPtr - 2) = OP_LD_A_HL;
        } else if (expr.outputRegister == REGISTER_HL) {
            LD_A_L();
        } else if (expr.outputRegister == REGISTER_DE) {
            LD_A_E();
        }
        LD_IMM_A(curCol);
    }

    // Get the third argument = output thing
    if (ice.tempToken == tComma) {
        MaybeLDIYFlags();
        if ((res = parseExpression(_getc())) != VALID) {
            return res;
        }

        // Call the right function to display it
        if (expr.outputIsString) {
            CALL(_PutS);
        } else {
            AnsToHL();
            CALL(_DispHL);
        }
        ResetAllRegs();
    } else if (ice.tempToken != tEnter) {
        return E_SYNTAX;
    }

    return VALID;
}

static uint8_t functionClrHome(int token) {
    if (!CheckEOL()) {
        return E_SYNTAX;
    }
    CALL(_HomeUp);
    CALL(_ClrLCDFull);
    ResetAllRegs();

    return VALID;
}

static uint8_t functionFor(int token) {
    bool endPointIsNumber = false, stepIsNumber = false, reversedCond = false, smallCode;
    uint24_t endPointNumber = 0, stepNumber = 0, tempDataOffsetElements;
    uint8_t tempGotoElements = prescan.amountOfGotos;
    uint8_t tempLblElements = prescan.amountOfLbls;
    uint8_t *endPointExpressionValue = 0, *stepExpression = 0, *jumpToCond, *loopStart;
    uint8_t tok, variable, res;

    if ((tok = _getc()) < tA || tok > tTheta) {
        return E_SYNTAX;
    }
    variable = GetVariableOffset(tok);
    expr.inFunction = true;
    if (_getc() != tComma) {
        return E_SYNTAX;
    }

    // Get the start value, followed by a comma
    if ((res = parseExpression(_getc())) != VALID) {
        return res;
    }
    if (ice.tempToken != tComma) {
        return E_SYNTAX;
    }

    // Load the value in the variable
    MaybeAToHL();
    if (expr.outputRegister == REGISTER_HL) {
        LD_IX_OFF_IND_HL(variable);
    } else {
        LD_IX_OFF_IND_DE(variable);
    }

    // Get the end value
    expr.inFunction = true;
    if ((res = parseExpression(_getc())) != VALID) {
        return res;
    }

    // If the end point is a number, we can optimize things :D
    if (expr.outputIsNumber) {
        endPointIsNumber = true;
        endPointNumber = expr.outputNumber;
        ice.programPtr -= expr.SizeOfOutputNumber;
    } else {
        AnsToHL();
        endPointExpressionValue = ice.programPtr;
        LD_ADDR_HL(0);
    }

    // Check if there was a step
    if (ice.tempToken == tComma) {
        expr.inFunction = true;

        // Get the step value
        if ((res = parseExpression(_getc())) != VALID) {
            return res;
        }
        if (ice.tempToken == tComma) {
            return E_SYNTAX;
        }

        if (expr.outputIsNumber) {
            stepIsNumber = true;
            stepNumber = expr.outputNumber;
            ice.programPtr -= expr.SizeOfOutputNumber;
        } else {
            AnsToHL();
            stepExpression = ice.programPtr;
            LD_ADDR_HL(0);
        }
    } else {
        stepIsNumber = true;
        stepNumber = 1;
    }

    jumpToCond = ice.programPtr;
    JP(0);
    tempDataOffsetElements = ice.dataOffsetElements;
    loopStart = ice.programPtr;
    ResetAllRegs();

    // Parse the inner loop
    if ((res = parseProgram()) != E_END && res != VALID) {
        return res;
    }

    // First add the step to the variable, if the step is 0 we don't need this
    if (!stepIsNumber || stepNumber) {
        // ld hl, (ix+*) \ inc hl/dec hl (x times) \ ld (ix+*), hl
        // ld hl, (ix+*) \ ld de, x \ add hl, de \ ld (ix+*), hl

        LD_HL_IND_IX_OFF(variable);
        if (stepIsNumber) {
            uint8_t a = 0;

            if (stepNumber < 5) {
                for (a = 0; a < (uint8_t)stepNumber; a++) {
                    INC_HL();
                }
            } else if (stepNumber > 0xFFFFFF - 4) {
                for (a = 0; a < (uint8_t)(0-stepNumber); a++) {
                    DEC_HL();
                }
            } else {
                LD_DE_IMM(stepNumber);
                ADD_HL_DE();
            }
        } else {
            w24(stepExpression + 1, ice.programPtr + PRGM_START - ice.programData + 1);
            ice.ForLoopSMCStack[ice.ForLoopSMCElements++] = (uint24_t*)(endPointExpressionValue + 1);

            LD_DE_IMM(0);
            ADD_HL_DE();
        }
        LD_IX_OFF_IND_HL(variable);
    }

    smallCode = JumpForward(jumpToCond, ice.programPtr, tempDataOffsetElements, tempGotoElements, tempLblElements);
    ResetAllRegs();

    // If both the step and the end point are a number, the variable is already in HL
    if (!(endPointIsNumber && stepIsNumber)) {
        LD_HL_IND_IX_OFF(variable);
    }

    if (endPointIsNumber) {
        if (stepNumber < 0x800000) {
            LD_DE_IMM(endPointNumber + 1);
        } else {
            LD_DE_IMM(endPointNumber);
            reversedCond = true;
        }
        OR_A_A();
    } else {
        w24(endPointExpressionValue + 1, ice.programPtr + PRGM_START - ice.programData + 1);
        ice.ForLoopSMCStack[ice.ForLoopSMCElements++] = (uint24_t*)(endPointExpressionValue + 1);

        LD_DE_IMM(0);
        if (stepNumber < 0x800000) {
            SCF();
        } else {
            OR_A_A();
            reversedCond = true;
        }
    }
    SBC_HL_DE();

    // Jump back to the loop
    JumpBackwards(loopStart - (smallCode ? 2 : 0), OP_JR_C - (reversedCond ? 8 : 0));

    return VALID;
}

static uint8_t functionPrgm(int token) {
    uint24_t length;
    uint8_t a = 0;
    uint8_t *tempProgramPtr;

    MaybeLDIYFlags();
    tempProgramPtr = ice.programPtr;

    *ice.programPtr++ = TI_PRGM_TYPE;

    // Fetch the name
    while ((token = _getc()) != EOF && (uint8_t)token != tEnter && ++a < 9) {
        *ice.programPtr++ = token;
    }
    *ice.programPtr++ = 0;

    // Check if valid program name
    if (!a || a == 9) {
        return E_INVALID_PROG;
    }

    length = ice.programPtr - tempProgramPtr;
    ice.programDataPtr -= length;
    memcpy(ice.programDataPtr, tempProgramPtr, length);
    ice.programPtr = tempProgramPtr;

    ProgramPtrToOffsetStack();
    LD_HL_IMM((uint24_t)ice.programDataPtr);

    // Insert the routine to run it
    CALL(_Mov9ToOP1);
    LD_HL_IMM(tempProgramPtr - ice.programData + PRGM_START + 28);
    memcpy(ice.programPtr, PrgmData, 20);
    ice.programPtr += 20;
    ResetAllRegs();

    return VALID;
}

static uint8_t functionCustom(int token) {
    uint8_t tok = _getc();

    if (tok >= tDefineSprite && tok <= tSetBrightness) {
        // Call
        if (tok == tCall) {
            insertGotoLabel();
            CALL(0);

            return VALID;
        } else {
            SeekMinus1();

            return parseExpression(token);
        }
    } else {
        return E_UNIMPLEMENTED;
    }
}

static uint8_t functionLbl(int token) {
    // Add the label to the stack, and skip the line
    label_t *labelCurr = &ice.LblStack[ice.curLbl++];
    uint8_t a = 0;

    // Get the label name
    while ((token = _getc()) != EOF && (uint8_t)token != tEnter && a < 20) {
        labelCurr->name[a++] = token;
    }
    labelCurr->name[a] = 0;
    labelCurr->addr = (uint24_t)ice.programPtr;
    labelCurr->LblGotoElements = prescan.amountOfLbls;
    ResetAllRegs();

    return VALID;
}

static uint8_t functionGoto(int token) {
    insertGotoLabel();
    JP(0);

    return VALID;
}

void insertGotoLabel(void) {
    // Add the label to the stack, and skip the line
    label_t *gotoCurr = &ice.GotoStack[ice.curGoto++];
    uint8_t a = 0;
    int token;

    while ((token = _getc()) != EOF && (uint8_t)token != tEnter && a < 20) {
        gotoCurr->name[a++] = token;
    }
    gotoCurr->name[a] = 0;
    gotoCurr->addr = (uint24_t)ice.programPtr;
    gotoCurr->offset = _tell(ice.inPrgm);
    gotoCurr->dataOffsetElements = ice.dataOffsetElements;
    gotoCurr->LblGotoElements = prescan.amountOfGotos;
    ResetAllRegs();
}

static uint8_t functionPause(int token) {
    if (CheckEOL()) {
        MaybeLDIYFlags();
        CALL(_GetCSC);
        CP_A(9);
        JR_NZ(-8);
        ResetReg(REGISTER_HL);
        reg.AIsNumber = true;
        reg.AIsVariable = false;
        reg.AValue = 9;
    } else {
        uint8_t res;

        SeekMinus1();
        if ((res = parseExpression(_getc())) != VALID) {
            return res;
        }
        AnsToHL();

        CallRoutine(&ice.usedAlreadyPause, &ice.PauseAddr, (uint8_t*)PauseData, SIZEOF_PAUSE_DATA, prescan.amountOfPauseRoutines);
        reg.HLIsNumber = reg.DEIsNumber = true;
        reg.HLIsVariable = reg.DEIsVariable = false;
        reg.HLValue = reg.DEValue = -1;
        ResetReg(REGISTER_BC);
    }

    return VALID;
}

static uint8_t functionInput(int token) {
    uint8_t res, var;

    expr.inFunction = true;
    if ((res = parseExpression(_getc())) != VALID) {
        return res;
    }

    if (ice.tempToken == tComma && expr.outputIsString) {
        expr.inFunction = true;
        if ((res = parseExpression(_getc())) != VALID) {
            return res;
        }
        var = *(ice.programPtr - 1);
        ice.programPtr -= 3;
    } else {
        var = *(ice.programPtr - 1);
        ice.programPtr -= 3;

        // FF0000 reads all zeroes, and that's important
        LD_HL_IMM(0xFF0000);
    }

    if (ice.tempToken != tEnter || !expr.outputIsVariable) {
        return E_SYNTAX;
    }

    MaybeLDIYFlags();
    
    if (prescan.amountOfInputRoutines == 1) {
        memcpy(ice.programPtr, InputData, SIZEOF_INPUT_DATA);
        ice.programPtr += SIZEOF_INPUT_DATA;
        *(ice.programPtr - 5) = var;
        *(ice.programPtr - 4) = OP_CALL;
    } else {
        LD_A_IND_IX_OFF(var);
        
        // Copy the Input routine to the data section
        if (!ice.usedAlreadyInput) {
            ice.programDataPtr -= SIZEOF_INPUT_DATA;
            ice.InputAddr = (uintptr_t)ice.programDataPtr;
            memcpy(ice.programDataPtr, (uint8_t*)InputData, SIZEOF_INPUT_DATA);
            ice.usedAlreadyInput = true;
        }

        // Set which var we need to store to
        ProgramPtrToOffsetStack();
        LD_ADDR_A(ice.InputAddr + SIZEOF_INPUT_DATA - 5);

        // Call the right routine
        ProgramPtrToOffsetStack();
        CALL(ice.InputAddr);
    }
    ResetAllRegs();

    return VALID;
}

static uint8_t functionBB(int token) {
    // Asm(
    if ((uint8_t)(token = _getc()) == tAsm) {
        while ((token = _getc()) != EOF && (uint8_t)token != tEnter && (uint8_t)token != tRParen) {
            uint8_t tok1, tok2;

            // Get hexadecimals
            if ((tok1 = IsHexadecimal(token)) == 16 || (tok2 = IsHexadecimal(_getc())) == 16) {
                return E_INVALID_HEX;
            }

            *ice.programPtr++ = (tok1 << 4) + tok2;
        }
        if ((uint8_t)token == tRParen) {
            if (!CheckEOL()) {
                return E_SYNTAX;
            }
        }

        ResetAllRegs();

        return VALID;
    }

    // AsmComp(
    else if ((uint8_t)token == tAsmComp) {
        uint8_t res = VALID;
        uint24_t currentLine = ice.currentLine;
        ti_var_t tempProg = ice.inPrgm;
        prog_t *outputPrgm;
        
        outputPrgm = GetProgramName();
        if (outputPrgm->errorCode != VALID) {
            return outputPrgm->errorCode;
        }

#ifdef COMPUTER_ICE
        if ((ice.inPrgm = _open(str_dupcat(outputPrgm->prog, ".8xp")))) {
            int tempProgSize = ice.programLength;

            fseek(ice.inPrgm, 0, SEEK_END);
            ice.programLength = ftell(ice.inPrgm);
            _rewind(ice.inPrgm);
            fprintf(stdout, "Compiling subprogram %s\n", str_dupcat(outputPrgm->prog, ".8xp"));

            // Compile it, and close
            ice.currentLine = 0;
            if ((res = parseProgram()) != VALID) {
                return res;
            }
            fclose(ice.inPrgm);
            ice.currentLine = currentLine;
            ice.programLength = tempProgSize;
        } else {
            res = E_PROG_NOT_FOUND;
        }
#elif defined(__EMSCRIPTEN__)
        if ((ice.inPrgm = _open(outputPrgm->prog))) {
            ice.currentLine = 0;
            if ((res = parseProgram()) != VALID) {
                return res;
            }
            _close(ice.inPrgm);
            ice.currentLine = currentLine;
        } else {
            res = E_PROG_NOT_FOUND;
        }
#else
        if ((ice.inPrgm = _open(outputPrgm->prog))) {
            char buf[35];

            displayLoadingBarFrame();
            sprintf(buf, "Compiling subprogram %s...", outputPrgm->prog);
            displayMessageLineScroll(buf);
            strcpy(ice.currProgName[ice.inPrgm], outputPrgm->prog);

            // Compile it, and close
            ice.currentLine = 0;
            if ((res = parseProgram()) != VALID) {
                return res;
            }
            ti_Close(ice.inPrgm);

            displayLoadingBarFrame();
            displayMessageLineScroll("Return from subprogram...");
            ice.currentLine = currentLine;
        } else {
            res = E_PROG_NOT_FOUND;
        }
#endif
        ice.inPrgm = tempProg;

        return res;
    } else {
        SeekMinus1();
        return parseExpression(t2ByteTok);
    }
}

void optimizeZeroCarryFlagOutput(void) {
    if (!expr.AnsSetZeroFlag && !expr.AnsSetCarryFlag && !expr.AnsSetZeroFlagReversed && !expr.AnsSetCarryFlagReversed) {
        if (expr.outputRegister == REGISTER_HL) {
            ADD_HL_DE();
            OR_A_SBC_HL_DE();
            expr.AnsSetZeroFlag = true;
        } else if (expr.outputRegister == REGISTER_DE) {
            SCF();
            SBC_HL_HL();
            ADD_HL_DE();
            expr.AnsSetCarryFlagReversed = true;
        } else {
            OR_A_A();
        }
    } else {
        ice.programPtr -= expr.ZeroCarryFlagRemoveAmountOfBytes;
    }
    return;
}

void skipLine(void) {
    while (!CheckEOL());
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
    functionI,          // 0x2C
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
    ParseOSLists,       // 0x5D
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
    parseExpression,    // 0xAE
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
    functionIf,         // 0xCE
    ParseUnimplemented, // 0xCF
    functionElse,       // 0xD0
    functionWhile,      // 0xD1
    functionRepeat,     // 0xD2
    functionFor,        // 0xD3
    functionEnd,        // 0xD4
    functionReturn,     // 0xD5
    functionLbl,        // 0xD6
    functionGoto,       // 0xD7
    functionPause,      // 0xD8
    ParseUnimplemented, // 0xD9
    ParseUnimplemented, // 0xDA
    ParseUnimplemented, // 0xDB
    functionInput,      // 0xDC
    ParseUnimplemented, // 0xDD
    functionDisp,       // 0xDE
    ParseUnimplemented, // 0xDF
    functionOutput,     // 0xE0
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
