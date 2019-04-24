 .section ".rodata"
 .globl vert
 .type text, STT_OBJECT
vert:
 .incbin "meme.vert"
 .byte 0
 .size vert, .-vert
 
 .section ".rodata"
 .globl frag
 .type text, STT_OBJECT
frag:
 .incbin "meme.frag"
 .byte 0
 .size frag, .-frag
 