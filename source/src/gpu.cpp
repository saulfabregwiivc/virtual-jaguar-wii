//
// GPU Core
//
// Originally by David Raingeard (Cal2)
// GCC/SDL port by Niels Wagenaar (Linux/WIN32) and Caz (BeOS)
// Cleanups, endian wrongness, and bad ASM amelioration by James L. Hammons
// Note: Endian wrongness probably stems from the MAME origins of this emu and
//       the braindead way in which MAME handles memory. :-)
//
// Problem with not booting the BIOS was the incorrect way that the
// SUBC instruction set the carry when the carry was set going in...
// Same problem with ADDC...
//

#include "gpu.h"

//#define GPU_DEBUG

// For GPU dissasembly...

#define GPU_DIS_ABS
#define GPU_DIS_ADD
#define GPU_DIS_ADDC
#define GPU_DIS_ADDQ
#define GPU_DIS_ADDQT
#define GPU_DIS_AND
#define GPU_DIS_BCLR
#define GPU_DIS_BSET
#define GPU_DIS_BTST
#define GPU_DIS_CMP
#define GPU_DIS_CMPQ
#define GPU_DIS_DIV
#define GPU_DIS_IMULT
#define GPU_DIS_JUMP
#define GPU_DIS_JR
#define GPU_DIS_LOAD
#define GPU_DIS_LOADB
#define GPU_DIS_LOADW
#define GPU_DIS_LOAD14I
#define GPU_DIS_LOAD14R
#define GPU_DIS_LOAD15I
#define GPU_DIS_LOAD15R
#define GPU_DIS_MOVE
#define GPU_DIS_MOVEFA
#define GPU_DIS_MOVEI
#define GPU_DIS_MOVEPC
#define GPU_DIS_MOVETA
#define GPU_DIS_MOVEQ
#define GPU_DIS_MULT
#define GPU_DIS_NEG
#define GPU_DIS_NOP
#define GPU_DIS_NOT
#define GPU_DIS_OR
#define GPU_DIS_PACK
#define GPU_DIS_ROR
#define GPU_DIS_RORQ
#define GPU_DIS_SAT8
#define GPU_DIS_SH
#define GPU_DIS_SHA
#define GPU_DIS_SHARQ
#define GPU_DIS_SHLQ
#define GPU_DIS_SHRQ
#define GPU_DIS_STORE
#define GPU_DIS_STOREB
#define GPU_DIS_STOREW
#define GPU_DIS_STORE14I
#define GPU_DIS_STORE14R
#define GPU_DIS_STORE15I
#define GPU_DIS_STORE15R
#define GPU_DIS_SUB
#define GPU_DIS_SUBC
#define GPU_DIS_SUBQ
#define GPU_DIS_SUBQT
#define GPU_DIS_XOR

bool doGPUDis = false;
//bool doGPUDis = true;
//*/
/*
GPU opcodes use (BIOS flying ATARI logo):
+	              add 357416
+	             addq 538030
+	            addqt 6999
+	              sub 116663
+	             subq 188059
+	            subqt 15086
+	              neg 36097
+	              and 233993
+	               or 109332
+	              xor 1384
+	             btst 111924
+	             bset 25029
+	             bclr 10551
+	             mult 28147
+	            imult 69148
+	              div 64102
+	              abs 159394
+	             shlq 194690
+	             shrq 292587
+	            sharq 192649
+	             rorq 58672
+	              cmp 244963
+	             cmpq 114834
+	             move 833472
+	            moveq 56427
+	           moveta 220814
+	           movefa 170678
+	            movei 152025
+	            loadw 108220
+	             load 430936
+	           storew 3036
+	            store 372490
+	          move_pc 2330
+	             jump 349134
+	               jr 529171
	            mmult 64904
+	              nop 432179
*/

// Various bits

#define CINT0FLAG			0x0200
#define CINT1FLAG			0x0400
#define CINT2FLAG			0x0800
#define CINT3FLAG			0x1000
#define CINT4FLAG			0x2000
#define CINT04FLAGS			(CINT0FLAG | CINT1FLAG | CINT2FLAG | CINT3FLAG | CINT4FLAG)

// GPU_FLAGS bits

#define ZERO_FLAG		0x0001
#define CARRY_FLAG		0x0002
#define NEGA_FLAG		0x0004
#define IMASK			0x0008
#define INT_ENA0		0x0010
#define INT_ENA1		0x0020
#define INT_ENA2		0x0040
#define INT_ENA3		0x0080
#define INT_ENA4		0x0100
#define INT_CLR0		0x0200
#define INT_CLR1		0x0400
#define INT_CLR2		0x0800
#define INT_CLR3		0x1000
#define INT_CLR4		0x2000
#define REGPAGE			0x4000
#define DMAEN			0x8000

// External global variables

extern int start_logging;
extern int gpu_start_log;

// Private function prototypes

void GPUUpdateRegisterBanks(void);

void GPUDumpDisassembly(void);
void GPUDumpRegisters(void);
void GPUDumpMemory(void);

static void gpu_opcode_add(void);
static void gpu_opcode_addc(void);
static void gpu_opcode_addq(void);
static void gpu_opcode_addqt(void);
static void gpu_opcode_sub(void);
static void gpu_opcode_subc(void);
static void gpu_opcode_subq(void);
static void gpu_opcode_subqt(void);
static void gpu_opcode_neg(void);
static void gpu_opcode_and(void);
static void gpu_opcode_or(void);
static void gpu_opcode_xor(void);
static void gpu_opcode_not(void);
static void gpu_opcode_btst(void);
static void gpu_opcode_bset(void);
static void gpu_opcode_bclr(void);
static void gpu_opcode_mult(void);
static void gpu_opcode_imult(void);
static void gpu_opcode_imultn(void);
static void gpu_opcode_resmac(void);
static void gpu_opcode_imacn(void);
static void gpu_opcode_div(void);
static void gpu_opcode_abs(void);
static void gpu_opcode_sh(void);
static void gpu_opcode_shlq(void);
static void gpu_opcode_shrq(void);
static void gpu_opcode_sha(void);
static void gpu_opcode_sharq(void);
static void gpu_opcode_ror(void);
static void gpu_opcode_rorq(void);
static void gpu_opcode_cmp(void);
static void gpu_opcode_cmpq(void);
static void gpu_opcode_sat8(void);
static void gpu_opcode_sat16(void);
static void gpu_opcode_move(void);
static void gpu_opcode_moveq(void);
static void gpu_opcode_moveta(void);
static void gpu_opcode_movefa(void);
static void gpu_opcode_movei(void);
static void gpu_opcode_loadb(void);
static void gpu_opcode_loadw(void);
static void gpu_opcode_load(void);
static void gpu_opcode_loadp(void);
static void gpu_opcode_load_r14_indexed(void);
static void gpu_opcode_load_r15_indexed(void);
static void gpu_opcode_storeb(void);
static void gpu_opcode_storew(void);
static void gpu_opcode_store(void);
static void gpu_opcode_storep(void);
static void gpu_opcode_store_r14_indexed(void);
static void gpu_opcode_store_r15_indexed(void);
static void gpu_opcode_move_pc(void);
static void gpu_opcode_jump(void);
static void gpu_opcode_jr(void);
static void gpu_opcode_mmult(void);
static void gpu_opcode_mtoi(void);
static void gpu_opcode_normi(void);
static void gpu_opcode_nop(void);
static void gpu_opcode_load_r14_ri(void);
static void gpu_opcode_load_r15_ri(void);
static void gpu_opcode_store_r14_ri(void);
static void gpu_opcode_store_r15_ri(void);
static void gpu_opcode_sat24(void);
static void gpu_opcode_pack(void);

// This is wrong, since it doesn't take pipeline effects into account. !!! FIX !!!
/*uint8 gpu_opcode_cycles[64] = 
{
	3,  3,  3,  3,  3,  3,  3,  3,
	3,  3,  3,  3,  3,  3,  3,  3,
	3,  3,  1,  3,  1, 18,  3,  3,
	3,  3,  3,  3,  3,  3,  3,  3,
	3,  3,  2,  2,  2,  2,  3,  4,
	5,  4,  5,  6,  6,  1,  1,  1,
	1,  2,  2,  2,  1,  1,  9,  3,
	3,  1,  6,  6,  2,  2,  3,  3
};//*/
//Here's a QnD kludge...
//This is wrong, wrong, WRONG, but it seems to work for the time being...
//(That is, it fixes Flip Out which relies on GPU timing rather than semaphores. Bad developers! Bad!)
//What's needed here is a way to take pipeline effects into account (including pipeline stalls!)...
uint8 gpu_opcode_cycles[64] = 
{
	1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  9,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  2,
	2,  2,  2,  3,  3,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  4,  1,
	1,  1,  3,  3,  1,  1,  1,  1
};

void (*gpu_opcode[64])()= 
{	
	gpu_opcode_add,					gpu_opcode_addc,				gpu_opcode_addq,				gpu_opcode_addqt,
	gpu_opcode_sub,					gpu_opcode_subc,				gpu_opcode_subq,				gpu_opcode_subqt,
	gpu_opcode_neg,					gpu_opcode_and,					gpu_opcode_or,					gpu_opcode_xor,
	gpu_opcode_not,					gpu_opcode_btst,				gpu_opcode_bset,				gpu_opcode_bclr,
	gpu_opcode_mult,				gpu_opcode_imult,				gpu_opcode_imultn,				gpu_opcode_resmac,
	gpu_opcode_imacn,				gpu_opcode_div,					gpu_opcode_abs,					gpu_opcode_sh,
	gpu_opcode_shlq,				gpu_opcode_shrq,				gpu_opcode_sha,					gpu_opcode_sharq,
	gpu_opcode_ror,					gpu_opcode_rorq,				gpu_opcode_cmp,					gpu_opcode_cmpq,
	gpu_opcode_sat8,				gpu_opcode_sat16,				gpu_opcode_move,				gpu_opcode_moveq,
	gpu_opcode_moveta,				gpu_opcode_movefa,				gpu_opcode_movei,				gpu_opcode_loadb,
	gpu_opcode_loadw,				gpu_opcode_load,				gpu_opcode_loadp,				gpu_opcode_load_r14_indexed,
	gpu_opcode_load_r15_indexed,	gpu_opcode_storeb,				gpu_opcode_storew,				gpu_opcode_store,
	gpu_opcode_storep,				gpu_opcode_store_r14_indexed,	gpu_opcode_store_r15_indexed,	gpu_opcode_move_pc,
	gpu_opcode_jump,				gpu_opcode_jr,					gpu_opcode_mmult,				gpu_opcode_mtoi,
	gpu_opcode_normi,				gpu_opcode_nop,					gpu_opcode_load_r14_ri,			gpu_opcode_load_r15_ri,
	gpu_opcode_store_r14_ri,		gpu_opcode_store_r15_ri,		gpu_opcode_sat24,				gpu_opcode_pack,
};

