include 'includes/ez80.inc'
include 'includes/ti84pceg.inc'
include 'includes/tiformat.inc'
format ti appvar 'ICEAPPV'

repeat 3
	macro r#% inst&
		local pc
		pc := $
		token_table#% equ pc
		inst
	end macro
end repeat

AMOUNT_OF_CUSTOM_TOKENS = 13
AMOUNT_OF_GRAPHX_FUNCTIONS = 93
AMOUNT_OF_FILEIOC_FUNCTIONS = 24

SQUARE_WIDTH = 18
SQUARE_HEIGHT = 13
SQUARES_START_POS = 30

org 0

KeyHook_start:
	db	83h
	or	a, a
	ret	z
	ld	b, a
	ld	a, (cxCurApp)
	cp	a, cxPrgmEdit
	ld	a, b
	ret	nz
	push	af
	call	_os_ClearStatusBarLow
	res	0, (iy-41h)
; Stupid OS, we need to copy it to safe RAM, because the OS clears progToEdit when opening another OS context
	ld	a, (progToEdit)
	or	a, a
	jr	z, DontCopyProgramName
	ld	de, saveSScreen
	ld	hl, progToEdit
	call	_Mov9b
DontCopyProgramName:
	pop	af
	cp	a, kTrace
	jr	z, DisplayCustomTokens
	cp	a, kGraph
	ret	nz
	ld	de, DisplayPalette
	ld	hl, (rawKeyHookPtr)
	add	hl, de
	jp	(hl)
DisplayCustomTokens:
	call	_CursorOff
	ld	d, 0
DisplayTabWithTokens:
	push	de
	call	_ClrLCDFull
	pop	de
	ld	a, 30
	ld	(penRow), a
	ld	hl, 12
	ld	(penCol), hl
	ld	b, h
	ld	a, d
	ld	e, 3
	mlt	de
	ld	hl, TabPointers
	add	hl, de
	ld	de, (rawKeyHookPtr)
	add	hl, de
	ld	hl, (hl)
	add	hl, de
	ld	d, a
	ld	e, b
	jr	DisplayTokensLoop
KeyIsLeft:
	ld	a, d
	or	a, a
	jr	z, KeyLoop
	dec	d
	jr	DisplayTabWithTokens
KeyIsRight:
	ld	a, d
	cp	a, 8
	jr	z, KeyLoop
	inc	d
	jr	DisplayTabWithTokens
DisplayTokensLoop:
	ld	a, b
	cp	a, 16
	jr	z, StopDisplayingTokens
	inc	b
	call	_VPutS
	push	hl
	ld	a, (penRow)
	add	a, 13
	ld	(penRow), a
	ld	hl, 12
	ld	(penCol), hl
	pop	hl
	ld	a, (hl)
	or	a, a
	jr	nz, DisplayTokensLoop
StopDisplayingTokens:
	ld	a, 1
	ld	(penCol), a
GetRightCustomToken:
; penRow = 30+e*13
	ld	a, e
	add	a, a
	add	a, a
	ld	b, a
	add	a, a
	add	a, b
	add	a, e
	add	a, 30
	ld	(penRow), a
	ld	a, 1
	ld	(penCol), a
	push	de
	ld	a, '>'
	call	_VPutMap
	pop	de
	ld	a, 1
	ld	(penCol), a
KeyLoop:
	call	_GetCSC
	or	a, a
	jr	z, KeyLoop
	cp	a, skLeft
	jr	z, KeyIsLeft
	cp	a, skRight
	jr	z, KeyIsRight
	cp	a, skUp
	jr	nz, KeyNotUp
	ld	a, e
	or	a, a
	jr	z, KeyLoop
	dec	e
EraseCursor:
	push	de
	ld	a, ' '
	call	_VPutMap
	ld	a, ' '
	call	_VPutMap
	ld	a, ' '
	call	_VPutMap
	pop	de
	jr	GetRightCustomToken
KeyNotUp:
	cp	a, skDown
	jr	nz, KeyNotDown
	ld	a, d
	cp	a, 8
	ld	a, e
	jr	nz, CheckLastTab
	cp	a, (AMOUNT_OF_CUSTOM_TOKENS + AMOUNT_OF_GRAPHX_FUNCTIONS + AMOUNT_OF_FILEIOC_FUNCTIONS) mod 16 - 1
	jr	z, KeyLoop
