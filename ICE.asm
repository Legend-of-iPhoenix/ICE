#include "ti84pce.inc"
#include "defines.asm"

.db tExtTok, tAsm84CeCmp
.org UserMem

DEBUGMODE = 0

start:
	ld (backupSP), sp
	ld hl, (curPC)
	ld (backupCurPC), hl
	ld hl, (endPC)
	ld (backupEndPC), hl
	call _RunIndicOff
	ld hl, offsets
	ld de, stackPtr
	ld bc, offset_end - offsets
	ldir
	call InstallHooks
GUI:
	ld a, lcdBpp8
	ld (mpLcdCtrl), a
SetPalette:
	ld hl, mpLcdPalette
	ld b, 0
_:	ld d, b
	ld a, b
	and 011000000b
	srl d
	rra
	ld e, a
	ld a, 000011111b
	and b
	or e
	ld (hl), a
	inc hl
	ld (hl), d
	inc hl
	inc b
	jr nz, -_
	ld hl, vRAM
	ld (hl), 189
	push hl
	pop de
	inc de
	ld bc, 320*10
	ldir
	ld (hl), 0
	ld bc, 320
	ldir
	ld (hl), 255
	ld bc, 320*229-1
	ldir	
	ld hl, ICEName
	ld a, 1
	ld (TextYPos_ASM), a
	add a, 20
	ld (TextXPos_ASM), a
	call PrintString
	ld hl, TextYPos_ASM
	inc (hl)
	inc (hl)
	ld hl, (progPtr)
FindPrograms:
	call FindNextGoodVar
	jr nz, StopFindingPrograms
	push hl
		ld a, (TextYPos_ASM)
		add a, 10
		jr c, +_
		ld (TextYPos_ASM), a
		ld hl, 10
		ld (TextXPos_ASM), hl
		ld hl, AmountOfPrograms
		inc (hl)
		call _ChkInRAM
		ld a, '#'
		call c, _PrintChar_ASM
		ld hl, (ProgramNamesPtr)
		ld de, -8
		add hl, de
		call PrintString
_:	pop hl
	jr FindPrograms
StopFindingPrograms:
	ld a, 13
	ld (TextYPos_ASM), a
	ld hl, 1
	ld (TextXPos_ASM), hl
	ld a, (AmountOfPrograms)
	or a
	jp z, NoProgramsError
	ld (AmountPrograms), a
	ld l, 1
PrintCursor:
	ld e, l
	ld d, 10
	mlt de
	inc e
	inc e
	inc e
	ld a, e
	ld (TextYPos_ASM), a
	xor a
	ld (color), a
	inc a
	ld (TextXPos_ASM), a
	ld a, '>'
	call _PrintChar_ASM
	ld a, 255
	ld (color), a
	ld a, 1
	ld (TextXPos_ASM), a
_:	push hl
		call _GetCSC
	pop hl
	or a
	jr z, -_
	cp skUp
	jr z, PressedUp
	cp skDown
	jr z, PressedDown
	cp skClear
	jp z, StopProgram
	cp skEnter
	jr nz, -_
PressedEnter:
	dec l
	ld h, 8
	mlt hl
	ld de, ProgramNamesStack
	add hl, de
	dec hl
	call _Mov9ToOP1
	jr StartParsing
PressedUp:
	ld a, l
	dec a
	jr z, -_
	dec l
	ld a, 23
	call _PrintChar_ASM
	jr PrintCursor
PressedDown:
	ld a, l
AmountPrograms = $+1
	cp a, 0
	jr z, -_
	inc l
	ld a, 23
	call _PrintChar_ASM
	jr PrintCursor
StartParsing:
	ld a, ProgObj
	ld (OP1), a
_:	call _ChkFindSym
	jr nc, +_
	ld hl, OP1
	inc (hl)
	jr -_
_:	call _ChkInRAM
	jr nc, +_
	ex de, hl
	ld de, 9
	add hl, de
	ld e, (hl)
	add hl, de
	inc hl
	ex de, hl
_:	ld bc, 0
	ex de, hl
	ld c, (hl)																; BC = program length
	inc hl
	ld b, (hl)
	inc hl
	ld (curPC), hl
	push hl
	pop de
	add hl, bc
	dec hl
	ld (endPC), hl
	ld a, 0DDh
	call InsertA															; ld ix, cursorImage
	ld a, 021h
	ld hl, cursorImage
	call InsertAHL															; ld ix, cursorImage
	ld hl, varname
	ld e, 9
GetProgramName:
	push hl
		call _IncFetch
	pop hl
	jp c, AddDataToProgramData
	inc hl
	cp tEnter
	jr z, StartParse
	cp tA
	jp c, InvalidTokenError
	cp ttheta+1
	jp nc, InvalidTokenError
	ld (hl), a
	dec e
	jr nz, GetProgramName
	jp InvalidNameLength
StartParse:
	ld hl, OP1+1
	ld de, varname+1
	ld b, 8
CheckNames:
	ld a, (de)
	or a
	jr z, CheckNamesSameLength
	cp (hl)
	jr nz, GoodCompile
	inc hl
	inc de
	djnz CheckNames
CheckNamesSameLength:
	cp (hl)
	jp z, SameNameError