static uint8 * gpu_ram_8;
uint32 gpu_pc;
static uint32 gpu_acc;
static uint32 gpu_remain;
static uint32 gpu_hidata;
static uint32 gpu_flags;
static uint32 gpu_matrix_control;
static uint32 gpu_pointer_to_matrix;
static uint32 gpu_data_organization;
static uint32 gpu_control;
static uint32 gpu_div_control;
// There is a distinct advantage to having these separated out--there's no need to clear
// a bit before writing a result. I.e., if the result of an operation leaves a zero in
// the carry flag, you don't have to zero gpu_flag_c before you can write that zero!
static uint8 gpu_flag_z, gpu_flag_n, gpu_flag_c;
static uint32 * gpu_reg_bank_0;
static uint32 * gpu_reg_bank_1;
static uint32 * gpu_reg;
static uint32 * gpu_alternate_reg;

static uint32 gpu_instruction;
static uint32 gpu_opcode_first_parameter;
static uint32 gpu_opcode_second_parameter;

#define GPU_RUNNING		(gpu_control & 0x01)

#define RM				gpu_reg[gpu_opcode_first_parameter]
#define RN				gpu_reg[gpu_opcode_second_parameter]
#define ALTERNATE_RM	gpu_alternate_reg[gpu_opcode_first_parameter]
#define ALTERNATE_RN	gpu_alternate_reg[gpu_opcode_second_parameter]
#define IMM_1			gpu_opcode_first_parameter
#define IMM_2			gpu_opcode_second_parameter

#define SET_FLAG_Z(r)	(gpu_flag_z = ((r) == 0));
#define SET_FLAG_N(r)	(gpu_flag_n = (((UINT32)(r) >> 31) & 0x01));

#define RESET_FLAG_Z()	gpu_flag_z = 0;
#define RESET_FLAG_N()	gpu_flag_n = 0;
#define RESET_FLAG_C()	gpu_flag_c = 0;    

#define CLR_Z				(gpu_flag_z = 0)
#define CLR_ZN				(gpu_flag_z = gpu_flag_n = 0)
#define CLR_ZNC				(gpu_flag_z = gpu_flag_n = gpu_flag_c = 0)
#define SET_Z(r)			(gpu_flag_z = ((r) == 0))
#define SET_N(r)			(gpu_flag_n = (((UINT32)(r) >> 31) & 0x01))
#define SET_C_ADD(a,b)		(gpu_flag_c = ((UINT32)(b) > (UINT32)(~(a))))
#define SET_C_SUB(a,b)		(gpu_flag_c = ((UINT32)(b) > (UINT32)(a)))
#define SET_ZN(r)			SET_N(r); SET_Z(r)
#define SET_ZNC_ADD(a,b,r)	SET_N(r); SET_Z(r); SET_C_ADD(a,b)
#define SET_ZNC_SUB(a,b,r)	SET_N(r); SET_Z(r); SET_C_SUB(a,b)

uint32 gpu_convert_zero[32] =
	{ 32,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31 };

uint8 * branch_condition_table = 0;
#define BRANCH_CONDITION(x)	branch_condition_table[(x) + ((jaguar_flags & 7) << 5)]

uint32 gpu_opcode_use[64];

char * gpu_opcode_str[64]= 
{	
	"add",				"addc",				"addq",				"addqt",
	"sub",				"subc",				"subq",				"subqt",
	"neg",				"and",				"or",				"xor",
	"not",				"btst",				"bset",				"bclr",
	"mult",				"imult",			"imultn",			"resmac",
	"imacn",			"div",				"abs",				"sh",
	"shlq",				"shrq",				"sha",				"sharq",
	"ror",				"rorq",				"cmp",				"cmpq",
	"sat8",				"sat16",			"move",				"moveq",
	"moveta",			"movefa",			"movei",			"loadb",
	"loadw",			"load",				"loadp",			"load_r14_indexed",
	"load_r15_indexed",	"storeb",			"storew",			"store",
	"storep",			"store_r14_indexed","store_r15_indexed","move_pc",
	"jump",				"jr",				"mmult",			"mtoi",
	"normi",			"nop",				"load_r14_ri",		"load_r15_ri",
	"store_r14_ri",		"store_r15_ri",		"sat24",			"pack",
};

static uint32 gpu_in_exec = 0;
static uint32 gpu_releaseTimeSlice_flag = 0;

void gpu_releaseTimeslice(void)
{
	gpu_releaseTimeSlice_flag = 1;
}

uint32 gpu_get_pc(void)
{
	return gpu_pc;
}

void build_branch_condition_table(void)
{
	if (!branch_condition_table)
	{
		branch_condition_table = (uint8 *)malloc(32 * 8 * sizeof(branch_condition_table[0]));

		if (branch_condition_table)
		{
			for(int i=0; i<8; i++)
			{
				for(int j=0; j<32; j++)
				{
					int result = 1;
					if (j & 1)
						if (i & ZERO_FLAG)
							result = 0;
					if (j & 2)
						if (!(i & ZERO_FLAG))
							result = 0;
					if (j & 4)
						if (i & (CARRY_FLAG << (j >> 4)))
							result = 0;
					if (j & 8)
						if (!(i & (CARRY_FLAG << (j >> 4))))
							result = 0;
					branch_condition_table[i * 32 + j] = result;
				}
			}
		}
	}
}

//
// GPU byte access (read)
//
uint8 GPUReadByte(uint32 offset, uint32 who/*=UNKNOWN*/)
{
	if (offset >= 0xF02000 && offset <= 0xF020FF)
		WriteLog("GPU: ReadByte--Attempt to read from GPU register file by %s!\n", whoName[who]);

	if ((offset >= GPU_WORK_RAM_BASE) && (offset < GPU_WORK_RAM_BASE+0x1000))
		return gpu_ram_8[offset & 0xFFF];
	else if ((offset >= GPU_CONTROL_RAM_BASE) && (offset < GPU_CONTROL_RAM_BASE+0x20))
	{
		uint32 data = GPUReadLong(offset & 0xFFFFFFFC, who);

		if ((offset & 0x03) == 0)
			return data >> 24;
		else if ((offset & 0x03) == 1)
			return (data >> 16) & 0xFF;
		else if ((offset & 0x03) == 2)
			return (data >> 8) & 0xFF;
		else if ((offset & 0x03) == 3)
			return data & 0xFF;
	}

	return JaguarReadByte(offset, who);
}

//
// GPU word access (read)
//
uint16 GPUReadWord(uint32 offset, uint32 who/*=UNKNOWN*/)
{
	if (offset >= 0xF02000 && offset <= 0xF020FF)
		WriteLog("GPU: ReadWord--Attempt to read from GPU register file by %s!\n", whoName[who]);

	if ((offset >= GPU_WORK_RAM_BASE) && (offset < GPU_WORK_RAM_BASE+0x1000))
	{
		offset &= 0xFFF;
		uint16 data = ((uint16)gpu_ram_8[offset] << 8) | (uint16)gpu_ram_8[offset+1];
		return data;
	}
	else if ((offset >= GPU_CONTROL_RAM_BASE) && (offset < GPU_CONTROL_RAM_BASE+0x20))
	{
// This looks and smells wrong...
// But it *might* be OK...
		if (offset & 0x01)			// Catch cases 1 & 3... (unaligned read)
			return (GPUReadByte(offset, who) << 8) | GPUReadByte(offset+1, who);

		uint32 data = GPUReadLong(offset & 0xFFFFFFFC, who);

		if (offset & 0x02)			// Cases 0 & 2...
			return data & 0xFFFF;
		else
			return data >> 16;
	}

//TEMP--Mirror of F03000? No. Writes only...
//if (offset >= 0xF0B000 && offset <= 0xF0BFFF)
//WriteLog("[GPUR16] --> Possible GPU RAM mirror access by %s!", whoName[who]);

	return JaguarReadWord(offset, who);
}

//
// GPU dword access (read)
//
uint32 GPUReadLong(uint32 offset, uint32 who/*=UNKNOWN*/)
{
	if (offset >= 0xF02000 && offset <= 0xF020FF)
		WriteLog("GPU: ReadLong--Attempt to read from GPU register file by %s!\n", whoName[who]);

//	if ((offset >= GPU_WORK_RAM_BASE) && (offset < GPU_WORK_RAM_BASE + 0x1000))
	if ((offset >= GPU_WORK_RAM_BASE) && (offset <= GPU_WORK_RAM_BASE + 0x0FFC))
	{
		offset &= 0xFFF;
		return ((uint32)gpu_ram_8[offset] << 24) | ((uint32)gpu_ram_8[offset+1] << 16)
			| ((uint32)gpu_ram_8[offset+2] << 8) | (uint32)gpu_ram_8[offset+3];//*/
//		return GET32(gpu_ram_8, offset);
	}
//	else if ((offset >= GPU_CONTROL_RAM_BASE) && (offset < GPU_CONTROL_RAM_BASE+0x20))
	else if ((offset >= GPU_CONTROL_RAM_BASE) && (offset <= GPU_CONTROL_RAM_BASE + 0x1C))
	{
		offset &= 0x1F;
		switch (offset)
		{
		case 0x00:
			gpu_flag_c = (gpu_flag_c ? 1 : 0);
			gpu_flag_z = (gpu_flag_z ? 1 : 0);
			gpu_flag_n = (gpu_flag_n ? 1 : 0);

			gpu_flags = (gpu_flags & 0xFFFFFFF8) | (gpu_flag_n << 2) | (gpu_flag_c << 1) | gpu_flag_z;
					
			return gpu_flags & 0xFFFFC1FF;
		case 0x04:
			return gpu_matrix_control;
		case 0x08:
			return gpu_pointer_to_matrix;
		case 0x0C:
			return gpu_data_organization;
		case 0x10:
			return gpu_pc;
		case 0x14:
			return gpu_control;
		case 0x18:
			return gpu_hidata;
		case 0x1C:
			return gpu_remain;
		default:								// unaligned long read
#ifdef GPU_DEBUG
			WriteLog("GPU: Read32--unaligned 32 bit read at %08X by %s.\n", GPU_CONTROL_RAM_BASE + offset, whoName[who]);
#endif	// GPU_DEBUG
			return 0;
		}
	}
//TEMP--Mirror of F03000? No. Writes only...
//if (offset >= 0xF0B000 && offset <= 0xF0BFFF)
//	WriteLog("[GPUR32] --> Possible GPU RAM mirror access by %s!\n", whoName[who]);
/*if (offset >= 0xF1D000 && offset <= 0xF1DFFF)
	WriteLog("[GPUR32] --> Reading from Wavetable ROM!\n");//*/

	return (JaguarReadWord(offset, who) << 16) | JaguarReadWord(offset + 2, who);
}

