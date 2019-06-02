 .section ".rodata"
 .globl vert
 .type text, STT_OBJECT
vert:
 .incbin "ui.vert"
 .byte 0
 .size vert, .-vert
 
 .section ".rodata"
 .globl frag
 .type text, STT_OBJECT
frag:
 .incbin "ui.frag"
 .byte 0
 .size frag, .-frag
 