#include "defines.h"
#include "functions.h"

#include "errors.h"
#include "stack.h"
#include "parse.h"
#include "main.h"
#include "output.h"
#include "operator.h"
#include "routines.h"
#include "prescan.h"

static const uint8_t GraphxArgs[] = {
    RET_NONE | 0, ARG_NORM,    // Begin
    RET_NONE | 0, ARG_NORM,    // End
    RET_A    | 1, SMALL_1,     // SetColor
    RET_NONE | 0, ARG_NORM,    // SetDefaultPalette
    RET_NONE | 3, ARG_NORM,    // SetPalette
    RET_NONE | 1, SMALL_1,     // FillScreen
    RET_NONE | 2, SMALL_2,     // SetPixel
    RET_A    | 2, SMALL_2,     // GetPixel
    RET_A    | 0, ARG_NORM,    // GetDraw
    RET_NONE | 1, SMALL_1,     // SetDraw
    RET_NONE | 0, ARG_NORM,    // SwapDraw
    RET_NONE | 1, SMALL_1,     // Blit
    RET_NONE | 3, SMALL_123,   // BlitLines
    RET_NONE | 5, SMALL_13,    // BlitArea
    RET_NONE | 1, SMALL_1,     // PrintChar
    RET_NONE | 2, SMALL_2,     // PrintInt
    RET_NONE | 2, SMALL_2,     // PrintUInt
    RET_NONE | 1, ARG_NORM,    // PrintString
    RET_NONE | 3, ARG_NORM,    // PrintStringXY
    RET_NONE | 2, ARG_NORM,    // SetTextXY
    RET_A    | 1, SMALL_1,     // SetTextBGColor
    RET_A    | 1, SMALL_1,     // SetTextFGColor
    RET_A    | 1, SMALL_1,     // SetTextTransparentColor
    RET_NONE | 1, ARG_NORM,    // SetCustomFontData
    RET_NONE | 1, ARG_NORM,    // SetCustomFontSpacing
    RET_NONE | 1, SMALL_1,     // SetMonoSpaceFont
    RET_HL   | 1, ARG_NORM,    // GetStringWidth
    RET_HL   | 1, SMALL_1,     // GetCharWidth
    RET_HL   | 0, ARG_NORM,    // GetTextX
    RET_HL   | 0, ARG_NORM,    // GetTextY
    RET_NONE | 4, ARG_NORM,    // Line
    RET_NONE | 3, ARG_NORM,    // HorizLine
    RET_NONE | 3, ARG_NORM,    // VertLine
    RET_NONE | 3, ARG_NORM,    // Circle
    RET_NONE | 3, ARG_NORM,    // FillCircle
    RET_NONE | 4, ARG_NORM,    // Rectangle
    RET_NONE | 4, ARG_NORM,    // FillRectangle
    RET_NONE | 4, SMALL_24,    // Line_NoClip
    RET_NONE | 3, SMALL_2,     // HorizLine_NoClip
    RET_NONE | 3, SMALL_2,     // VertLine_NoClip
    RET_NONE | 3, SMALL_2,     // FillCircle_NoClip
    RET_NONE | 4, SMALL_24,    // Rectangle_NoClip
    RET_NONE | 4, SMALL_24,    // FillRectangle_NoClip
    RET_NONE | 4, ARG_NORM,    // SetClipRegion
    RET_A    | 1, ARG_NORM,    // GetClipRegion
    RET_NONE | 1, SMALL_1,     // ShiftDown
    RET_NONE | 1, SMALL_1,     // ShiftUp
    RET_NONE | 1, SMALL_1,     // ShiftLeft
    RET_NONE | 1, SMALL_1,     // ShiftRight
    RET_NONE | 3, ARG_NORM,    // Tilemap
    RET_NONE | 3, ARG_NORM,    // Tilemap_NoClip
    RET_NONE | 3, ARG_NORM,    // TransparentTilemap
    RET_NONE | 3, ARG_NORM,    // TransparentTilemap_NoClip
    RET_HL   | 3, ARG_NORM,    // TilePtr
    RET_HL   | 3, SMALL_23,    // TilePtrMapped
    UN       | 0, ARG_NORM,    // LZDecompress
    UN       | 0, ARG_NORM,    // AllocSprite
    RET_NONE | 3, ARG_NORM,    // Sprite
    RET_NONE | 3, ARG_NORM,    // TransparentSprite
    RET_NONE | 3, SMALL_3,     // Sprite_NoClip
    RET_NONE | 3, SMALL_3,     // TransparentSprite_NoClip
    RET_HL   | 3, ARG_NORM,    // GetSprite
    RET_NONE | 5, SMALL_345,   // ScaledSprite_NoClip
    RET_NONE | 5, SMALL_345,   // ScaledTransparentSprite_NoClip
    RET_HL   | 2, ARG_NORM,    // FlipSpriteY
    RET_HL   | 2, ARG_NORM,    // FlipSpriteX
    RET_HL   | 2, ARG_NORM,    // RotateSpriteC
    RET_HL   | 2, ARG_NORM,    // RotateSpriteCC
    RET_HL   | 2, ARG_NORM,    // RotateSpriteHalf
    RET_NONE | 2, ARG_NORM,    // Polygon
    RET_NONE | 2, ARG_NORM,    // Polygon_NoClip
    RET_NONE | 6, ARG_NORM,    // FillTriangle
    RET_NONE | 6, ARG_NORM,    // FillTriangle_NoClip
    UN       | 0, ARG_NORM,    // LZDecompressSprite
    RET_NONE | 2, SMALL_12,    // SetTextScale
    RET_A    | 1, SMALL_1,     // SetTransparentColor
    RET_NONE | 0, ARG_NORM,    // ZeroScreen
    RET_NONE | 1, SMALL_1,     // SetTextConfig
    RET_HL   | 1, SMALL_1,     // GetSpriteChar
    RET_HLs  | 2, SMALL_2,     // Lighten
    RET_HLs  | 2, SMALL_2,     // Darken
    RET_A    | 1, SMALL_1,     // SetFontHeight
    RET_HL   | 2, ARG_NORM,    // ScaleSprite
    RET_NONE | 3, SMALL_23,    // FloodFill
    RET_NONE | 3, ARG_NORM,    // RLETSprite
    RET_NONE | 3, SMALL_3,     // RLETSprite_NoClip
    RET_HL   | 2, ARG_NORM,    // ConvertFromRLETSprite
    RET_HL   | 2, ARG_NORM,    // ConvertToRLETSprite
    UN       | 2, ARG_NORM,    // ConvertToNewRLETSprite
    RET_HL   | 4, SMALL_34,    // RotateScaleSprite
    RET_HL   | 5, SMALL_345,   // RotatedScaledTransparentSprite_NoClip
    RET_HL   | 5, SMALL_345,   // RotatedScaledSprite_NoClip
};

