#ifndef PARSE_H
#define PARSE_H

enum {
    TYPE_NUMBER,
    TYPE_VARIABLE,
    TYPE_CHAIN_ANS,
    TYPE_CHAIN_PUSH,
    TYPE_FUNCTION_START,
    TYPE_ARG_DELIMITER,
    TYPE_OPERATOR,
    TYPE_FUNCTION
};

enum {
    TYPE_MASK_U8,
    TYPE_MASK_U16,
    TYPE_MASK_U24
};

enum {
    TYPE_BYTE,
    TYPE_INT,
    TYPE_FLOAT
};

void skipLine(void);
void EntireStackToOutput(void);
uint8_t ParseProgram(void);
uint8_t ParseUntilEnd(void);
uint8_t ParseNewLine(uint8_t tok);
uint8_t ParseOperator(uint8_t tok);
uint8_t ParseFunction(uint8_t tok);
uint8_t ParseExpression(void);

#endif