CheckLastTab:
	ld	a, e
	cp	a, 16-1
	jr	z, KeyLoop
	inc	e
	jr	EraseCursor
KeyNotDown:
	cp	a, skClear
	jr	z, KeyIsClear
	cp	a, skEnter
	jr	nz, KeyLoop
	ld	a, e
	ld	e, 16
	mlt	de
	add	a, e
	sub	a, AMOUNT_OF_CUSTOM_TOKENS
	jr	c, InsertCustomToken
	ld	hl, saveSScreen + 9
	cp	a, AMOUNT_OF_FILEIOC_FUNCTIONS
	jr	nc, InsertDetToken
	ld	(hl), tSum
	jr	InsertCFunction
InsertDetToken:
	ld	(hl), tDet
	sub	a, AMOUNT_OF_FILEIOC_FUNCTIONS
InsertCFunction:
	inc	hl
	cp	a, 10
	jr	c, TokenIsLessThan10
	ld	d, a
	ld	e, 10
	xor	a, a
	ld	b, 8
.loop:
	sla	d
	rla
	cp	a, e
	jr	c, $+4
	sub	a, e
	inc	d
	djnz	.loop
	ld	e, a
	ld	a, d
	add	a, t0
	ld	(hl), a
	inc	hl
	ld	a, e
TokenIsLessThan10:
	add	a, t0
	ld	(hl), a
	inc	hl
	ld	(hl), 0
	ld	hl, saveSScreen + 9
InsertCFunctionLoop:
	ld	a, (hl)
	or	a, a
KeyIsClear:
	jr	z, BufferSearch
	ld	de, (editTail)
	ld	a, (de)
	cp	a, tEnter
	ld	d, 0
	ld	e, (hl)
	jr	z, DoInsertToken
	push	hl
	call	_BufReplace
	pop	hl
	inc	hl
	jr	InsertCFunctionLoop
DoInsertToken:
	push	hl
	call	_BufInsert
	pop	hl
	inc	hl
	jr	InsertCFunctionLoop
InsertCustomToken:
	add	a, 10 + AMOUNT_OF_CUSTOM_TOKENS
	ld	e, a
	ld	d, tVarOut
	ld	hl, (editCursor)
	ld	a, (hl)
	cp	a, tEnter
	jr	z, .jump
	call	_BufReplace
	jr	BufferSearch
.jump:	call	_BufInsert
BufferSearch:
	ld	bc, 0
.loop:	call	_BufLeft
	jr	z, BufferFound
	ld	a, e
	cp	a, tEnter
	jr	z, BufferFoundEnter
	inc	bc
	jr	.loop
BufferFoundEnter:
	call	_BufRight
BufferFound:
	push	bc
	call	_ClrLCDFull
	call	_ClrTxtShd
	xor	a, a
	ld	(curCol), a
	ld	(curRow), a
	ld	a, (winTop)
	or	a, a
	jr	z, DisplayProgramText		; This is apparently a Cesium feature
	ld	de, ProgramText
	ld	hl, (rawKeyHookPtr)
	add	hl, de
	call	_PutS
	ld	hl, saveSScreen
	ld	b, 8
.loop:	ld	a, (hl)
	or	a, a
	jr	z, StopDisplayingString
	call	_PutC
	inc	hl
	djnz	.loop
StopDisplayingString:
	call	_NewLine
DisplayProgramText:
	ld	a, ':'
	call	_PutC
	call	_DispEOW
	pop	bc
MoveCursorOnce:
	ld	a, b
	or	a, c
	jr	z, ReturnToEditor
	call	_CursorRight
	dec	bc
	jr	MoveCursorOnce
ReturnToEditor:
	call	_CursorOn
	inc	a			;    reset zero flag
	ld	a, 0
	ret
	
DisplayPalette:
	call	_RunIndicOff
	call	_boot_ClearVRAM