//
// GPU byte access (write)
//
void GPUWriteByte(uint32 offset, uint8 data, uint32 who/*=UNKNOWN*/)
{
	if (offset >= 0xF02000 && offset <= 0xF020FF)
		WriteLog("GPU: WriteByte--Attempt to write to GPU register file by %s!\n", whoName[who]);

	if ((offset >= GPU_WORK_RAM_BASE) && (offset <= GPU_WORK_RAM_BASE + 0x0FFF))
	{
		gpu_ram_8[offset & 0xFFF] = data;

//This is the same stupid worthless code that was in the DSP!!! AARRRGGGGHHHHH!!!!!!
/*		if (!gpu_in_exec)
		{
			m68k_end_timeslice();
			dsp_releaseTimeslice();
		}*/
		return;
	}
	else if ((offset >= GPU_CONTROL_RAM_BASE) && (offset <= GPU_CONTROL_RAM_BASE + 0x1F))
	{
		uint32 reg = offset & 0x1C;
		int bytenum = offset & 0x03;

//This is definitely wrong!
		if ((reg >= 0x1C) && (reg <= 0x1F))
			gpu_div_control = (gpu_div_control & (~(0xFF << (bytenum << 3)))) | (data << (bytenum << 3));
		else
		{
			uint32 old_data = GPUReadLong(offset & 0xFFFFFFC, who);
			bytenum = 3 - bytenum; // convention motorola !!!
			old_data = (old_data & (~(0xFF << (bytenum << 3)))) | (data << (bytenum << 3));
			GPUWriteLong(offset & 0xFFFFFFC, old_data, who);
		}
		return;
	}
//	WriteLog("gpu: writing %.2x at 0x%.8x\n",data,offset);
	JaguarWriteByte(offset, data, who);
}

//
// GPU word access (write)
//
void GPUWriteWord(uint32 offset, uint16 data, uint32 who/*=UNKNOWN*/)
{
	if (offset >= 0xF02000 && offset <= 0xF020FF)
		WriteLog("GPU: WriteWord--Attempt to write to GPU register file by %s!\n", whoName[who]);

	if ((offset >= GPU_WORK_RAM_BASE) && (offset <= GPU_WORK_RAM_BASE + 0x0FFE))
	{
		gpu_ram_8[offset & 0xFFF] = (data>>8) & 0xFF;
		gpu_ram_8[(offset+1) & 0xFFF] = data & 0xFF;//*/
/*		offset &= 0xFFF;
		SET16(gpu_ram_8, offset, data);//*/

/*if (offset >= 0xF03214 && offset < 0xF0321F)
	WriteLog("GPU: Writing WORD (%04X) to GPU RAM (%08X)...\n", data, offset);//*/


//This is the same stupid worthless code that was in the DSP!!! AARRRGGGGHHHHH!!!!!!
/*		if (!gpu_in_exec)
		{
			m68k_end_timeslice();
			dsp_releaseTimeslice();
		}*/
		return;
	}
	else if ((offset >= GPU_CONTROL_RAM_BASE) && (offset <= GPU_CONTROL_RAM_BASE + 0x1E))
	{
		if (offset & 0x01)		// This is supposed to weed out unaligned writes, but does nothing...
		{
#ifdef GPU_DEBUG
			WriteLog("GPU: Write16--unaligned write @ %08X [%04X]\n", offset, data);
			GPUDumpRegisters();
#endif	// GPU_DEBUG
			return;
		}
//Dual locations in this range: $1C Divide unit remainder/Divide unit control (R/W)
//This just literally sucks.
		if ((offset & 0x1C) == 0x1C)
		{
//This doesn't look right either--handles cases 1, 2, & 3 all the same!
			if (offset & 0x02)
				gpu_div_control = (gpu_div_control & 0xFFFF0000) | (data & 0xFFFF);
			else
				gpu_div_control = (gpu_div_control & 0x0000FFFF) | ((data & 0xFFFF) << 16);
		}
		else 
		{
//WriteLog("[GPU W16:%08X,%04X]", offset, data);
			uint32 old_data = GPUReadLong(offset & 0xFFFFFFC, who);
			if (offset & 0x02)
				old_data = (old_data & 0xFFFF0000) | (data & 0xFFFF);
			else
				old_data = (old_data & 0x0000FFFF) | ((data & 0xFFFF) << 16);
			GPUWriteLong(offset & 0xFFFFFFC, old_data, who);
		}
		return;
	}
	else if ((offset == GPU_WORK_RAM_BASE + 0x0FFF) || (GPU_CONTROL_RAM_BASE + 0x1F))
	{
#ifdef GPU_DEBUG
			WriteLog("GPU: Write16--unaligned write @ %08X by %s [%04X]!\n", offset, whoName[who], data);
			GPUDumpRegisters();
#endif	// GPU_DEBUG
		return;
	}

	// Have to be careful here--this can cause an infinite loop!
	JaguarWriteWord(offset, data, who);
}

//
// GPU dword access (write)
//
void GPUWriteLong(uint32 offset, uint32 data, uint32 who/*=UNKNOWN*/)
{
	if (offset >= 0xF02000 && offset <= 0xF020FF)
		WriteLog("GPU: WriteLong--Attempt to write to GPU register file by %s!\n", whoName[who]);

//	if ((offset >= GPU_WORK_RAM_BASE) && (offset < GPU_WORK_RAM_BASE + 0x1000))
	if ((offset >= GPU_WORK_RAM_BASE) && (offset <= GPU_WORK_RAM_BASE + 0x0FFC))
	{
#ifdef GPU_DEBUG
		if (offset & 0x03)
		{
			WriteLog("GPU: Write32--unaligned write @ %08X [%08X] by %s\n", offset, data, whoName[who]);
			GPUDumpRegisters();
		}
#endif	// GPU_DEBUG

		offset &= 0xFFF;
		SET32(gpu_ram_8, offset, data);
		return;
	}
//	else if ((offset >= GPU_CONTROL_RAM_BASE) && (offset < GPU_CONTROL_RAM_BASE+0x20))
	else if ((offset >= GPU_CONTROL_RAM_BASE) && (offset <= GPU_CONTROL_RAM_BASE + 0x1C))
	{
		offset &= 0x1F;
		switch (offset)
		{
		case 0x00:
		{
			bool IMASKCleared = (gpu_flags & IMASK) && !(data & IMASK);
			gpu_flags = data;
			gpu_flag_z = gpu_flags & ZERO_FLAG;
			gpu_flag_c = (gpu_flags & CARRY_FLAG) >> 1;
			gpu_flag_n = (gpu_flags & NEGA_FLAG) >> 2;
			GPUUpdateRegisterBanks();
			gpu_control &= ~((gpu_flags & CINT04FLAGS) >> 3);	// Interrupt latch clear bits
//Writing here is only an interrupt enable--this approach is just plain wrong!
//			GPUHandleIRQs();
//This, however, is A-OK! ;-)
			if (IMASKCleared)						// If IMASK was cleared,
				GPUHandleIRQs();					// see if any other interrupts need servicing!
#ifdef GPU_DEBUG
			if (gpu_flags & (INT_ENA0 | INT_ENA1 | INT_ENA2 | INT_ENA3 | INT_ENA4))
				WriteLog("GPU: Interrupt enable set by %s! Bits: %02X\n", whoName[who], (gpu_flags >> 4) & 0x1F);
			WriteLog("GPU: REGPAGE %s...\n", (gpu_flags & REGPAGE ? "set" : "cleared"));
#endif	// GPU_DEBUG
			break;
		}
		case 0x04:
			gpu_matrix_control = data;
			break;
		case 0x08:
			// This can only point to long aligned addresses
			gpu_pointer_to_matrix = data & 0xFFFFFFFC;
			break;
		case 0x0C:
			gpu_data_organization = data;
			break;
		case 0x10:
			gpu_pc = data;
#ifdef GPU_DEBUG
WriteLog("GPU: %s setting GPU PC to %08X %s\n", whoName[who], gpu_pc, (GPU_RUNNING ? "(GPU is RUNNING!)" : ""));//*/
#endif	// GPU_DEBUG
			break;
		case 0x14:
		{	
//			uint32 gpu_was_running = GPU_RUNNING;
			data &= ~0xF7C0;		// Disable writes to INT_LAT0-4 & TOM version number

			// check for GPU -> CPU interrupt
			if (data & 0x02)
			{
//WriteLog("GPU->CPU interrupt\n");
				if (tom_irq_enabled(IRQ_GPU))
				{
					if ((tom_irq_enabled(IRQ_GPU)) && (jaguar_interrupt_handler_is_valid(64)))
					{
						tom_set_pending_gpu_int();
						m68k_set_irq(7);			// Set 68000 NMI
						gpu_releaseTimeslice();
					}
				}
				data &= ~0x02;
			}

			// check for CPU -> GPU interrupt #0
			if (data & 0x04)
			{
//WriteLog("CPU->GPU interrupt\n");
				GPUSetIRQLine(0, ASSERT_LINE);
				m68k_end_timeslice();
				dsp_releaseTimeslice();
				data &= ~0x04;
			}

			// single stepping
			if (data & 0x10)
			{
				//WriteLog("asked to perform a single step (single step is %senabled)\n",(data&0x8)?"":"not ");
			}
			gpu_control = (gpu_control & 0xF7C0) | (data & (~0xF7C0));

			// if gpu wasn't running but is now running, execute a few cycles
#ifndef GPU_SINGLE_STEPPING
/*			if (!gpu_was_running && GPU_RUNNING)
#ifdef GPU_DEBUG
			{
				WriteLog("GPU: Write32--About to do stupid braindead GPU execution for 200 cycles.\n");
#endif	// GPU_DEBUG
				gpu_exec(200);
#ifdef GPU_DEBUG
			}
#endif	// GPU_DEBUG//*/
#else
			if (gpu_control & 0x18)
				gpu_exec(1);
#endif	// #ifndef GPU_SINGLE_STEPPING
#ifdef GPU_DEBUG
WriteLog("Write to GPU CTRL by %s: %08X ", whoName[who], data);
if (GPU_RUNNING)
	WriteLog(" --> Starting to run at %08X by %s...", gpu_pc, whoName[who]);
else
	WriteLog(" --> Stopped by %s! (GPU_PC: %08X)", whoName[who], gpu_pc);
WriteLog("\n");
#endif	// GPU_DEBUG
//if (GPU_RUNNING)
//	GPUDumpDisassembly();
/*if (GPU_RUNNING)
{
	if (gpu_pc == 0xF035D8)
	{
//		GPUDumpDisassembly();
//		log_done();
//		exit(1);
		gpu_control &= 0xFFFFFFFE;	// Don't run it and let's see what happens!
//Hmm. Seems to lock up when going into the demo...
//Try to disable the collision altogether!
	}
}//*/
extern int effect_start5;
static bool finished = false;
//if (GPU_RUNNING && effect_start5 && !finished)
if (GPU_RUNNING && effect_start5 && gpu_pc == 0xF035D8)
{
	// Let's do a dump of $6528!
/*	uint32 numItems = JaguarReadWord(0x6BD6);
	WriteLog("\nDump of $6528: %u items.\n\n", numItems);
	for(int i=0; i<numItems*3*4; i+=3*4)
	{
		WriteLog("\t%04X: %08X %08X %08X -> ", 0x6528+i, JaguarReadLong(0x6528+i),
			JaguarReadLong(0x6528+i+4), JaguarReadLong(0x6528+i+8));
		uint16 link = JaguarReadWord(0x6528+i+8+2);
		for(int j=0; j<40; j+=4)
			WriteLog("%08X ", JaguarReadLong(link + j));
		WriteLog("\n");
	}
	WriteLog("\n");//*/
	// Let's try a manual blit here...
//This isn't working the way it should! !!! FIX !!!
//Err, actually, it is.
// NOW, it works right! Problem solved!!! It's a blitter bug!
/*	uint32 src = 0x4D54, dst = 0xF03000, width = 10 * 4;
	for(int y=0; y<127; y++)
	{
		for(int x=0; x<2; x++)
		{
			JaguarWriteLong(dst, JaguarReadLong(src));
			
			src += 4;
			dst += 4;
		}
		src += width - (2 * 4);
	}//*/
/*	finished = true;
	doGPUDis = true;
	WriteLog("\nGPU: About to execute collision detection code.\n\n");//*/

/*	WriteLog("\nGPU: About to execute collision detection code. Data @ 4D54:\n\n");
	int count = 0;
	for(int i=0x004D54; i<0x004D54+2048; i++)
	{
		WriteLog("%02X ", JaguarReadByte(i));
		count++;
		if (count == 32)
		{
			count = 0;
			WriteLog("\n");
		}
	}
	WriteLog("\n\nData @ F03000:\n\n");
	count = 0;
	for(int i=0xF03000; i<0xF03200; i++)
	{
		WriteLog("%02X ", JaguarReadByte(i));
		count++;
		if (count == 32)
		{
			count = 0;
			WriteLog("\n");
		}
	}
	WriteLog("\n\n");
	log_done();
	exit(0);//*/
}
//if (!GPU_RUNNING)
//	doGPUDis = false;
/*if (!GPU_RUNNING && finished)
{
	WriteLog("\nGPU: Finished collision detection code. Exiting!\n\n");
	GPUDumpRegisters();
	log_done();
	exit(0);
}//*/
			// (?) If we're set running by the M68K (or DSP?) then end its timeslice to
			// allow the GPU a chance to run...
			// Yes! This partially fixed Trevor McFur...
			if (GPU_RUNNING)
				m68k_end_timeslice();
			break;
		}
		case 0x18:
			gpu_hidata = data;
			break;
		case 0x1C:
			gpu_div_control = data;
			break;
//		default:   // unaligned long write
			//exit(0);
			//__asm int 3
		}
		return;
	}

//	JaguarWriteWord(offset, (data >> 16) & 0xFFFF, who);
//	JaguarWriteWord(offset+2, data & 0xFFFF, who);
// We're a 32-bit processor, we can do a long write...!
	JaguarWriteLong(offset, data, who);
}

