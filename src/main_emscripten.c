#include "defines.h"
#include "main.h"

#ifdef __EMSCRIPTEN__

#include "functions.h"
#include "errors.h"
#include "stack.h"
#include "parse.h"
#include "output.h"
#include "operator.h"
#include "routines.h"

ice_t ice;
expr_t expr;
reg_t reg;

extern const uint8_t CheaderData[];
extern const uint8_t SrandData[];
extern const uint8_t FileiocheaderData[];
const uint8_t colorTable[16] = {255,24,224,0,248,36,227,97,9,19,230,255,181,107,106,74};    // Thanks Cesium :D

void w24(void *x, uint32_t val) {
    uint8_t *ptr = (uint8_t*)(x);
    ptr[0] = val & 0xFF;
    ptr[1] = val >> 8 & 0xFF;
    ptr[2] = val >> 16 & 0xFF;
}

void w16(void *x, uint32_t val) {
    uint8_t *ptr = (uint8_t*)(x);
    ptr[0] = val & 0xFF;
    ptr[1] = val >> 8 & 0xFF;
}

uint32_t r24(void *x) {
    uint8_t *ptr = (uint8_t*)(x);
    return (ptr[2] << 16) | (ptr[1] << 8) | (ptr[0]);
}

int main(int argc, char **argv) {
    uint8_t res, temp;
    uint24_t programDataSize, offset, totalSize;
    prog_t *outputPrgm;
    
    ice.programData     = malloc(0xFFFF + 0x100);
    ice.programPtr      = ice.programData;
    ice.programDataData = ice.programData + 0xFFFF;
    ice.programDataPtr  = ice.programDataData;

    // Check for icon and description before putting the C functions in the output program
    preScanProgram();
    if (!(ice.LblStack = (label_t*)malloc(prescan.amountOfLbls * sizeof(label_t))) ||
        !(ice.GotoStack = (label_t*)malloc(prescan.amountOfGotos * sizeof(label_t)))) {
        displayError(E_MEM_LABEL);
        goto stop;
    }
    
    if (_getc() != 0x2C) {
        return 1;
    }
    outputPrgm = GetProgramName();
    if (outputPrgm->errorCode != VALID) {
        displayError(outputPrgm->errorCode);
        goto stop;
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
                displayError(E_INVALID_HEX);
                goto stop;
            }
            *ice.programPtr++ = colorTable[temp];
        } while (++b);

        if ((uint8_t)_getc() != tString || (uint8_t)_getc() != tEnter) {
            displayError(E_SYNTAX);
            goto stop;
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

    prescan.freeMemoryPtr = (prescan.tempStrings[1] = (prescan.tempStrings[0] = pixelShadow + 2000 * prescan.amountOfOSVarsUsed) + 2000) + 2000;

    LD_IX_IMM(IX_VARIABLES);

    // Eventually seed the rand
    if (ice.usesRandRoutine) {
        ice.programDataPtr -= SIZEOF_RAND_DATA;
        ice.randAddr = (uint24_t)ice.programDataPtr;
        memcpy(ice.programDataPtr, SrandData, SIZEOF_RAND_DATA);
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

    // Do the stuff
    res = parseProgram();

    // Create or empty the output program if parsing succeeded
    if (res == VALID) {
        uint8_t currentGoto, currentLbl;
        uint24_t previousSize = 0;

        // If the last token is not "Return", write a "ret" to the program
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
            res = 0;
            goto stop;
findNextLabel:;
        }

        // Get the sizes of both stacks
        ice.programSize = (uintptr_t)ice.programPtr - (uintptr_t)ice.programData;
        programDataSize = (uintptr_t)ice.programDataData - (uintptr_t)ice.programDataPtr;

        // Change the pointers to the data as well, but first calculate the offset
        offset = PRGM_START + ice.programSize - (uintptr_t)ice.programDataPtr;
        while (ice.dataOffsetElements--) {
            w24(ice.dataOffsetStack[ice.dataOffsetElements], *ice.dataOffsetStack[ice.dataOffsetElements] + offset);
        }
        totalSize = ice.programSize + programDataSize + 3;

        if (ice.startedGRAPHX && !ice.endedGRAPHX) {
            displayError(W_CLOSE_GRAPHX);
        }

        uint8_t *export = malloc(0x10000);

        // Write ASM header
        export[0] = tExtTok;
        export[1] = tAsm84CeCmp;

        // Write ICE header to be recognized by Cesium
        export[2] = 0x7F;

        // Write the header, main program, and data to output :D
        memcpy(&export[3], ice.programData, ice.programSize);
        memcpy(&export[3 + ice.programSize], ice.programDataData, programDataSize);

        // Write the actual program file
        export_program(ice.outName, export, totalSize);
        free(export);
    } else {
        displayError(res);
    }
    return 0;
stop:
    return 1;
}

#endif