static const uint8_t FileiocArgs[] = {
    RET_NONE | 0, ARG_NORM,    // CloseAll
    RET_A    | 2, ARG_NORM,    // Open
    RET_A    | 3, SMALL_3,     // OpenVar
    RET_NONE | 1, SMALL_1,     // Close
    RET_HL   | 4, SMALL_4,     // Write
    RET_HL   | 4, SMALL_4,     // Read
    RET_HL   | 1, SMALL_1,     // GetChar
    RET_HL   | 2, SMALL_12,    // PutChar
    RET_HL   | 1, ARG_NORM,    // Delete
    RET_HL   | 2, SMALL_2,     // DeleteVar
    RET_HL   | 3, SMALL_23,    // Seek
    RET_HL   | 2, SMALL_2,     // Resize
    RET_HL   | 1, SMALL_1,     // IsArchived
    RET_NONE | 2, SMALL_12,    // SetArchiveStatus
    RET_HL   | 1, SMALL_1,     // Tell
    RET_HL   | 1, SMALL_1,     // Rewind
    RET_HL   | 1, SMALL_1,     // GetSize
    RET_HL   | 3, ARG_NORM,    // GetTokenString
    RET_HL   | 1, SMALL_1,     // GetDataPtr
    RET_HL   | 2, ARG_NORM,    // Detect
    RET_HL   | 3, SMALL_3,     // DetectVar
};

static uint8_t amountOfArgs;
static element_t outputCurr;
static element_t outputPrev;
static element_t outputPrevPrev;

float execFunc(uint8_t func, float operand1, float operand2) {
    switch (func) {
        case tNot:
            return !operand1;
        case tMin:
            return (operand1 < operand2 ? operand1 : operand2);
        case tMax:
            return (operand1 > operand2 ? operand1 : operand2);
        case tMean:
            return (operand1 + operand2) / 2;
        case tSqrt:
            return sqrt(operand2);
        case tSin:
            return 255 * sin(operand2 * (2 * M_PI / 256));
        case tCos:
            return 255 * cos(operand2 * (2 * M_PI / 256));
        case tInt:
            return floor(operand2);
        case tAbs:
            return abs(operand2);
        case tRound:
            return round(operand2);
        case tExtTok:
            return (float)((uint24_t)operand1 % (uint24_t)operand2);
        default:
            return 0;
    }
}

uint8_t compileFunction(uint24_t index) {
    outputCurr     = getOutputElement(index);
    outputPrev     = getOutputElement(index - 1);
    outputPrevPrev = getOutputElement(index - 2);
    amountOfArgs   = outputCurr.operand.func.amountOfArgs;
    
    return (*functions[index].functionPtr)(functions[index].function);
}

uint8_t FunctionBrace(uint8_t tok) {
	return VALID;
}

uint8_t FunctionOSList(uint8_t tok) {
	return VALID;
}

