#ifndef OPERATOR_H
#define OPERATOR_H

#define REGISTER_HL    0
#define REGISTER_DE    1
#define REGISTER_BC    2
#define REGISTER_HL_DE 3
#define REGISTER_A     4

#define NO_PUSH   false
#define NEED_PUSH true

#define TempString1 0
#define TempString2 1

bool comparePtrToTempStrings(uint24_t addr);
uint8_t getIndexOfOperator(uint8_t _operator);
uint24_t executeOperator(uint24_t operand1, uint24_t operand2, uint8_t op);
void LD_HL_STRING(uint24_t stringPtr, uint8_t type);
uint8_t parseOperator(element_t *outputPrevPrevPrev, element_t *outputPrevPrev, element_t *outputPrev, element_t *outputCurr, bool canOptimizeStrings);

void StoToChainAns(void);
void EQInsert(void);
void GEInsert(void);
void AddStringString(void);
void StoStringString(void);
void StoStringVariable(void);
void StoStringChainAns(void);

void MultWithNumber(uint24_t num, uint8_t *programPtr, bool ChangeRegisters);

extern const char operators[];
extern const uint8_t operatorPrecedence[];
extern const uint8_t operatorPrecedence2[];

#endif