; Display the header strings
	ld	hl, 120
	ld	(penCol), hl
	ld	a, 1
	ld	(penRow), a
	ld	de, (rawKeyHookPtr)
	ld	hl, ColorText
	add	hl, de
	call	_VPutS
	ld	hl, SQUARES_START_POS + 6
	ld	(penCol), hl
	ld	a, 14
	ld	(penRow), a
	push	de
	ld	hl, ColumnHeaderText
	add	hl, de
	call	_VPutS
	ld	hl, vRAM + ((SQUARES_START_POS * lcdWidth + SQUARES_START_POS) * 2)
	ld	b, 0
; Display all the squares
DisplaySquaresRows:
; Display the entire square
	ld	c, SQUARE_HEIGHT
DisplaySquareRowLoop:
; Display one row of a square
	ld	a, SQUARE_WIDTH
	ld	de, (lcdWidth - SQUARE_WIDTH) * 2
DisplaySquareRowInnerLoop:
	ld	(hl), b
	inc	hl
	ld	(hl), b
	inc	hl
	dec	a
	jr	nz, DisplaySquareRowInnerLoop
	add	hl, de
	dec	c
	jr	nz, DisplaySquareRowLoop
	inc	b
	ld	a, b
	and	a, 000001111b
	jr	nz, DontAddNewRow
; We need to switch to the next row = update pointer + display index in front of row
; (Index / 16) * 13 + 30 = Y pos
	ld	a, b
	sub	a, 16
	push	hl
	ld	e, a
	push	bc
	or	a, a
	rra
	rra
	rra
	rra
	ld	b, a
	ld	c, SQUARE_HEIGHT
	mlt	bc
	ld	a, SQUARES_START_POS
	add	a, c
	ld	(penRow), a
; Do some magic stuff to get X pos
	ld	a, e
	ld	de, 8
	ld	hl, 4
	cp	a, 100
	jr	nc, SetHLPenCol
	add	hl, de
	cp	a, 10
	jr	nc, SetHLPenCol
	dec	de
	dec	de
	add	hl, de
SetHLPenCol:
	ld	(penCol), hl
	ld	e, a
	ex	de, hl
	ld	b, 3
	call	_VDispHL
	pop	bc
	pop	hl
; Set pointer to new row
	ld	de, (-SQUARE_WIDTH * 15) * 2
	jr	$+6
DontAddNewRow:
; Add square to pointer
	ld	de, (-SQUARE_HEIGHT * lcdWidth + SQUARE_WIDTH) * 2
	add	hl, de
	ld	a, b
	or	a, a
	jr	nz, DisplaySquaresRows
; Done, wait and return
.wait:	call	_GetCSC
	or	a, a
	jr	z, .wait
	call	_DrawStatusBar
	pop	de
	ld	hl, BufferSearch
	add	hl, de
	jp	(hl)
KeyHook_end:

repeat 1,x:$-KeyHook_start
	display `x,10
end repeat

TokenHook_start:
	db	83h
	ld	a, d
	cp	a, 4
	ret	nz
	ld	a, e
	cp	a, 3
	ret	c
	cp	a, 5 + (AMOUNT_OF_CUSTOM_TOKENS * 3)
	ret	nc
	sub	a, 5
	sbc	hl, hl
	ld	l, a
	ld	de, CustomTokensPointers
	add	hl, de
	ld	de, (rawKeyHookPtr)
	add	hl, de
	ld	hl, (hl)
	add	hl, de
	ret
TokenHook_end:

repeat 1,x:$-TokenHook_start
	display `x,10
end repeat

CursorHook_start:
	db	83h
	cp	a, 24h
	jr	nz, CheckCursor
	inc	a
	ld	a, (curUnder)
	ret
CheckCursor:
	cp	a, 22h
	ret	nz
	ld	a, (cxCurApp)
	cp	a, cxPrgmEdit
	ret	nz
	ld	hl, (editCursor)
	ld	a, (hl)
	cp	a, tSum
	jr	z, DrawDetText
	cp	a, tDet
	ret	nz
DrawDetText:
	bit	0, (iy-41h)
	ret	nz
	ld	b, a
	ld	hl, (editTail)
	inc	hl
	ld	a, (hl)
	sub	a, t0
	ret	c
	cp	a, t9-t0+1
	jr	c, GetDetValue
WrongDetValue:
	or	a, 1
	ret
GetDetValue:
	ld	iyl, b
	ld	bc, (editBtm)
	ld	de, 0
	ld	e, a
