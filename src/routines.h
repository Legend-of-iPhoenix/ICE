#ifndef ROUTINES_H
#define ROUTINES_H

int getNextToken(void);
int grabString(uint8_t **outputPtr, bool stopAtStoreAndString, bool allowSquish);
bool CheckEOL(void);
bool IsA2ByteTok(uint8_t tok);
void LD_A_BC_IND_IX_OFF(uint8_t offset);
void ProgramPtrToOffsetStack(void);
void displayLoadingBarFrame(void);
void SeekMinus1(void);
void displayLoadingBar(void);
void ClearAnsFlags(void);
void LoadRegValue(uint8_t reg2, uint24_t val);
void LoadRegVariable(uint8_t reg2, uint8_t variable);
void ChangeRegValue(uint24_t inValue, uint24_t outValue, uint8_t opcodes[7]);
void ResetAllRegs(void);
void ResetReg(uint8_t reg2);
void RegChangeHLDE(void);
void AnsToHL(void);
void AnsToDE(void);
void AnsToBC(void);
void FloatAnsToEUHL(void);
void LD_A_BC_FLOAT(float num);
void LD_E_HL_FLOAT(float num);
void PushAns(void);
void LD_E_HL_IND_IX_OFF(uint8_t offset);
void LD_A_BC_IND_IX_OFF(uint8_t offset);
void displayMessageLineScroll(char *string);
void MaybeAToHL(void);
void MaybeLDIYFlags(void);
void CallRoutine(bool *routineBool, uint24_t *routineAddress, const uint8_t *routineData, uint8_t routineLength, uint8_t amountOfRoutine);
void printButton(uint24_t xPos);
prog_t *GetProgramName(void);
uint8_t IsHexadecimal(int token);
uint8_t GetVariableOffset(uint8_t tok);
uint8_t getLastByteOfFloat(float x);
uint24_t get3ByteOfFloat(float x);

#endif
