#ifndef ERRORS_H
#define ERRORS_H

enum {
    E_UNIMPLEMENTED,
    E_WRONG_PLACE,
    E_NO_CONDITION,
    E_ELSE,
    E_END,
    E_EXTRA_PAREN,
    E_SYNTAX,
    E_WRONG_ICON,
    E_INVALID_HEX,
    E_ICE_ERROR,
    E_ARGUMENTS,
    E_UNKNOWN_C,
    E_PROG_NOT_FOUND,
    E_NO_SUBPROG,
    E_INVALID_PROG,
    E_MEM_LABEL,
    E_NOT_ICE_PROG,
    E_MEM_VARIABLES,
    W_WRONG_CHAR,
    W_SQUISHED,
    W_VALID,
    VALID
};

void displayLabelError(char *label);
void displayError(uint8_t index);

#endif