GetDetValueLoop:
	inc	hl
	or	a, a
	sbc	hl, bc
	jr	z, GetDetValueStop
	add	hl, bc
	ld	a, (hl)
	sub	a, t0
	jr	c, GetDetValueStop
	cp	a, t9-t0+1
	jr	nc, GetDetValueStop
	push	hl
	ex	de, hl
	add	hl, hl
	push	hl
	pop	de
	add	hl, hl
	add	hl, hl
	add	hl, de
	ld	de, 0
	ld	e, a
	add	hl, de
	ex	de, hl
	pop	hl
	jr	GetDetValueLoop
GetDetValueStop:
	ex	de, hl
	ld	a, iyl
	ld	iy, flags
	cp	a, tDet
	ld	de, AMOUNT_OF_FILEIOC_FUNCTIONS
	ld	bc, FileiocFunctionsPointers
	jr	nz, DontCheckDetFunctions
	ld	de, AMOUNT_OF_GRAPHX_FUNCTIONS
	ld	bc, GraphxFunctionsPointers
DontCheckDetFunctions:
	or	a, a
	sbc	hl, de
	jr	nc, WrongDetValue
	add	hl, de
	ld	h, 3
	mlt	hl
	add	hl, bc
	push	hl
	call	_os_ClearStatusBarLow
	pop	hl
	ld	de, (rawKeyHookPtr)
	add	hl, de
	ld	hl, (hl)
	add	hl, de
	ld	de, 000E71Ch
	ld.sis	(drawFGColor and 0FFFFh), de
	ld.sis	de, (statusBarBGColor and 0FFFFh)
	ld.sis	(drawBGColor and 0FFFFh), de
	ld	a, 14
	ld	(penRow),a
	ld	de, 2
	ld.sis	(penCol and 0FFFFh), de
	call	_VPutS
	ld	de, 0FFFFh
	ld.sis	(drawBGColor and 0FFFFh), de
	set	0, (iy-41h)
	inc	a
	ret

Tab1:
	db "DefineSprite(W,H[,PTR])", 0
	db "Call LABEL", 0
	db "Data(SIZE,CONST...)", 0
	db "Copy(PTR_OUT,PTR_IN,SIZE)", 0
	db "Alloc(BYTES)", 0
	db "DefineTilemap()", 0
	db "CopyData(PTR_OUT,SIZE,CONST...)", 0
	db "LoadData(TILEMAP,OFFSET,SIZE)", 0
	db "SetBrightness(LEVEL)", 0
	db "SetByte(VAR1[,VAR2...])", 0
	db "SetInt(VAR1[,VAR2...])", 0
	db "SetFloat(VAR1[,VAR2...])", 0
	db "Compare(PTR1,PTR2,SIZE)", 0

	r1 db "CloseAll()", 0
	r1 db "Open(NAME,MODE)", 0
	r1 db "OpenVar(NAME,MODE,TYPE)", 0
Tab2:
	r1 db "Close(SLOT)", 0
	r1 db "Write(DATA,SIZE,COUNT,SLOT)", 0
	r1 db "Read(PTR,SIZE,COUNT,SLOT)", 0
	r1 db "GetChar(SLOT)", 0
	r1 db "PutChar(CHAR,SLOT)", 0
	r1 db "Delete(NAME)", 0
	r1 db "DeleteVar(NAME,TYPE)", 0
	r1 db "Seek(OFFSET,ORIGIN,SLOT)", 0
	r1 db "Resize(SIZE,SLOT)", 0
	r1 db "IsArchived(SLOT)", 0
	r1 db "SetArchiveStatus(ARCHIVED,SLOT)", 0
	r1 db "Tell(SLOT)", 0
	r1 db "Rewind(SLOT)", 0
	r1 db "GetSize(SLOT)", 0
	r1 db "GetTokenString(", 014h, "PTR,", 014h, "L_TOK,", 014h, "L_STRING)", 0
	r1 db "GetDataPtr(SLOT)", 0