uint8_t FunctionGetKey(uint8_t tok) {
	return VALID;
}

uint8_t FunctionNot(uint8_t tok) {
	return VALID;
}

uint8_t FunctionMinMax(uint8_t tok) {
	return VALID;
}

uint8_t FunctionMean(uint8_t tok) {
	return VALID;
}

uint8_t FunctionSqrt(uint8_t tok) {
	return VALID;
}

uint8_t FunctionDetSum(uint8_t tok) {
	return VALID;
}

uint8_t FunctionSinCos(uint8_t tok) {
	return VALID;
}

uint8_t FunctionRand(uint8_t tok) {
	return VALID;
}

uint8_t FunctionAns(uint8_t tok) {
	return VALID;
}

uint8_t FunctionParen(uint8_t tok) {
	return VALID;
}

uint8_t FunctionBrack(uint8_t tok) {
	return VALID;
}

uint8_t FunctionInt(uint8_t tok) {
    return VALID;
}

uint8_t FunctionAbs(uint8_t tok) {
    return VALID;
}

uint8_t FunctionRound(uint8_t tok) {
    return VALID;
}

uint8_t FunctionRemainder(uint8_t tok) {
	return VALID;
}

uint8_t FunctionCheckTmr(uint8_t tok) {
	return VALID;
}

uint8_t FunctionStartTmr(uint8_t tok) {
	return VALID;
}

uint8_t FunctionSubString(uint8_t tok) {
	return VALID;
}

uint8_t FunctionLength(uint8_t tok) {
	return VALID;
}

uint8_t FunctionRandInt(uint8_t tok) {
	return VALID;
}

uint8_t FunctionDefineSprite(uint8_t tok) {
	return VALID;
}

uint8_t FunctionData(uint8_t tok) {
	return VALID;
}

uint8_t FunctionCopy(uint8_t tok) {
	return VALID;
}

uint8_t FunctionAlloc(uint8_t tok) {
	return VALID;
}

uint8_t FunctionDefineTilemap(uint8_t tok) {
    return VALID;
}

uint8_t FunctionCopyData(uint8_t tok) {
	return VALID;
}

uint8_t FunctionLoadData(uint8_t tok) {
	return VALID;
}

uint8_t FunctionSetBrightness(uint8_t tok) {
	return VALID;
}

const function_t functions[AMOUNT_OF_FUNCTIONS] = {
// function / second byte / amount of arguments / disallow arguments as numbers / function
    {tLBrace,   0,              1,   0, FunctionBrace},
    {tVarLst,   0,              1,   0, FunctionOSList},
    {tGetKey,   0,              -1,  0, FunctionGetKey},
/* Never change the first 3 elements of this function array, these indexes are hardcoded and important! */
    {tNot,      0,              1,   1, FunctionNot},
    {tMin,      0,              2,   1, FunctionMinMax},
    {tMax,      0,              2,   1, FunctionMinMax},
    {tMean,     0,              2,   1, FunctionMean},
    {tSqrt,     0,              1,   1, FunctionSqrt},
    {tDet,      0,              -1,  0, FunctionDetSum},
    {tSum,      0,              -1,  0, FunctionDetSum},
    {tSin,      0,              1,   1, FunctionSinCos},
    {tCos,      0,              1,   1, FunctionSinCos},
    {tRand,     0,              0,   0, FunctionRand},
    {tAns,      0,              0,   0, FunctionAns},
    {tLParen,   0,              1,   0, FunctionParen},
    {tLBrack,   0,              1,   0, FunctionBrack},
    {tInt,      0,              1,   1, FunctionInt},
    {tAbs,      0,              1,   1, FunctionAbs},
    {tRound,    0,              1,   1, FunctionRound},
    {tExtTok,   tRemainder,     2,   1, FunctionRemainder},
    {tExtTok,   tCheckTmr,      2,   0, FunctionCheckTmr},
    {tExtTok,   tStartTmr,      0,   0, FunctionStartTmr},
    {t2ByteTok, tSubStrng,      3,   0, FunctionSubString},
    {t2ByteTok, tLength,        1,   0, FunctionLength},
    {t2ByteTok, tRandInt,       2,   0, FunctionRandInt},
    {tVarOut,   tDefineSprite,  -1,  0, FunctionDefineSprite},
    {tVarOut,   tData,          -1,  0, FunctionData},
    {tVarOut,   tCopy,          -1,  0, FunctionCopy},
    {tVarOut,   tAlloc,         1,   0, FunctionAlloc},
    {tVarOut,   tDefineTilemap, -1,  0, FunctionDefineTilemap},
    {tVarOut,   tCopyData,      -1,  0, FunctionCopyData},
    {tVarOut,   tLoadData,      3,   0, FunctionLoadData},
    {tVarOut,   tSetBrightness, 1,   0, FunctionSetBrightness}
};
