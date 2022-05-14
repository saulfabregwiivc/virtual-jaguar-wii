
extern void  (*m68ki_instruction_jump_table[0x10000])(void); // opcode handler jump table 
//unsigned char m68ki_cycles[NUM_CPU_TYPES][0x10000]; // Cycles used by CPU type 
extern unsigned char m68ki_cycles[4][0x10000]; // Cycles used by CPU type 

extern void m68ki_build_opcode_table(void);