Tab3:
	r1 db "Detect(", 014h, "PTR,DATA)", 0
	r1 db "DetectVar(", 014h, "PTR,DATA,TYPE)", 0
	r1 db "SetVar(TYPE,NAME,DATA)", 0
	r1 db "StoVar(TYPE_O,PTR_O,TYPE_I,PTR_I)", 0
	r1 db "RclVar(TYPE,NAME,PTR)", 0

	r2 db "Begin()", 0
	r2 db "End()", 0
	r2 db "SetColor(COLOR)", 0
	r2 db "SetDefaultPalette()", 0
	r2 db "SetPalette(PALETTE)", 0
	r2 db "FillScreen(COLOR)", 0
	r2 db "SetPixel(X,Y)", 0
	r2 db "GetPixel(X,Y)", 0
	r2 db "GetDraw()", 0
	r2 db "SetDraw(LOC)", 0
	r2 db "SwapDraw()", 0
Tab4:
	r2 db "Blit(LOC)", 0
	r2 db "BlitLines(LOC,Y,NUM)", 0
	r2 db "BlitArea(LOC,X,Y,W,H)", 0
	r2 db "PrintChar(CHAR)", 0
	r2 db "PrintInt(N,CHARS)", 0
	r2 db "PrintUInt(N,CHARS)", 0
	r2 db "PrintString(STRING)", 0
	r2 db "PrintStringXY(STRING,X,Y)", 0
	r2 db "SetTextXY(X,Y)", 0
	r2 db "SetTextBGColor(COLOR)", 0
	r2 db "SetTextFGColor(COLOR)", 0
	r2 db "SetTextTransparentColor(COLOR)", 0
	r2 db "SetCustomFontData(DATA)", 0
	r2 db "SetCustomFontSpacing(DATA)", 0
	r2 db "SetMonospaceFont(SPACE)", 0
	r2 db "GetStringWidth(STRING)", 0
Tab5:
	r2 db "GetCharWidth(CHAR)", 0
	r2 db "GetTextX()", 0
	r2 db "GetTextY()", 0
	r2 db "Line(X1,Y1,X2,Y2)", 0
	r2 db "HorizLine(X,Y,LENGTH)", 0
	r2 db "VertLine(X,Y,LENGTH)", 0
	r2 db "Circle(X,Y,R)", 0
	r2 db "FillCircle(X,Y,R)", 0
	r2 db "Rectangle(X,Y,W,H)", 0
	r2 db "FillRectangle(X,Y,W,H)", 0
	r2 db "Line_NoClip(X1,Y1,X2,Y2)", 0
	r2 db "HorizLine_NoClip(X,Y,LENGTH)", 0
	r2 db "VertLine_NoClip(X,Y,LENGTH)", 0
	r2 db "FillCircle_NoClip(X,Y,R)", 0
	r2 db "Rectangle_NoClip(X,Y,W,H)", 0
	r2 db "FillRectangle_NoClip(X,Y,W,H)", 0
Tab6:
	r2 db "SetClipRegion(XMIN,YMIN,XMAX,YMAX)", 0
	r2 db "GetClipRegion(PTR)", 0
	r2 db "ShiftDown(PIXELS)", 0
	r2 db "ShiftUp(PIXELS)", 0
	r2 db "ShiftLeft(PIXELS)", 0
	r2 db "ShiftRight(PIXELS)", 0
	r2 db "Tilemap(PTR,X,Y)", 0
	r2 db "Tilemap_NoClip(PTR,X,Y)", 0
	r2 db "TransparentTilemap(PTR,X,Y)", 0
	r2 db "TransparentTilemap_NoClip(PTR,X,Y)", 0
	r2 db "TilePtr(PTR,X,Y)", 0
	r2 db "TilePtrMapped(PTR,ROW,COL)", 0
	r2 db "NOT USED", 0
	r2 db "NOT USED", 0
	r2 db "Sprite(PTR,X,Y)", 0
	r2 db "TransparentSprite(PTR,X,Y)", 0