//
// Change register banks if necessary
//
void GPUUpdateRegisterBanks(void)
{
	int bank = (gpu_flags & REGPAGE);		// REGPAGE bit

	if (gpu_flags & IMASK)					// IMASK bit
		bank = 0;							// IMASK forces main bank to be bank 0

	if (bank)
		gpu_reg = gpu_reg_bank_1, gpu_alternate_reg = gpu_reg_bank_0;
	else
		gpu_reg = gpu_reg_bank_0, gpu_alternate_reg = gpu_reg_bank_1;
}

void GPUHandleIRQs(void)
{
	// Bail out if we're already in an interrupt!
	if (gpu_flags & IMASK)
		return;

	// Get the interrupt latch & enable bits
	uint32 bits = (gpu_control >> 6) & 0x1F, mask = (gpu_flags >> 4) & 0x1F;
	
	// Bail out if latched interrupts aren't enabled
	bits &= mask;
	if (!bits)
		return;
	
	// Determine which interrupt to service
	uint32 which = 0; //Isn't there a #pragma to disable this warning???
	if (bits & 0x01)
		which = 0;
	if (bits & 0x02)
		which = 1;
	if (bits & 0x04)
		which = 2;
	if (bits & 0x08)
		which = 3;
	if (bits & 0x10)
		which = 4;

	if (start_logging)
		WriteLog("GPU: Generating IRQ #%i\n", which);

	// set the interrupt flag 
	gpu_flags |= IMASK;
	GPUUpdateRegisterBanks();

	// subqt  #4,r31		; pre-decrement stack pointer 
	// move  pc,r30			; address of interrupted code 
	// store  r30,(r31)     ; store return address
	gpu_reg[31] -= 4;
	GPUWriteLong(gpu_reg[31], gpu_pc - 2, GPU);
	
	// movei  #service_address,r30  ; pointer to ISR entry 
	// jump  (r30)					; jump to ISR 
	// nop
	gpu_pc = gpu_reg[30] = GPU_WORK_RAM_BASE + (which * 0x10);
}

void GPUSetIRQLine(int irqline, int state)
{
	if (start_logging)
		WriteLog("GPU: Setting GPU IRQ line #%i\n", irqline);

	uint32 mask = 0x0040 << irqline;
	gpu_control &= ~mask;				// Clear the interrupt latch

	if (state)
	{
		gpu_control |= mask;			// Assert the interrupt latch
		GPUHandleIRQs();				// And handle the interrupt...
	}
}

//TEMPORARY: Testing only!
//#include "gpu2.h"
//#include "gpu3.h"

void gpu_init(void)
{
	memory_malloc_secure((void **)&gpu_ram_8, 0x1000, "GPU work RAM");
	memory_malloc_secure((void **)&gpu_reg_bank_0, 32 * sizeof(int32), "GPU bank 0 regs");
	memory_malloc_secure((void **)&gpu_reg_bank_1, 32 * sizeof(int32), "GPU bank 1 regs");

	build_branch_condition_table();

	gpu_reset();

//TEMPORARY: Testing only!
//	gpu2_init();
//	gpu3_init();
}

void gpu_reset(void)
{
	// GPU registers (directly visible)
	gpu_flags			  = 0x00000000;
	gpu_matrix_control    = 0x00000000;
	gpu_pointer_to_matrix = 0x00000000;
	gpu_data_organization = 0xFFFFFFFF;
	gpu_pc				  = 0x00F03000;
	gpu_control			  = 0x00002800;			// Correctly sets this as TOM Rev. 2
	gpu_hidata			  = 0x00000000;
	gpu_remain			  = 0x00000000;			// These two registers are RO/WO
	gpu_div_control		  = 0x00000000;

	// GPU internal register
	gpu_acc				  = 0x00000000;

	gpu_reg = gpu_reg_bank_0;
	gpu_alternate_reg = gpu_reg_bank_1;

	for(int i=0; i<32; i++)
		gpu_reg[i] = gpu_alternate_reg[i] = 0x00000000;

	CLR_ZNC;
	memset(gpu_ram_8, 0xFF, 0x1000);
	gpu_in_exec = 0;
//not needed	GPUInterruptPending = false;
	gpu_reset_stats();
}

uint32 gpu_read_pc(void)
{
	return gpu_pc;
}

void gpu_reset_stats(void)
{
	for(uint32 i=0; i<64; i++)
		gpu_opcode_use[i] = 0;
	WriteLog("--> GPU stats were reset!\n");
}

void GPUDumpDisassembly(void)
{
	char buffer[512];

	WriteLog("\n---[GPU code at 00F03000]---------------------------\n");
	uint32 j = 0xF03000;
	while (j <= 0xF03FFF)
	{
		uint32 oldj = j;
		j += dasmjag(JAGUAR_GPU, buffer, j);
		WriteLog("\t%08X: %s\n", oldj, buffer);
	}
}

void GPUDumpRegisters(void)
{
	WriteLog("\n---[GPU flags: NCZ %d%d%d]-----------------------\n", gpu_flag_n, gpu_flag_c, gpu_flag_z);
	WriteLog("\nRegisters bank 0\n");
	for(int j=0; j<8; j++)
	{
		WriteLog("\tR%02i = %08X R%02i = %08X R%02i = %08X R%02i = %08X\n",
						  (j << 2) + 0, gpu_reg_bank_0[(j << 2) + 0],
						  (j << 2) + 1, gpu_reg_bank_0[(j << 2) + 1],
						  (j << 2) + 2, gpu_reg_bank_0[(j << 2) + 2],
						  (j << 2) + 3, gpu_reg_bank_0[(j << 2) + 3]);
	}
	WriteLog("Registers bank 1\n");
	for(int j=0; j<8; j++)
	{
		WriteLog("\tR%02i = %08X R%02i = %08X R%02i = %08X R%02i = %08X\n",
						  (j << 2) + 0, gpu_reg_bank_1[(j << 2) + 0],
						  (j << 2) + 1, gpu_reg_bank_1[(j << 2) + 1],
						  (j << 2) + 2, gpu_reg_bank_1[(j << 2) + 2],
						  (j << 2) + 3, gpu_reg_bank_1[(j << 2) + 3]);
	}
}

void GPUDumpMemory(void)
{
	WriteLog("\n---[GPU data at 00F03000]---------------------------\n");
	for(int i=0; i<0xFFF; i+=4)
		WriteLog("\t%08X: %02X %02X %02X %02X\n", 0xF03000+i, gpu_ram_8[i],
			gpu_ram_8[i+1], gpu_ram_8[i+2], gpu_ram_8[i+3]);
}

