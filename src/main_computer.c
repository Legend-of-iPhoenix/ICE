#include "defines.h"
#include "main.h"

#ifdef COMPUTER_ICE

#include "functions.h"
#include "errors.h"
#include "stack.h"
#include "parse.h"
#include "output.h"
#include "operator.h"
#include "routines.h"
#include "prescan.h"

ice_t ice;
expr_t expr;
reg_t reg;

const char *infoStr = "ICE Compiler v3.0 - By Peter \"PT_\" Tillema";

extern char *str_dupcat(const char *s, const char *c);

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
    uint8_t amountOfPrograms, res = VALID, temp;
    uint24_t programDataSize, offset, totalSize;
    char buf[30], *var_name;

    var_name = argv[1];
    if (argc != 2) {
        fprintf(stderr, "Error: invalid amount of arguments\n");
        exit(1);
    }

    ice.inPrgm = _open(var_name);
    if (!ice.inPrgm) {
        fprintf(stdout, "Can't find input program\n");
        goto stop;
    }
    fprintf(stdout, "%s\nPrescanning...\n", infoStr);
    _seek(0, SEEK_END, ice.inPrgm);

    ice.programLength   = _tell(ice.inPrgm);
    ice.programData     = malloc(0xFFFF + 0x100);
    ice.programPtr      = ice.programData;
    ice.programDataData = ice.programData + 0xFFFF;
    ice.programDataPtr  = ice.programDataData;

    // Check for icon and description before putting the C functions in the output program
    preScanProgram();
    if ((res = getNameIconDescription()) != VALID || (res = parsePrescan())) {
        displayError(res);
        goto stop;
    }

    // Do the stuff
    fprintf(stdout, "Compiling program %s...\n", var_name);

    // Parse the program, create or empty the output program if parsing succeeded
    if ((res = parseProgram()) == VALID) {
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
        free(ice.LblStack);
        free(ice.GotoStack);

        // Display the size
        fprintf(stdout, "Succesfully compiled to %s.8xp!\n", ice.outName);
        fprintf(stdout, "Output size: %u bytes\n", totalSize);
    } else {
        displayError(res);
    }
    return 0;
stop:
    return 1;
}

#endif