Tab7:
	r2 db "Sprite_NoClip(PTR,X,Y)", 0
	r2 db "TransparentSprite_NoClip(PTR,X,Y)", 0
	r2 db "GetSprite_NoClip(PTR,X,Y)", 0
	r2 db "ScaledSprite_NoClip(PTR,X,Y)", 0
	r2 db "ScaledTransparentSprite_NoClip(PTR,X,Y)", 0
	r2 db "FlipSpriteY(PTR_IN,PTR_OUT)", 0
	r2 db "FlipSpriteX(PTR_IN,PTR_OUT)", 0
	r2 db "RotateSpriteC(PTR_IN,PTR_OUT)", 0
	r2 db "RotateSpriteCC(PTR_IN,PTR_OUT)", 0
	r2 db "RotateSpriteHalf(PTR_IN,PTR_OUT)", 0
	r2 db "Polygon(POINTS,NUM)", 0
	r2 db "Polygon_NoClip(POINTS,NUM)", 0
	r2 db "FillTriangle(X1,Y1,X2,Y2,X3,Y3)", 0
	r2 db "FillTriangle_NoClip(X1,Y1,X2,Y2,X3,Y3)", 0
	r2 db "NOT USED", 0
	r2 db "SetTextScale(W_SCALE,H_SCALE)", 0
Tab8:
	r2 db "SetTransparentColor(COLOR)", 0
	r2 db "ZeroScreen()", 0
	r2 db "SetTextConfig(CONFIG)", 0
	r2 db "GetSpriteChar(CHAR)", 0
	r2 db "Lighten(COLOR,AMOUNT)", 0
	r2 db "Darken(COLOR,AMOUNT)", 0
	r2 db "SetFontHeight(HEIGHT)", 0
	r2 db "ScaledSprite(PTR_IN,PTR_OUT)", 0
	r2 db "FloodFill(X,Y,COLOR)", 0
	r2 db "RLETSprite(PTR,X,Y)", 0
	r2 db "RLETSprite_NoClip(PTR,X,Y)", 0
	r2 db "ConvertFromRLETSprite(PTR_IN,PTR_OUT)", 0
	r2 db "ConvertToRLETSprite(PTR_IN,PTR_OUT)", 0
	r2 db "ConvertToNewRLETSprite()", 0
	r2 db "Rot.Sc.Spr.(PTR_IN,PTR_OUT,ANGLE,SCALE)", 0
	r2 db "Rot.Sc.Tr.Spr._NC(PTR,X,Y,ANGLE,SCALE)", 0
Tab9:
	r2 db "Rot.Sc.Spr._NC(PTR,X,Y,ANGLE,SCALE)", 0
	r2 db "SetCharData(INDEX,DATA)", 0
	db 0
      
; These magic bytes can be found with _GetKeyPress (D is 2-byte token, E is token, A is output)
	r3 db 090h, 13, "DefineSprite("		; 62 0A
	r3 db 0EEh, 5,  "Call "			; 62 0B
	r3 db 038h, 5,  "Data("			; 62 0C
	r3 db 084h, 5,  "Copy("			; 62 0D
	r3 db 087h, 6,  "Alloc("		; 62 0E
	r3 db 042h, 14, "DefineTilemap("	; 62 0F
	r3 db 043h, 9,  "CopyData("		; 62 10
	r3 db 0FFh, 9,  "LoadData("		; 62 11
	r3 db 0BAh, 14, "SetBrightness("	; 62 12
	r3 db 0C0h, 8,  "SetByte("		; 62 13
	r3 db 0BFh, 7,  "SetInt("		; 62 14
	r3 db 0C1h, 9,  "SetFloat("		; 62 15
	r3 db 0B7h, 8,  "Compare("		; 62 16

TabPointers:
	dl Tab1, Tab2, Tab3, Tab4, Tab5, Tab6, Tab7, Tab8
	dl Tab9
	
FileiocFunctionsPointers:
	irpv token, token_table1
		dl token
	end irpv
	
GraphxFunctionsPointers:
	irpv token, token_table2
		dl token
	end irpv
	
CustomTokensPointers:
	irpv token, token_table3
		dl token
	end irpv

ProgramText:
	db "PROGRAM:", 0
ColorText:
	db "COLOR=COL+ROW", 0
ColumnHeaderText:
	db "0", 0EFh
	db "1", 0EFh
	db "2", 0EFh
	db "3", 0EFh
	db "4", 0EFh
	db "5", 0EFh
	db "6", 0EFh
	db "7", 0EFh
	db "8", 0EFh
	db "9", 0EEh
	db "10", " "
	db "11", " "
	db "12", " "
	db "13", " "
	db "14", " "
	db "15", 0
Hooks_end:
