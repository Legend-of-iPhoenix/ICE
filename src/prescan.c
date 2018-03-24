#include "defines.h"
#include "prescan.h"

#include "stack.h"
#include "parse.h"
#include "main.h"
#include "output.h"
#include "operator.h"
#include "functions.h"
#include "routines.h"
#include "errors.h"

#ifdef COMPUTER_ICE
#define INCBIN_PREFIX
#include "incbin.h"
INCBIN(Cheader, "src/asm/cheader.bin");
INCBIN(Fileiocheader, "src/asm/fileiocheader.bin");
#endif

extern const uint8_t implementedFunctions[AMOUNT_OF_FUNCTIONS][4];
prescan_t prescan;
const uint8_t colorTable[16] = {255,24,224,0,248,36,227,97,9,19,230,255,181,107,106,74};    // Thanks Cesium :D

void preScanProgram(void) {
    bool inString = false, afterNewLine = true, isFloatExpression = false, inInt = false;
    uint8_t intDepth = 0;
    int token;

    _rewind(ice.inPrgm);

    // Scan the entire program
    while ((token = _getc()) != EOF) {
        uint8_t tok = token, tok2 = 0;
        
        if (IsA2ByteTok(tok)) {
            tok2 = _getc();
        }

        if (afterNewLine) {
            afterNewLine = false;
            if (tok == tii) {
                skipLine();
            } else if (tok == tLbl) {
                prescan.amountOfLbls++;
                skipLine();
            } else if (tok == tGoto) {
                prescan.amountOfGotos++;
                skipLine();
            }
        }

        if (tok == tString) {
            prescan.usedTempStrings = true;
            inString = !inString;
        } else if (tok == tStore) {
            inString = false;
            token = _getc();
        } else {
            if (tok == tEnter || (tok == tColon && !inString)) {
                inString = false;
                isFloatExpression = false;
                afterNewLine = 2;
                intDepth = 0;
            } else if (!inString) {
                if (tok == tRand) {
                    prescan.amountOfRandRoutines++;
                    prescan.modifiedIY = true;
                } else if (tok == tSqrt) {
                    prescan.amountOfSqrtRoutines++;
                    prescan.modifiedIY = true;
                } else if (tok == tMean) {
                    prescan.amountOfMeanRoutines++;
                } else if (tok == tInput) {
                    prescan.amountOfInputRoutines++;
                } else if (tok == tPause) {
                    prescan.amountOfPauseRoutines++;
                } else if (tok == tVarLst) {
                    if (!prescan.OSLists[token = _getc()]) {
                        prescan.OSLists[token] = pixelShadow + 2000 * (prescan.amountOfOSVarsUsed++);
                    }
                } else if (tok == tVarStrng) {
                    prescan.usedTempStrings = true;
                    if (!prescan.OSStrings[token = _getc()]) {
                        prescan.OSStrings[token] = pixelShadow + 2000 * (prescan.amountOfOSVarsUsed++);
                    }
                } else if (tok == t2ByteTok) {
                    // AsmComp(
                    if ((tok = (uint8_t)_getc()) == tAsmComp) {
                        ti_var_t tempProg = ice.inPrgm;
                        prog_t *newProg = GetProgramName();

                        if ((ice.inPrgm = _open(newProg->prog))) {
                            preScanProgram();
                            _close(ice.inPrgm);
                        }
                        ice.inPrgm = tempProg;
                    } else if (tok == tRandInt) {
                        prescan.amountOfRandRoutines++;
                        prescan.modifiedIY = true;
                    }
                } else if (tok == tExtTok) {
                    if ((tok = (uint8_t)_getc()) == tStartTmr) {
                        prescan.amountOfTimerRoutines++;
                    }
                } else if (tok == tDet || tok == tSum) {
                    uint8_t tok1 = _getc();
                    uint8_t tok2 = _getc();

                    prescan.modifiedIY = true;

                    // Invalid det( command
                    if (tok1 < t0 || tok1 > t9) {
                        break;
                    }

                    // Get the det( command
                    if (tok2 < t0 || tok2 > t9) {
                        token = tok1 - t0;
                    } else {
                        token = (tok1 - t0) * 10 + (tok2 - t0);
                    }

                    if (tok == tDet) {
                        prescan.hasGraphxFunctions = true;
                        if (!token) {
                            prescan.hasGraphxStart = true;
                        }
                        if (token == 1) {
                            prescan.hasGraphxEnd = true;
                        }
                        if (!prescan.GraphxRoutinesStack[token]) {
                            prescan.GraphxRoutinesStack[token] = 1;
                        }
                    } else {
                        prescan.hasFileiocFunctions = true;
                        if (!token) {
                            prescan.hasFileiocStart = true;
                        }
                        if (!prescan.FileiocRoutinesStack[token]) {
                            prescan.FileiocRoutinesStack[token] = 1;
                        }
                    }
                } else if ((tok == tRParen || tok == tRBrace) && inInt) {
                    inInt = --intDepth;
                } else if (tok == tInt || tok == tDet) {
                    inInt = true;
                    intDepth++;
                } else if (tok == tDecPt && !inInt) {
                    isFloatExpression = true;
                } else {
                    uint8_t a;
                    
                    for (a = 0; a < AMOUNT_OF_FUNCTIONS; a++) {
                        if (tok == implementedFunctions[a][0] && tok2 == implementedFunctions[a][1] && implementedFunctions[a][2] && inInt) {
                            intDepth++;
                        }
                    }
                }
            }
        }
    }

    _rewind(ice.inPrgm);
}