GoodCompile:
	ld hl, (endPC)
	ld de, (curPC)
	ld (begPC_), de
	or a
	sbc hl, de
	inc hl
	ld (programSize), hl
	ld (iy+myFlags3), 0
	set comp_with_libs, (iy+myFlags3)
	ld hl, CData
	ld de, (programPtr)
	ld bc, CData2 - CData
	ldir
	ld (programPtr), de
	call ScanForCFunctions
	ld a, 0CDh
	ld hl, _RunIndicOff
	call InsertAHL															; call _RunIndicOff
	ld hl, (programPtr)
	ld de, 4+4+5+UserMem-program
	add hl, de
	call InsertAHL															; call *
	ld de, 08021FDh
	ld hl, 0C3D000h
	call InsertDEHL															; ld iy, flags \ jp *
	ld hl, _DrawStatusBar
	call InsertHL															; jp _DrawStatusBar
	ld a, (amountOfCRoutines)
	or a
	jr nz, StartCompiling
	res comp_with_libs, (iy+myFlags3)
	ld hl, program+5
	ld (programPtr), hl
StartCompiling:
	call ClearScreen
	ld hl, StartParseMessage
	call PrintString
	ld (iy+myFlags2), 0
ParseProgramUntilEnd:
	call _IncFetch
CompileLoop:
	ld (tempToken), a
	ld (iy+myFlags), 0
	ld (iy+myFlags4), 0
	cp tEnd
	jr nz, ++_
_:	ld a, (amountOfEnds)
	or a
	jp z, EndError
	dec a
	ld (amountOfEnds), a
	ld a, (tempToken)
	ret
_:	cp tElse
	jr z, --_
	call ParseExpression
	ld hl, (curPC)
begPC_ = $+1
	ld de, 0
	or a
	sbc hl, de
	ld bc, 320
	call __imulu
programSize = $+1
	ld bc, 0
	call __idivu
	push hl
	pop bc
	ld hl, -1
	add hl, bc
	jr nc, +_
	ld hl, vRAM+(320*25)
	ld (hl), 0
	push hl
	pop de
	inc de
	dec bc
	call _ChkBCIs0
	jr z, +_
	ldir
_:	call _IncFetch
	jr nc, CompileLoop
FindGotos:
	ld hl, amountOfEnds
	ld a, (hl)
	dec a
	ld (hl), a
	inc a
	ret nz
FindGotosLoop:
	ld hl, (gotoPtr)
	ld de, gotoStack
	or a
	sbc hl, de
	jr z, AddDataToProgramData												; have we finished all the Goto's?
	add hl, de
	dec hl
	dec hl
	dec hl
	push hl
		ld hl, (hl)
		ex de, hl															; de = pointer to goto data
		ld hl, (labelPtr)
FindLabels:
		ld bc, labelStack
		or a
		sbc hl, bc
		jp z, LabelError													; have we finished all the Lbl's?
		add hl, bc
		dec hl
		dec hl
		dec hl																; hl = pointer to label
FindLabel:
		push hl
			push de
				ld hl, (hl)													; hl = pointer to label data
				call CompareStrings											; is it the right label?
			pop de
		pop hl
		jr nz, LabelNotRightOne
RightLabel:
		dec hl
		dec hl
		dec hl
		ld hl, (hl)
		ld de, UserMem-program
		add hl, de
		ex de, hl															; de points to label memory
	pop hl																	; hl = pointer to goto
	dec hl
	dec hl
	dec hl
	ld hl, (hl)																; hl = pointer to jump to
	ld (hl), de
	ld hl, (gotoPtr)
	ld de, -6
	add hl, de																; get next Goto
	ld (gotoPtr), hl
	jr FindGotosLoop
LabelNotRightOne:
		dec hl
		dec hl
		dec hl
		jr FindLabels
StopFindLabels:
	pop hl
AddDataToProgramData:
	bit last_token_is_ret, (iy+myFlags2)
	jr nz, +_
	ld a, 0C9h
	call InsertA															; ret
_:	ld hl, (programDataDataPtr)
	ld bc, programDataData
	or a
	sbc hl, bc
	push hl
	pop bc																	; bc = data length
	jr z, CreateProgram														; check IF there is data
	ld de, (programPtr)
	ld hl, programDataData
	or a
	sbc hl, de
	push hl
		add hl, de
		ldir																; move the data to the end of the program
		ld (programPtr), de
	pop de
	ld hl, (programDataOffsetPtr)
AddDataLoop:																; update all the pointers pointing to data
	ld bc, programDataOffsetStack
	or a
	sbc hl, bc
	jr z, CreateProgram														; no more pointers left
	add hl, bc
	dec hl
	dec hl
	dec hl
	push hl
		ld hl, (hl)															; complicated stuff XD
		push hl
			ld hl, (hl)
			or a
			sbc hl, de
			ld bc, UserMem-program
			add hl, bc
			push hl
			pop bc
		pop hl
		ld (hl), bc															; ld (XXXXXX), hl
	pop hl
	jr AddDataLoop
CreateProgram:
	ld hl, varname
	call _Mov9ToOP1
	call _ChkFindSym
	call nc, _DelVar
	ld hl, (programPtr)
	ld bc, program
	or a
	sbc hl, bc
	push hl
		ld bc, 17
		add hl, bc
		push hl
			call _EnoughMem
			ld hl, NotEnoughMem
			jp c, DispFinalString
		pop hl
		ld bc, -15
		add hl, bc
		call _CreateProtProg
	pop bc
	inc de
	inc de
	ld hl, program
	ex de, hl
	ld (hl), tExtTok														; insert header
	inc hl
	ld (hl), 07Bh
	inc hl
	ex de, hl
	ldir																	; insert the program data
	ld hl, GoodCompileMessage
	set good_compilation, (iy+myFlags)
	jp DispFinalString														; DONE :D :D :D
	
#include "functions.asm"
#include "parse.asm"
#include "putchar.asm"
#include "programs.asm"
#include "hooks.asm"
#include "operators functions/functions.asm"
#include "operators functions/operators.asm"
#include "operators functions/function_for.asm"
#include "operators functions/function_C.asm"
#include "clibs/graphics.asm"
#include "data.asm"
;#include "editor2.asm"

.echo $-start+14