void gpu_done(void)
{ 
	WriteLog("GPU: Stopped at PC=%08X (GPU %s running)\n", (unsigned int)gpu_pc, GPU_RUNNING ? "was" : "wasn't");

	// Get the interrupt latch & enable bits 
	uint8 bits = (gpu_control >> 6) & 0x1F, mask = (gpu_flags >> 4) & 0x1F;
	WriteLog("GPU: Latch bits = %02X, enable bits = %02X\n", bits, mask);

	GPUDumpRegisters();
	GPUDumpDisassembly();

	WriteLog("\nGPU opcodes use:\n");
	for(int i=0; i<64; i++)
	{
		if (gpu_opcode_use[i])
			WriteLog("\t%17s %lu\n", gpu_opcode_str[i], gpu_opcode_use[i]);
	}
	WriteLog("\n");

	memory_free(gpu_ram_8);
	memory_free(gpu_reg_bank_0);
	memory_free(gpu_reg_bank_1);
}

//
// Main GPU execution core
//
static int testCount = 1;
static int len = 0;
static bool tripwire = false;
void gpu_exec(int32 cycles)
{
	if (!GPU_RUNNING)
		return;

#ifdef GPU_SINGLE_STEPPING
	if (gpu_control & 0x18)
	{
		cycles = 1;
		gpu_control &= ~0x10;
	}
#endif
	GPUHandleIRQs();
	gpu_releaseTimeSlice_flag = 0;
	gpu_in_exec++;

	while (cycles > 0 && GPU_RUNNING)
	{
if (gpu_ram_8[0x054] == 0x98 && gpu_ram_8[0x055] == 0x0A && gpu_ram_8[0x056] == 0x03
	&& gpu_ram_8[0x057] == 0x00 && gpu_ram_8[0x058] == 0x00 && gpu_ram_8[0x059] == 0x00)
{
	if (gpu_pc == 0xF03000)
	{
		extern uint32 starCount;
		starCount = 0;
/*		WriteLog("GPU: Starting starfield generator... Dump of [R03=%08X]:\n", gpu_reg_bank_0[03]);
		uint32 base = gpu_reg_bank_0[3];
		for(uint32 i=0; i<0x100; i+=16)
		{
			WriteLog("%02X: ", i);
			for(uint32 j=0; j<16; j++)
			{
				WriteLog("%02X ", JaguarReadByte(base + i + j));
			}
			WriteLog("\n");
		}*/
	}
//	if (gpu_pc == 0xF03)
	{
	}
}//*/
/*if (gpu_pc == 0xF03B9E && gpu_reg_bank_0[01] == 0)
{
	GPUDumpRegisters();
	WriteLog("GPU: Starting disassembly log...\n");
	doGPUDis = true;
}//*/
/*if (gpu_pc == 0xF0359A)
{
	doGPUDis = true;
	GPUDumpRegisters();
}*/
/*		gpu_flag_c = (gpu_flag_c ? 1 : 0);
		gpu_flag_z = (gpu_flag_z ? 1 : 0);
		gpu_flag_n = (gpu_flag_n ? 1 : 0);*/
	
		uint16 opcode = GPUReadWord(gpu_pc, GPU);
		uint32 index = opcode >> 10;
		gpu_instruction = opcode;				// Added for GPU #3...
		gpu_opcode_first_parameter = (opcode >> 5) & 0x1F;
		gpu_opcode_second_parameter = opcode & 0x1F;
/*if (gpu_pc == 0xF03BE8)
WriteLog("Start of OP frame write...\n");
if (gpu_pc == 0xF03EEE)
WriteLog("--> Writing BRANCH object ---\n");
if (gpu_pc == 0xF03F62)
WriteLog("--> Writing BITMAP object ***\n");//*/
/*if (gpu_pc == 0xF03546)
{
	WriteLog("\n--> GPU PC: F03546\n");
	GPUDumpRegisters();
	GPUDumpDisassembly();
}//*/
/*if (gpu_pc == 0xF033F6)
{
	WriteLog("\n--> GPU PC: F033F6\n");
	GPUDumpRegisters();
	GPUDumpDisassembly();
}//*/
/*if (gpu_pc == 0xF033CC)
{
	WriteLog("\n--> GPU PC: F033CC\n");
	GPUDumpRegisters();
	GPUDumpDisassembly();
}//*/
/*if (gpu_pc == 0xF033D6)
{
	WriteLog("\n--> GPU PC: F033D6 (#%d)\n", testCount++);
	GPUDumpRegisters();
	GPUDumpMemory();
}//*/
/*if (gpu_pc == 0xF033D8)
{
	WriteLog("\n--> GPU PC: F033D8 (#%d)\n", testCount++);
	GPUDumpRegisters();
	GPUDumpMemory();
}//*/
/*if (gpu_pc == 0xF0358E)
{
	WriteLog("\n--> GPU PC: F0358E (#%d)\n", testCount++);
	GPUDumpRegisters();
	GPUDumpMemory();
}//*/
/*if (gpu_pc == 0xF034CA)
{
	WriteLog("\n--> GPU PC: F034CA (#%d)\n", testCount++);
	GPUDumpRegisters();
}//*/
/*if (gpu_pc == 0xF034CA)
{
	len = gpu_reg[1] + 4;//, r9save = gpu_reg[9];
	WriteLog("\nAbout to subtract [#%d] (R14=%08X, R15=%08X, R9=%08X):\n   ", testCount++, gpu_reg[14], gpu_reg[15], gpu_reg[9]);
	for(int i=0; i<len; i+=4)
		WriteLog(" %08X", GPUReadLong(gpu_reg[15]+i));
	WriteLog("\n   ");
	for(int i=0; i<len; i+=4)
		WriteLog(" %08X", GPUReadLong(gpu_reg[14]+i));
	WriteLog("\n\n");
}
if (gpu_pc == 0xF034DE)
{
	WriteLog("\nSubtracted! (R14=%08X, R15=%08X):\n   ", gpu_reg[14], gpu_reg[15]);
	for(int i=0; i<len; i+=4)
		WriteLog(" %08X", GPUReadLong(gpu_reg[15]+i));
	WriteLog("\n   ");
	for(int i=0; i<len; i+=4)
		WriteLog(" %08X", GPUReadLong(gpu_reg[14]+i));
	WriteLog("\n   ");
	for(int i=0; i<len; i+=4)
		WriteLog(" --------");
	WriteLog("\n   ");
	for(int i=0; i<len; i+=4)
		WriteLog(" %08X", GPUReadLong(gpu_reg[9]+4+i));
	WriteLog("\n\n");
}//*/
/*if (gpu_pc == 0xF035C8)
{
	WriteLog("\n--> GPU PC: F035C8 (#%d)\n", testCount++);
	GPUDumpRegisters();
	GPUDumpDisassembly();
}//*/

if (gpu_start_log)
{
//	gpu_reset_stats();
static char buffer[512];
dasmjag(JAGUAR_GPU, buffer, gpu_pc);
WriteLog("GPU: [%08X] %s (RM=%08X, RN=%08X) -> ", gpu_pc, buffer, RM, RN);
}//*/
//$E400 -> 1110 01 -> $39 -> 57
//GPU #1
		gpu_pc += 2;
		gpu_opcode[index]();
//GPU #2
//		gpu2_opcode[index]();
//		gpu_pc += 2;
//GPU #3				(Doesn't show ATARI logo! #1 & #2 do...)
//		gpu_pc += 2;
//		gpu3_opcode[index]();

// BIOS hacking
//GPU: [00F03548] jr      nz,00F03560 (0xd561) (RM=00F03114, RN=00000004) ->     --> JR: Branch taken. 
/*static bool firstTime = true;
if (gpu_pc == 0xF03548 && firstTime)
{
	gpu_flag_z = 1;
//	firstTime = false;

//static char buffer[512];
//int k=0xF03548;
//while (k<0xF0356C)
//{
//int oldk = k;
//k += dasmjag(JAGUAR_GPU, buffer, k);
//WriteLog("GPU: [%08X] %s\n", oldk, buffer);
//}
//	gpu_start_log = 1;
}//*/
//GPU: [00F0354C] jump    nz,(r29) (0xd3a1) (RM=00F03314, RN=00000004) -> (RM=00F03314, RN=00000004)
/*if (gpu_pc == 0xF0354C)
	gpu_flag_z = 0;//, gpu_start_log = 1;//*/

		cycles -= gpu_opcode_cycles[index];
		gpu_opcode_use[index]++;
if (gpu_start_log)
	WriteLog("(RM=%08X, RN=%08X)\n", RM, RN);//*/
if ((gpu_pc < 0xF03000 || gpu_pc > 0xF03FFF) && !tripwire)
{
	WriteLog("GPU: Executing outside local RAM! GPU_PC: %08X\n", gpu_pc);
	tripwire = true;
}
	}

	gpu_in_exec--;
}

//
// GPU opcodes
//

/*
GPU opcodes use (offset punch--vertically below bad guy):
	              add 18686
	             addq 32621
	              sub 7483
	             subq 10252
	              and 21229
	               or 15003
	             btst 1822
	             bset 2072
	             mult 141
	              div 2392
	             shlq 13449
	             shrq 10297
	            sharq 11104
	              cmp 6775
	             cmpq 5944
	             move 31259
	            moveq 4473
	            movei 23277
	            loadb 46
	            loadw 4201
	             load 28580
	 load_r14_indexed 1183
	 load_r15_indexed 1125
	           storew 178
	            store 10144
	store_r14_indexed 320
	store_r15_indexed 1
	          move_pc 1742
	             jump 24467
	               jr 18090
	              nop 41362
*/

static void gpu_opcode_jump(void)
{
#ifdef GPU_DIS_JUMP
char * condition[32] =
{	"T", "nz", "z", "???", "nc", "nc nz", "nc z", "???", "c", "c nz",
	"c z", "???", "???", "???", "???", "???", "???", "???", "???",
	"???", "nn", "nn nz", "nn z", "???", "n", "n nz", "n z", "???",
	"???", "???", "???", "F" };
	if (doGPUDis)
		WriteLog("%06X: JUMP   %s, (R%02u) [NCZ:%u%u%u, R%02u=%08X] ", gpu_pc-2, condition[IMM_2], IMM_1, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM);
#endif
	// normalize flags
/*	gpu_flag_c = (gpu_flag_c ? 1 : 0);
	gpu_flag_z = (gpu_flag_z ? 1 : 0);
	gpu_flag_n = (gpu_flag_n ? 1 : 0);*/
	// KLUDGE: Used by BRANCH_CONDITION
	uint32 jaguar_flags = (gpu_flag_n << 2) | (gpu_flag_c << 1) | gpu_flag_z;

	if (BRANCH_CONDITION(IMM_2))
	{
#ifdef GPU_DIS_JUMP
	if (doGPUDis)
		WriteLog("Branched!\n");
#endif
if (gpu_start_log)
	WriteLog("    --> JUMP: Branch taken.\n");
		uint32 delayed_pc = RM;
		gpu_exec(1);
		gpu_pc = delayed_pc;
/*		uint16 opcode = GPUReadWord(gpu_pc, GPU);
		gpu_opcode_first_parameter = (opcode >> 5) & 0x1F;
		gpu_opcode_second_parameter = opcode & 0x1F;

		gpu_pc = delayed_pc;
		gpu_opcode[opcode>>10]();//*/
	}
#ifdef GPU_DIS_JUMP
	else
		if (doGPUDis)
			WriteLog("Branch NOT taken.\n");
#endif
}