uint8_t getNameIconDescription(void) {
    prog_t *outputPrgm;
    uint8_t temp;
    
    if (_getc() != 0x2C) {
        return E_NOT_ICE_PROG;
    }
    
    outputPrgm = GetProgramName();
    if (outputPrgm->errorCode != VALID) {
        return outputPrgm->errorCode;
    }
    strcpy(ice.outName, outputPrgm->prog);
    
    // Has icon
    if ((uint8_t)_getc() == tii && (uint8_t)_getc() == tString) {
        uint8_t b = 0;

        *ice.programPtr = OP_JP;
        w24(ice.programPtr + 4, 0x101001);
        ice.programPtr += 7;

        // Get hexadecimal
        do {
            if ((temp = IsHexadecimal(_getc())) == 16) {
                return E_INVALID_HEX;
            }
            *ice.programPtr++ = colorTable[temp];
        } while (++b);

        if ((uint8_t)_getc() != tString || (uint8_t)_getc() != tEnter) {
            return E_SYNTAX;
        }

        // Check description
        if ((uint8_t)_getc() == tii) {
            grabString(&ice.programPtr, false);
        }
        *ice.programPtr++ = 0;

        // Write the right jp offset
        w24(ice.programData + 1, ice.programPtr - ice.programData + PRGM_START);
    }
    
    _rewind(ice.inPrgm);
    return VALID;
}

uint8_t parsePrescan(void) {
    // Insert C functions
    if (prescan.hasGraphxFunctions) {
        uint8_t a;

        memcpy(ice.programPtr, CheaderData, SIZEOF_CHEADER);
        ice.programPtr += SIZEOF_CHEADER;
        for (a = 0; a < AMOUNT_OF_GRAPHX_FUNCTIONS; a++) {
            if (prescan.GraphxRoutinesStack[a]) {
                prescan.GraphxRoutinesStack[a] = (uint24_t)ice.programPtr;
                JP(a * 3);
            }
        }
    } else if (prescan.hasFileiocFunctions) {
        memcpy(ice.programPtr, CheaderData, SIZEOF_CHEADER - 9);
        ice.programPtr += SIZEOF_CHEADER - 9;
    }

    if (prescan.hasFileiocFunctions) {
        uint8_t a;

        memcpy(ice.programPtr, FileiocheaderData, 10);
        ice.programPtr += 10;
        for (a = 0; a < AMOUNT_OF_FILEIOC_FUNCTIONS; a++) {
            if (prescan.FileiocRoutinesStack[a]) {
                prescan.FileiocRoutinesStack[a] = (uint24_t)ice.programPtr;
                JP(a * 3);
            }
        }
    }
    
    // Set free RAM pointers (for strings)
    prescan.freeMemoryPtr = (prescan.tempStrings[1] = (prescan.tempStrings[0] = pixelShadow + 2000 * prescan.amountOfOSVarsUsed) + 2000) + 2000;
    
    // Cleanup code
    if (prescan.hasGraphxFunctions) {
        CALL(_RunIndicOff);
        CALL(ice.programPtr - ice.programData + PRGM_START);
        LD_IY_IMM(flags);
        JP(_DrawStatusBar);
    } else if (prescan.modifiedIY) {
        CALL(ice.programPtr - ice.programData + PRGM_START);
        LD_IY_IMM(flags);
        RET();
    }
    
    // Check free RAM for Lbls and Gotos
    if (!(ice.LblStack = (label_t*)malloc(prescan.amountOfLbls * sizeof(label_t))) ||
        !(ice.GotoStack = (label_t*)malloc(prescan.amountOfGotos * sizeof(label_t)))) {
            return E_MEM_LABEL;
    }
    
    // Define variable types (ints and floats)
    
    return VALID;
}