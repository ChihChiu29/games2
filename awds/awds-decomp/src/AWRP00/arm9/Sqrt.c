typedef unsigned int u32;

asm u32 Sqrt(register u32 x) {
    swi 0x0d
    bx lr
}