static void gpu_opcode_jr(void)
{
#ifdef GPU_DIS_JR
char * condition[32] =
{	"T", "nz", "z", "???", "nc", "nc nz", "nc z", "???", "c", "c nz",
	"c z", "???", "???", "???", "???", "???", "???", "???", "???",
	"???", "nn", "nn nz", "nn z", "???", "n", "n nz", "n z", "???",
	"???", "???", "???", "F" };
	if (doGPUDis)
		WriteLog("%06X: JR     %s, %06X [NCZ:%u%u%u] ", gpu_pc-2, condition[IMM_2], gpu_pc+((IMM_1 & 0x10 ? 0xFFFFFFF0 | IMM_1 : IMM_1) * 2), gpu_flag_n, gpu_flag_c, gpu_flag_z);
#endif
/*	if (CONDITION(jaguar.op & 31))
	{
		INT32 r1 = (INT8)((jaguar.op >> 2) & 0xF8) >> 2;
		UINT32 newpc = jaguar.PC + r1;
		CALL_MAME_DEBUG;
		jaguar.op = ROPCODE(jaguar.PC);
		jaguar.PC = newpc;
		(*jaguar.table[jaguar.op >> 10])();

		jaguar_icount -= 3;	// 3 wait states guaranteed
	}*/
	// normalize flags
/*	gpu_flag_n = (gpu_flag_n ? 1 : 0);
	gpu_flag_c = (gpu_flag_c ? 1 : 0);
	gpu_flag_z = (gpu_flag_z ? 1 : 0);*/
	// KLUDGE: Used by BRANCH_CONDITION
	uint32 jaguar_flags = (gpu_flag_n << 2) | (gpu_flag_c << 1) | gpu_flag_z;

	if (BRANCH_CONDITION(IMM_2))
	{
#ifdef GPU_DIS_JR
	if (doGPUDis)
		WriteLog("Branched!\n");
#endif
if (gpu_start_log)
	WriteLog("    --> JR: Branch taken.\n");
		int32 offset = (IMM_1 & 0x10 ? 0xFFFFFFF0 | IMM_1 : IMM_1);		// Sign extend IMM_1
		int32 delayed_pc = gpu_pc + (offset * 2);
		gpu_exec(1);
		gpu_pc = delayed_pc;
/*		uint16 opcode = GPUReadWord(gpu_pc, GPU);
		gpu_opcode_first_parameter = (opcode >> 5) & 0x1F;
		gpu_opcode_second_parameter = opcode & 0x1F;

		gpu_pc = delayed_pc;
		gpu_opcode[opcode>>10]();//*/
	}
#ifdef GPU_DIS_JR
	else
		if (doGPUDis)
			WriteLog("Branch NOT taken.\n");
#endif
}

