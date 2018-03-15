#include "defines.h"
#include "errors.h"

#include "stack.h"
#include "parse.h"
#include "main.h"
#include "output.h"
#include "operator.h"
#include "functions.h"
#include "routines.h"

static const char *errors[] = {
    "This token/function is not implemented (yet)",
#if !defined(COMPUTER_ICE) && !defined(__EMSCRIPTEN__)
    "This token cannot be used at the start of the   line",
#else
    "This token cannot be used at the start of the line",
#endif
    "This token doesn't have a condition",
    "You used 'Else' outside an If-statement",
    "You used 'End' outside a condition block",
    "You have an invalid \")\", \",\", \"(\", \")\", \"}\" or \"]\"",
    "You have an invalid expression",
    "Your icon should start with a quote",
    "Invalid hexadecimal",
    "ICE ERROR: please report it!",
    "You have the wrong number or arguments",
#if !defined(COMPUTER_ICE) && !defined(__EMSCRIPTEN__)
    "Unknown C function. If you are sure this              function exists, please contact me!",
#else
    "Unknown C function",
#endif
    "Subprogram not found",
    "Compiling subprograms not supported",
    "Invalid program name",
    "Not enough memory for Lbl and Goto",
    "Warning: Unknown char in the string!",
    "Warning: string has been automatically squish-ed!",
#if !defined(COMPUTER_ICE) && !defined(__EMSCRIPTEN__)
    "Warning: you need det(0) before using any           other graphics function!",
    "Warning: you need sum(0) before using any           other file i/o function!",
    "Warning: you need det(1) before returning to    the OS!",
#else
    "Warning: you need det(0) before using any other graphics function!",
    "Warning: you need sum(0) before using any other file i/o function!",
    "Warning: you need det(1) before returning to the OS!",
#endif
};

void displayLabelError(char *label) {
    char buf[30];

    sprintf(buf, "Label %s not found", label);
#ifdef COMPUTER_ICE
    fprintf(stdout, "%s\n", buf);
#elif defined(__EMSCRIPTEN__)
    ice_error(buf, 0);
#else
    gfx_SetTextFGColor(224);
    displayMessageLineScroll(buf);
#endif
}

void displayError(uint8_t index) {
    char buf[30];
    
#ifdef COMPUTER_ICE
    fprintf(stdout, "%s\n", errors[index]);
    fprintf(stdout, "Error at line %u\n", ice.currentLine);
#elif defined(__EMSCRIPTEN__)
    strcpy(buf, errors[index]);
    ice_error(buf, ice.currentLine);
#else
    gfx_SetTextFGColor(index < W_WRONG_CHAR ? 224 : 227);
    displayMessageLineScroll(errors[index]);

    gfx_SetTextFGColor(0);
    sprintf(buf, "Error at line %u", ice.currentLine);
    displayMessageLineScroll(buf);
#endif
}