static void gpu_opcode_add(void)
{
#ifdef GPU_DIS_ADD
	if (doGPUDis)
		WriteLog("%06X: ADD    R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	UINT32 res = RN + RM;
	CLR_ZNC; SET_ZNC_ADD(RN, RM, res);
	RN = res;
#ifdef GPU_DIS_ADD
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_addc(void)
{
#ifdef GPU_DIS_ADDC
	if (doGPUDis)
		WriteLog("%06X: ADDC   R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
/*	int dreg = jaguar.op & 31;
	UINT32 r1 = jaguar.r[(jaguar.op >> 5) & 31];
	UINT32 r2 = jaguar.r[dreg];
	UINT32 res = r2 + r1 + ((jaguar.FLAGS >> 1) & 1);
	jaguar.r[dreg] = res;
	CLR_ZNC; SET_ZNC_ADD(r2,r1,res);*/

	UINT32 res = RN + RM + gpu_flag_c;
	UINT32 carry = gpu_flag_c;
//	SET_ZNC_ADD(RN, RM, res); //???BUG??? Yes!
	SET_ZNC_ADD(RN + carry, RM, res);
//	SET_ZNC_ADD(RN, RM + carry, res);
	RN = res;
#ifdef GPU_DIS_ADDC
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_addq(void)
{
#ifdef GPU_DIS_ADDQ
	if (doGPUDis)
		WriteLog("%06X: ADDQ   #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1], IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	UINT32 r1 = gpu_convert_zero[IMM_1];
	UINT32 res = RN + r1;
	CLR_ZNC; SET_ZNC_ADD(RN, r1, res);
	RN = res;
#ifdef GPU_DIS_ADDQ
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_addqt(void)
{
#ifdef GPU_DIS_ADDQT
	if (doGPUDis)
		WriteLog("%06X: ADDQT  #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1], IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	RN += gpu_convert_zero[IMM_1];
#ifdef GPU_DIS_ADDQT
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_sub(void)
{
#ifdef GPU_DIS_SUB
	if (doGPUDis)
		WriteLog("%06X: SUB    R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	UINT32 res = RN - RM;
	SET_ZNC_SUB(RN, RM, res);
	RN = res;
#ifdef GPU_DIS_SUB
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_subc(void)
{
#ifdef GPU_DIS_SUBC
	if (doGPUDis)
		WriteLog("%06X: SUBC   R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	UINT32 res = RN - RM - gpu_flag_c;
	UINT32 borrow = gpu_flag_c;
//	SET_ZNC_SUB(RN, RM, res); //???BUG??? YES!!!
//No matter how you do it, there is a problem. With below, it's 0-0 with carry,
//and the one below it it's FFFFFFFF - FFFFFFFF with carry... !!! FIX !!!
//	SET_ZNC_SUB(RN - borrow, RM, res);
	SET_ZNC_SUB(RN, RM + borrow, res);
	RN = res;
#ifdef GPU_DIS_SUBC
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}
/*
N = 5, M = 3, 3 - 5 = -2, C = 1... Or, in our case:
N = 0, M = 1, 0 - 1 = -1, C = 0!

#define SET_C_SUB(a,b)		(gpu_flag_c = ((UINT32)(b) > (UINT32)(a)))
#define SET_ZN(r)			SET_N(r); SET_Z(r)
#define SET_ZNC_ADD(a,b,r)	SET_N(r); SET_Z(r); SET_C_ADD(a,b)
#define SET_ZNC_SUB(a,b,r)	SET_N(r); SET_Z(r); SET_C_SUB(a,b)
*/
static void gpu_opcode_subq(void)
{
#ifdef GPU_DIS_SUBQ
	if (doGPUDis)
		WriteLog("%06X: SUBQ   #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1], IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	UINT32 r1 = gpu_convert_zero[IMM_1];
	UINT32 res = RN - r1;
	SET_ZNC_SUB(RN, r1, res);
	RN = res;
#ifdef GPU_DIS_SUBQ
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_subqt(void)
{
#ifdef GPU_DIS_SUBQT
	if (doGPUDis)
		WriteLog("%06X: SUBQT  #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1], IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	RN -= gpu_convert_zero[IMM_1];
#ifdef GPU_DIS_SUBQT
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_cmp(void)
{
#ifdef GPU_DIS_CMP
	if (doGPUDis)
		WriteLog("%06X: CMP    R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	UINT32 res = RN - RM;
	SET_ZNC_SUB(RN, RM, res);
#ifdef GPU_DIS_CMP
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z);
#endif
}

static void gpu_opcode_cmpq(void)
{
	static int32 sqtable[32] =
		{ 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,-16,-15,-14,-13,-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1 };
#ifdef GPU_DIS_CMPQ
	if (doGPUDis)
		WriteLog("%06X: CMPQ   #%d, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, sqtable[IMM_1], IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	UINT32 r1 = sqtable[IMM_1 & 0x1F]; // I like this better -> (INT8)(jaguar.op >> 2) >> 3;
	UINT32 res = RN - r1;
	SET_ZNC_SUB(RN, r1, res);
#ifdef GPU_DIS_CMPQ
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z);
#endif
}

static void gpu_opcode_and(void)
{
#ifdef GPU_DIS_AND
	if (doGPUDis)
		WriteLog("%06X: AND    R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	RN = RN & RM;
	SET_ZN(RN);
#ifdef GPU_DIS_AND
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_or(void)
{
#ifdef GPU_DIS_OR
	if (doGPUDis)
		WriteLog("%06X: OR     R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	RN = RN | RM;
	SET_ZN(RN);
#ifdef GPU_DIS_OR
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_xor(void)
{
#ifdef GPU_DIS_XOR
	if (doGPUDis)
		WriteLog("%06X: XOR    R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	RN = RN ^ RM;
	SET_ZN(RN);
#ifdef GPU_DIS_XOR
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_not(void)
{
#ifdef GPU_DIS_NOT
	if (doGPUDis)
		WriteLog("%06X: NOT    R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	RN = ~RN;
	SET_ZN(RN);
#ifdef GPU_DIS_NOT
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_move_pc(void)
{
#ifdef GPU_DIS_MOVEPC
	if (doGPUDis)
		WriteLog("%06X: MOVE   PC, R%02u [NCZ:%u%u%u, PC=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, gpu_pc-2, IMM_2, RN);
#endif
	// Should be previous PC--this might not always be previous instruction!
	// Then again, this will point right at the *current* instruction, i.e., MOVE PC,R!
	RN = gpu_pc - 2;
#ifdef GPU_DIS_MOVEPC
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_sat8(void)
{
#ifdef GPU_DIS_SAT8
	if (doGPUDis)
		WriteLog("%06X: SAT8   R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	RN = ((int32)RN < 0 ? 0 : (RN > 0xFF ? 0xFF : RN));
	SET_ZN(RN);
#ifdef GPU_DIS_SAT8
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_sat16(void)
{
	RN = ((int32)RN < 0 ? 0 : (RN > 0xFFFF ? 0xFFFF : RN));
	SET_ZN(RN);
}

static void gpu_opcode_sat24(void)
{
	RN = ((int32)RN < 0 ? 0 : (RN > 0xFFFFFF ? 0xFFFFFF : RN));
	SET_ZN(RN);
}

static void gpu_opcode_store_r14_indexed(void)
{
#ifdef GPU_DIS_STORE14I
	if (doGPUDis)
		WriteLog("%06X: STORE  R%02u, (R14+$%02X) [NCZ:%u%u%u, R%02u=%08X, R14+$%02X=%08X]\n", gpu_pc-2, IMM_2, gpu_convert_zero[IMM_1] << 2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN, gpu_convert_zero[IMM_1] << 2, gpu_reg[14]+(gpu_convert_zero[IMM_1] << 2));
#endif
	GPUWriteLong(gpu_reg[14] + (gpu_convert_zero[IMM_1] << 2), RN, GPU);
}

static void gpu_opcode_store_r15_indexed(void)
{
#ifdef GPU_DIS_STORE15I
	if (doGPUDis)
		WriteLog("%06X: STORE  R%02u, (R15+$%02X) [NCZ:%u%u%u, R%02u=%08X, R15+$%02X=%08X]\n", gpu_pc-2, IMM_2, gpu_convert_zero[IMM_1] << 2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN, gpu_convert_zero[IMM_1] << 2, gpu_reg[15]+(gpu_convert_zero[IMM_1] << 2));
#endif
	GPUWriteLong(gpu_reg[15] + (gpu_convert_zero[IMM_1] << 2), RN, GPU);
}

static void gpu_opcode_load_r14_ri(void)
{
#ifdef GPU_DIS_LOAD14R
	if (doGPUDis)
		WriteLog("%06X: LOAD   (R14+R%02u), R%02u [NCZ:%u%u%u, R14+R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM+gpu_reg[14], IMM_2, RN);
#endif
	RN = GPUReadLong(gpu_reg[14] + RM, GPU);
#ifdef GPU_DIS_LOAD14R
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_load_r15_ri(void)
{
#ifdef GPU_DIS_LOAD15R
	if (doGPUDis)
		WriteLog("%06X: LOAD   (R15+R%02u), R%02u [NCZ:%u%u%u, R15+R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM+gpu_reg[15], IMM_2, RN);
#endif
	RN = GPUReadLong(gpu_reg[15] + RM, GPU);
#ifdef GPU_DIS_LOAD15R
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_store_r14_ri(void)
{
#ifdef GPU_DIS_STORE14R
	if (doGPUDis)
		WriteLog("%06X: STORE  R%02u, (R14+R%02u) [NCZ:%u%u%u, R%02u=%08X, R14+R%02u=%08X]\n", gpu_pc-2, IMM_2, IMM_1, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN, IMM_1, RM+gpu_reg[14]);
#endif
	GPUWriteLong(gpu_reg[14] + RM, RN, GPU);
}

static void gpu_opcode_store_r15_ri(void)
{
#ifdef GPU_DIS_STORE15R
	if (doGPUDis)
		WriteLog("%06X: STORE  R%02u, (R15+R%02u) [NCZ:%u%u%u, R%02u=%08X, R15+R%02u=%08X]\n", gpu_pc-2, IMM_2, IMM_1, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN, IMM_1, RM+gpu_reg[15]);
#endif
	GPUWriteLong(gpu_reg[15] + RM, RN, GPU);
}

static void gpu_opcode_nop(void)
{
#ifdef GPU_DIS_NOP
	if (doGPUDis)
		WriteLog("%06X: NOP    [NCZ:%u%u%u]\n", gpu_pc-2, gpu_flag_n, gpu_flag_c, gpu_flag_z);
#endif
}

static void gpu_opcode_pack(void)
{
#ifdef GPU_DIS_PACK
	if (doGPUDis)
		WriteLog("%06X: %s R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, (!IMM_1 ? "PACK  " : "UNPACK"), IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	uint32 val = RN;

//BUG!	if (RM == 0)				// Pack
	if (IMM_1 == 0)				// Pack
		RN = ((val >> 10) & 0x0000F000) | ((val >> 5) & 0x00000F00) | (val & 0x000000FF);
	else						// Unpack
		RN = ((val & 0x0000F000) << 10) | ((val & 0x00000F00) << 5) | (val & 0x000000FF);
#ifdef GPU_DIS_PACK
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_storeb(void)
{
#ifdef GPU_DIS_STOREB
	if (doGPUDis)
		WriteLog("%06X: STOREB R%02u, (R%02u) [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_pc-2, IMM_2, IMM_1, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN, IMM_1, RM);
#endif
//Is this right???
// Would appear to be so...!
	if ((RM >= 0xF03000) && (RM <= 0xF03FFF))
		GPUWriteLong(RM, RN & 0xFF, GPU);
	else
		JaguarWriteByte(RM, RN, GPU);
}

static void gpu_opcode_storew(void)
{
#ifdef GPU_DIS_STOREW
	if (doGPUDis)
		WriteLog("%06X: STOREW R%02u, (R%02u) [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_pc-2, IMM_2, IMM_1, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN, IMM_1, RM);
#endif
	if ((RM >= 0xF03000) && (RM <= 0xF03FFF))
		GPUWriteLong(RM, RN & 0xFFFF, GPU);
	else
		JaguarWriteWord(RM, RN, GPU);
}

static void gpu_opcode_store(void)
{
#ifdef GPU_DIS_STORE
	if (doGPUDis)
		WriteLog("%06X: STORE  R%02u, (R%02u) [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_pc-2, IMM_2, IMM_1, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN, IMM_1, RM);
#endif
	GPUWriteLong(RM, RN, GPU);
}

static void gpu_opcode_storep(void)
{
	GPUWriteLong(RM + 0, gpu_hidata, GPU);
	GPUWriteLong(RM + 4, RN, GPU);
}

static void gpu_opcode_loadb(void)
{
#ifdef GPU_DIS_LOADB
	if (doGPUDis)
		WriteLog("%06X: LOADB  (R%02u), R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	if ((RM >= 0xF03000) && (RM <= 0xF03FFF))
		RN = GPUReadLong(RM, GPU) & 0xFF;
	else
		RN = JaguarReadByte(RM, GPU);
#ifdef GPU_DIS_LOADB
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_loadw(void)
{
#ifdef GPU_DIS_LOADW
	if (doGPUDis)
		WriteLog("%06X: LOADW  (R%02u), R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	if ((RM >= 0xF03000) && (RM <= 0xF03FFF))
		RN = GPUReadLong(RM, GPU) & 0xFFFF;
	else
		RN = JaguarReadWord(RM, GPU);
#ifdef GPU_DIS_LOADW
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_load(void)
{
#ifdef GPU_DIS_LOAD
	if (doGPUDis)
		WriteLog("%06X: LOAD   (R%02u), R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	RN = GPUReadLong(RM, GPU);
#ifdef GPU_DIS_LOAD
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_loadp(void)
{
	gpu_hidata = GPUReadLong(RM + 0, GPU);
	RN		   = GPUReadLong(RM + 4, GPU);
}

static void gpu_opcode_load_r14_indexed(void)
{
#ifdef GPU_DIS_LOAD14I
	if (doGPUDis)
		WriteLog("%06X: LOAD   (R14+$%02X), R%02u [NCZ:%u%u%u, R14+$%02X=%08X, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1] << 2, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, gpu_convert_zero[IMM_1] << 2, gpu_reg[14]+(gpu_convert_zero[IMM_1] << 2), IMM_2, RN);
#endif
	RN = GPUReadLong(gpu_reg[14] + (gpu_convert_zero[IMM_1] << 2), GPU);
#ifdef GPU_DIS_LOAD14I
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_load_r15_indexed(void)
{
#ifdef GPU_DIS_LOAD15I
	if (doGPUDis)
		WriteLog("%06X: LOAD   (R15+$%02X), R%02u [NCZ:%u%u%u, R15+$%02X=%08X, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1] << 2, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, gpu_convert_zero[IMM_1] << 2, gpu_reg[15]+(gpu_convert_zero[IMM_1] << 2), IMM_2, RN);
#endif
	RN = GPUReadLong(gpu_reg[15] + (gpu_convert_zero[IMM_1] << 2), GPU);
#ifdef GPU_DIS_LOAD15I
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_movei(void)
{
#ifdef GPU_DIS_MOVEI
	if (doGPUDis)
		WriteLog("%06X: MOVEI  #$%08X, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, (uint32)GPUReadWord(gpu_pc) | ((uint32)GPUReadWord(gpu_pc + 2) << 16), IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	// This instruction is followed by 32-bit value in LSW / MSW format...
	RN = (uint32)GPUReadWord(gpu_pc, GPU) | ((uint32)GPUReadWord(gpu_pc + 2, GPU) << 16);
	gpu_pc += 4;
#ifdef GPU_DIS_MOVEI
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_moveta(void)
{
#ifdef GPU_DIS_MOVETA
	if (doGPUDis)
		WriteLog("%06X: MOVETA R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u(alt)=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, ALTERNATE_RN);
#endif
	ALTERNATE_RN = RM;
#ifdef GPU_DIS_MOVETA
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u(alt)=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, ALTERNATE_RN);
#endif
}

static void gpu_opcode_movefa(void)
{
#ifdef GPU_DIS_MOVEFA
	if (doGPUDis)
		WriteLog("%06X: MOVEFA R%02u, R%02u [NCZ:%u%u%u, R%02u(alt)=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, ALTERNATE_RM, IMM_2, RN);
#endif
	RN = ALTERNATE_RM;
#ifdef GPU_DIS_MOVEFA
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u(alt)=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, ALTERNATE_RM, IMM_2, RN);
#endif
}

static void gpu_opcode_move(void)
{
#ifdef GPU_DIS_MOVE
	if (doGPUDis)
		WriteLog("%06X: MOVE   R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	RN = RM;
#ifdef GPU_DIS_MOVE
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_moveq(void)
{
#ifdef GPU_DIS_MOVEQ
	if (doGPUDis)
		WriteLog("%06X: MOVEQ  #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	RN = IMM_1;
#ifdef GPU_DIS_MOVEQ
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_resmac(void)
{
	RN = gpu_acc;
}

static void gpu_opcode_imult(void)
{
#ifdef GPU_DIS_IMULT
	if (doGPUDis)
		WriteLog("%06X: IMULT  R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	RN = (int16)RN * (int16)RM;
	SET_ZN(RN);
#ifdef GPU_DIS_IMULT
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_mult(void)
{
#ifdef GPU_DIS_MULT
	if (doGPUDis)
		WriteLog("%06X: MULT   R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	RN = (uint16)RM * (uint16)RN;
	SET_ZN(RN);
#ifdef GPU_DIS_MULT
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_bclr(void)
{
#ifdef GPU_DIS_BCLR
	if (doGPUDis)
		WriteLog("%06X: BCLR   #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	UINT32 res = RN & ~(1 << IMM_1);
	RN = res;
	SET_ZN(res);
#ifdef GPU_DIS_BCLR
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_btst(void)
{
#ifdef GPU_DIS_BTST
	if (doGPUDis)
		WriteLog("%06X: BTST   #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	gpu_flag_z = (~RN >> IMM_1) & 1;
#ifdef GPU_DIS_BTST
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_bset(void)
{
#ifdef GPU_DIS_BSET
	if (doGPUDis)
		WriteLog("%06X: BSET   #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	UINT32 res = RN | (1 << IMM_1);
	RN = res;
	SET_ZN(res);
#ifdef GPU_DIS_BSET
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_imacn(void)
{
	uint32 res = (int16)RM * (int16)(RN);
	gpu_acc += res;
}

static void gpu_opcode_mtoi(void)
{
	uint32 _RM = RM;
	uint32 res = RN = (((INT32)_RM >> 8) & 0xFF800000) | (_RM & 0x007FFFFF);
	SET_ZN(res);
}

static void gpu_opcode_normi(void)
{
	uint32 _RM = RM;
	uint32 res = 0;

	if (_RM)
	{
		while ((_RM & 0xFFC00000) == 0)
		{
			_RM <<= 1;
			res--;
		}
		while ((_RM & 0xFF800000) != 0)
		{
			_RM >>= 1;
			res++;
		}
	}
	RN = res;
	SET_ZN(res);
}

static void gpu_opcode_mmult(void)
{
	int count	= gpu_matrix_control & 0x0F;	// Matrix width
	uint32 addr = gpu_pointer_to_matrix;		// In the GPU's RAM
	int64 accum = 0;
	uint32 res;

	if (gpu_matrix_control & 0x10)				// Column stepping
	{
		for(int i=0; i<count; i++)
		{ 
			int16 a;
			if (i & 0x01)
				a = (int16)((gpu_alternate_reg[IMM_1 + (i >> 1)] >> 16) & 0xFFFF);
			else
				a = (int16)(gpu_alternate_reg[IMM_1 + (i >> 1)] & 0xFFFF);

			int16 b = ((int16)GPUReadWord(addr + 2, GPU));
			accum += a * b;
			addr += 4 * count;
		}
	}
	else										// Row stepping
	{
		for(int i=0; i<count; i++)
		{
			int16 a;
			if (i & 0x01)
				a = (int16)((gpu_alternate_reg[IMM_1 + (i >> 1)] >> 16) & 0xFFFF);
			else
				a = (int16)(gpu_alternate_reg[IMM_1 + (i >> 1)] & 0xFFFF);

			int16 b = ((int16)GPUReadWord(addr + 2, GPU));
			accum += a * b;
			addr += 4;
		}
	}
	RN = res = (int32)accum;
	// carry flag to do (out of the last add)
	SET_ZN(res);
}

static void gpu_opcode_abs(void)
{
#ifdef GPU_DIS_ABS
	if (doGPUDis)
		WriteLog("%06X: ABS    R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	gpu_flag_c = RN >> 31;
	if (RN == 0x80000000)
	//Is 0x80000000 a positive number? If so, then we need to set C to 0 as well!
		gpu_flag_n = 1, gpu_flag_z = 0;
	else
	{
		if (gpu_flag_c)
			RN = -RN;
		gpu_flag_n = 0; SET_FLAG_Z(RN);
	}
#ifdef GPU_DIS_ABS
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_div(void)	// RN / RM
{
#ifdef GPU_DIS_DIV
	if (doGPUDis)
		WriteLog("%06X: DIV    R%02u, R%02u (%s) [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, (gpu_div_control & 0x01 ? "16.16" : "32"), gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
// NOTE: remainder is NOT calculated correctly here!
//       The original tried to get it right by checking to see if the
//       remainder was negative, but that's too late...
// The code there should do it now, but I'm not 100% sure...

	if (RM)
	{
		if (gpu_div_control & 0x01)		// 16.16 division
		{
			RN = ((UINT64)RN << 16) / RM;
			gpu_remain = ((UINT64)RN << 16) % RM;
		}
		else
		{
			RN = RN / RM;
			gpu_remain = RN % RM;
		}

		if ((gpu_remain - RM) & 0x80000000)	// If the result would have been negative...
			gpu_remain -= RM;			// Then make it negative!
	}
	else
		RN = 0xFFFFFFFF;

/*	uint32 _RM=RM;
	uint32 _RN=RN;

	if (_RM)
	{
		if (gpu_div_control & 1)
		{
			gpu_remain = (((uint64)_RN) << 16) % _RM;
			if (gpu_remain&0x80000000)
				gpu_remain-=_RM;
			RN = (((uint64)_RN) << 16) / _RM;
		}
		else
		{
			gpu_remain = _RN % _RM;
			if (gpu_remain&0x80000000)
				gpu_remain-=_RM;
			RN/=_RM;
		}
	}
	else
		RN=0xffffffff;*/
#ifdef GPU_DIS_DIV
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] Remainder: %08X\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN, gpu_remain);
#endif
}

static void gpu_opcode_imultn(void)
{
	uint32 res = (int32)((int16)RN * (int16)RM);
	gpu_acc = (int32)res;
	SET_FLAG_Z(res);
	SET_FLAG_N(res);
}

static void gpu_opcode_neg(void)
{
#ifdef GPU_DIS_NEG
	if (doGPUDis)
		WriteLog("%06X: NEG    R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	UINT32 res = -RN;
	SET_ZNC_SUB(0, RN, res);
	RN = res;
#ifdef GPU_DIS_NEG
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_shlq(void)
{
#ifdef GPU_DIS_SHLQ
	if (doGPUDis)
		WriteLog("%06X: SHLQ   #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, 32 - IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
// Was a bug here...
// (Look at Aaron's code: If r1 = 32, then 32 - 32 = 0 which is wrong!)
	INT32 r1 = 32 - IMM_1;
	UINT32 res = RN << r1;
	SET_ZN(res); gpu_flag_c = (RN >> 31) & 1;
	RN = res;
#ifdef GPU_DIS_SHLQ
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_shrq(void)
{
#ifdef GPU_DIS_SHRQ
	if (doGPUDis)
		WriteLog("%06X: SHRQ   #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1], IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	INT32 r1 = gpu_convert_zero[IMM_1];
	UINT32 res = RN >> r1;
	SET_ZN(res); gpu_flag_c = RN & 1;
	RN = res;
#ifdef GPU_DIS_SHRQ
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_ror(void)
{
#ifdef GPU_DIS_ROR
	if (doGPUDis)
		WriteLog("%06X: ROR    R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	UINT32 r1 = RM & 0x1F;
	UINT32 res = (RN >> r1) | (RN << (32 - r1));
	SET_ZN(res); gpu_flag_c = (RN >> 31) & 1;
	RN = res;
#ifdef GPU_DIS_ROR
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

static void gpu_opcode_rorq(void)
{
#ifdef GPU_DIS_RORQ
	if (doGPUDis)
		WriteLog("%06X: RORQ   #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1], IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	UINT32 r1 = gpu_convert_zero[IMM_1 & 0x1F];
	UINT32 r2 = RN;
	UINT32 res = (r2 >> r1) | (r2 << (32 - r1));
	RN = res;
	SET_ZN(res); gpu_flag_c = (r2 >> 31) & 0x01;
#ifdef GPU_DIS_RORQ
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_sha(void)
{
/*	int dreg = jaguar.op & 31;
	INT32 r1 = (INT32)jaguar.r[(jaguar.op >> 5) & 31];
	UINT32 r2 = jaguar.r[dreg];
	UINT32 res;

	CLR_ZNC;
	if (r1 < 0)
	{
		res = (r1 <= -32) ? 0 : (r2 << -r1);
		jaguar.FLAGS |= (r2 >> 30) & 2;
	}
	else
	{
		res = (r1 >= 32) ? ((INT32)r2 >> 31) : ((INT32)r2 >> r1);
		jaguar.FLAGS |= (r2 << 1) & 2;
	}
	jaguar.r[dreg] = res;
	SET_ZN(res);*/

#ifdef GPU_DIS_SHA
	if (doGPUDis)
		WriteLog("%06X: SHA    R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	UINT32 res;

	if ((INT32)RM < 0)
	{
		res = ((INT32)RM <= -32) ? 0 : (RN << -(INT32)RM);
		gpu_flag_c = RN >> 31;
	}
	else
	{
		res = ((INT32)RM >= 32) ? ((INT32)RN >> 31) : ((INT32)RN >> (INT32)RM);
		gpu_flag_c = RN & 0x01;
	}
	RN = res;
	SET_ZN(res);
#ifdef GPU_DIS_SHA
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif

/*	int32 sRM=(int32)RM;
	uint32 _RN=RN;

	if (sRM<0)
	{
		uint32 shift=-sRM;
		if (shift>=32) shift=32;
		gpu_flag_c=(_RN&0x80000000)>>31;
		while (shift)
		{
			_RN<<=1;
			shift--;
		}
	}
	else
	{
		uint32 shift=sRM;
		if (shift>=32) shift=32;
		gpu_flag_c=_RN&0x1;
		while (shift)
		{
			_RN=((int32)_RN)>>1;
			shift--;
		}
	}
	RN=_RN;
	SET_FLAG_Z(_RN);
	SET_FLAG_N(_RN);*/
}

static void gpu_opcode_sharq(void)
{
#ifdef GPU_DIS_SHARQ
	if (doGPUDis)
		WriteLog("%06X: SHARQ  #%u, R%02u [NCZ:%u%u%u, R%02u=%08X] -> ", gpu_pc-2, gpu_convert_zero[IMM_1], IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
	UINT32 res = (INT32)RN >> gpu_convert_zero[IMM_1];
	SET_ZN(res); gpu_flag_c = RN & 0x01;
	RN = res;
#ifdef GPU_DIS_SHARQ
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_2, RN);
#endif
}

static void gpu_opcode_sh(void)
{
#ifdef GPU_DIS_SH
	if (doGPUDis)
		WriteLog("%06X: SH     R%02u, R%02u [NCZ:%u%u%u, R%02u=%08X, R%02u=%08X] -> ", gpu_pc-2, IMM_1, IMM_2, gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
	if (RM & 0x80000000)		// Shift left
	{
		gpu_flag_c = RN >> 31;
		RN = ((int32)RM <= -32 ? 0 : RN << -(int32)RM);
	}
	else						// Shift right
	{
		gpu_flag_c = RN & 0x01;
		RN = (RM >= 32 ? 0 : RN >> RM);
	}
	SET_ZN(RN);
#ifdef GPU_DIS_SH
	if (doGPUDis)
		WriteLog("[NCZ:%u%u%u, R%02u=%08X, R%02u=%08X]\n", gpu_flag_n, gpu_flag_c, gpu_flag_z, IMM_1, RM, IMM_2, RN);
#endif
}

//Temporary: Testing only!
//#include "gpu2.cpp"
//#include "gpu3.cpp"
