#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cpu.h"

#define MEMORY_SIZE (1 << 20)
#define PROGRAM_START 0x1000

uint8_t memory[MEMORY_SIZE];
uint64_t reg[REG_COUNT];
uint64_t pc;
uint8_t nzcv;

static uint64_t program_end=PROGRAM_START;
static int cpu_running=0;

enum{
    FLAG_V=1<<0,
    FLAG_C=1<<1,
    FLAG_Z=1<<2,
    FLAG_N=1<<3
};

enum opcode{
    OP_ADD_IMM,
    OP_ADD_SHIFT,
    OP_ADD_EXT,
    OP_ADDS_IMM,
    OP_ADDS_SHIFT,
    OP_ADDS_EXT,
    OP_SUB_IMM,
    OP_SUB_SHIFT,
    OP_SUB_EXT,
    OP_SUBS_IMM,
    OP_SUBS_SHIFT,
    OP_SUBS_EXT,

    OP_AND_IMM,
    OP_AND_SHIFT,
    OP_ANDS_IMM,
    OP_ANDS_SHIFT,
    OP_ORR_IMM,
    OP_ORR_SHIFT,
    OP_EOR_IMM,
    OP_EOR_SHIFT,

    OP_MOVN,
    OP_MOVZ,
    OP_MOVK,

    OP_LDR,
    OP_STR,

    OP_B,
    OP_BL,
    OP_B_COND,
    OP_BR,
    OP_BLR,
    OP_RET,

    OP_UNKNOWN
};

static uint64_t sign_extend(uint64_t x,unsigned bits){
    uint64_t sign=1ULL<<(bits-1);
    if(x&sign)
        x|=~((1ULL<<bits)-1);
    return x;
}

static uint32_t mem_read32(uint64_t address){
    if(address+3>=MEMORY_SIZE) return 0;

    return
        ((uint32_t)memory[address]) |
        ((uint32_t)memory[address+1]<<8) |
        ((uint32_t)memory[address+2]<<16) |
        ((uint32_t)memory[address+3]<<24);
}

uint64_t mem_read64(uint64_t address){
    if(address+7>=MEMORY_SIZE) return 0;

    uint64_t value=0;

    for(int i=0;i<8;i++)
        value|=(uint64_t)memory[address+i]<<(i*8);

    return value;
}

static int mem_write64(uint64_t address,uint64_t value){
    if(address+7>=MEMORY_SIZE)
        return 0;

    for(int i=0;i<8;i++)
        memory[address+i]=(uint8_t)((value>>(i*8))&0xff);

    return 1;
}

static void update_nz(uint64_t result){
    nzcv&=~(FLAG_N|FLAG_Z);

    if(result==0)
        nzcv|=FLAG_Z;

    if(result&(1ULL<<63))
        nzcv|=FLAG_N;
}

static void update_add_flags(uint64_t a,uint64_t b,uint64_t result){
    nzcv=0;

    if(result&(1ULL<<63)) nzcv|=FLAG_N;
    if(result==0) nzcv|=FLAG_Z;
    if(result<a) nzcv|=FLAG_C;

    uint64_t sign=1ULL<<63;

    if((~(a^b)&(a^result)&sign)!=0)
        nzcv|=FLAG_V;
}

static void update_sub_flags(uint64_t a,uint64_t b,uint64_t result){
    nzcv=0;

    if(result&(1ULL<<63)) nzcv|=FLAG_N;
    if(result==0) nzcv|=FLAG_Z;
    if(a>=b) nzcv|=FLAG_C;

    uint64_t sign=1ULL<<63;

    if(((a^b)&(a^result)&sign)!=0)
        nzcv|=FLAG_V;
}

static int condition_holds(uint32_t cond){
    int N=!!(nzcv&FLAG_N);
    int Z=!!(nzcv&FLAG_Z);
    int C=!!(nzcv&FLAG_C);
    int V=!!(nzcv&FLAG_V);

    switch(cond){
        case 0x0: return Z;
        case 0x1: return !Z;
        case 0x2: return C;
        case 0x3: return !C;
        case 0x4: return N;
        case 0x5: return !N;
        case 0x6: return V;
        case 0x7: return !V;
        case 0x8: return C&&!Z;
        case 0x9: return !C||Z;
        case 0xA: return N==V;
        case 0xB: return N!=V;
        case 0xC: return !Z&&(N==V);
        case 0xD: return Z||(N!=V);
        case 0xE: return 1;
        default: return 0;
    }
}

static int build_logical_mask(
    uint32_t N,
    uint32_t immr,
    uint32_t imms,
    uint64_t *mask_out
){
    uint32_t value=(N<<6)|((~imms)&0x3f);
    int len=-1;

    for(int i=6;i>=0;i--){
        if(value&(1u<<i)){
            len=i;
            break;
        }
    }

    if(len<1) return 0;

    uint32_t levels=(1u<<len)-1;
    uint32_t S=imms&levels;
    uint32_t R=immr&levels;

    if(S==levels) return 0;

    uint32_t esize=1u<<len;
    uint64_t element;

    if(S==63)
        element=UINT64_MAX;
    else
        element=(1ULL<<(S+1))-1;

    uint64_t element_mask=
        esize==64
        ? UINT64_MAX
        : (1ULL<<esize)-1;

    R%=esize;

    if(R)
        element=((element>>R)|(element<<(esize-R)))&element_mask;

    uint64_t result=0;

    for(uint32_t pos=0;pos<64;pos+=esize)
        result|=element<<pos;

    *mask_out=result;
    return 1;
}

static int shift_register(
    uint64_t value,
    uint32_t type,
    uint32_t amount,
    uint64_t *result
){
    if(amount>63) return 0;

    switch(type){
        case 0:
            *result=value<<amount;
            return 1;

        case 1:
            *result=value>>amount;
            return 1;

        case 2:
            *result=(uint64_t)((int64_t)value>>amount);
            return 1;

        case 3:
            if(amount==0){
                *result=value;
                return 1;
            }

            *result=(value>>amount)|(value<<(64-amount));
            return 1;
    }

    return 0;
}

static int extend_register(
    uint64_t value,
    uint32_t option,
    uint32_t shift,
    uint64_t *result
){
    if(shift>4) return 0;

    uint64_t extended;

    switch(option){
        case 0:
            extended=value&0xff;
            break;

        case 1:
            extended=value&0xffff;
            break;

        case 2:
            extended=value&0xffffffffULL;
            break;

        case 3:
            extended=value;
            break;

        case 4:
            extended=(uint64_t)(int64_t)(int8_t)value;
            break;

        case 5:
            extended=(uint64_t)(int64_t)(int16_t)value;
            break;

        case 6:
            extended=(uint64_t)(int64_t)(int32_t)value;
            break;

        case 7:
            extended=(uint64_t)(int64_t)value;
            break;

        default:
            return 0;
    }

    *result=extended<<shift;
    return 1;
}

static enum opcode decode_instruction(uint32_t instr){
    const uint32_t MASK_ADD_SUB_IMM=0x1F800000;
    const uint32_t PATTERN_ADD_SUB_IMM=0x11000000;

    const uint32_t MASK_ADD_SUB_SHIFT=0x1F200000;
    const uint32_t PATTERN_ADD_SUB_SHIFT=0x0B000000;

    const uint32_t MASK_ADD_SUB_EXT=0x1FE00000;
    const uint32_t PATTERN_ADD_SUB_EXT=0x0B200000;

    const uint32_t MASK_LOGICAL_SHIFT=0x1F200000;
    const uint32_t PATTERN_LOGICAL_SHIFT=0x0A000000;

    const uint32_t MASK_LOGICAL_IMM=0x1F800000;
    const uint32_t PATTERN_LOGICAL_IMM=0x12000000;

    const uint32_t MASK_MOVE_WIDE=0x1F800000;
    const uint32_t PATTERN_MOVE_WIDE=0x12800000;

    const uint32_t MASK_UNCOND_BRANCH=0x7C000000;
    const uint32_t PATTERN_UNCOND_BRANCH=0x14000000;

    const uint32_t MASK_COND_BRANCH=0xFF000010;
    const uint32_t PATTERN_COND_BRANCH=0x54000000;

    const uint32_t MASK_BRANCH_REG=0xFF9FFC1F;
    const uint32_t PATTERN_BRANCH_REG=0xD61F0000;

    const uint32_t MASK_LOAD_STORE=0x3F000000;
    const uint32_t PATTERN_LOAD_STORE=0x39000000;

    if((instr&MASK_ADD_SUB_EXT)==PATTERN_ADD_SUB_EXT){
        uint32_t sf=(instr>>31)&1;
        uint32_t op=(instr>>30)&1;
        uint32_t S=(instr>>29)&1;

        if(!sf) return OP_UNKNOWN;

        if(!op&&!S) return OP_ADD_EXT;
        if(!op&&S) return OP_ADDS_EXT;
        if(op&&!S) return OP_SUB_EXT;
        return OP_SUBS_EXT;
    }

    if((instr&MASK_ADD_SUB_SHIFT)==PATTERN_ADD_SUB_SHIFT){
        uint32_t sf=(instr>>31)&1;
        uint32_t op=(instr>>30)&1;
        uint32_t S=(instr>>29)&1;

        if(!sf) return OP_UNKNOWN;

        if(!op&&!S) return OP_ADD_SHIFT;
        if(!op&&S) return OP_ADDS_SHIFT;
        if(op&&!S) return OP_SUB_SHIFT;
        return OP_SUBS_SHIFT;
    }

    if((instr&MASK_ADD_SUB_IMM)==PATTERN_ADD_SUB_IMM){
        uint32_t sf=(instr>>31)&1;
        uint32_t op=(instr>>30)&1;
        uint32_t S=(instr>>29)&1;

        if(!sf) return OP_UNKNOWN;

        if(!op&&!S) return OP_ADD_IMM;
        if(!op&&S) return OP_ADDS_IMM;
        if(op&&!S) return OP_SUB_IMM;
        return OP_SUBS_IMM;
    }

    if((instr&MASK_LOGICAL_SHIFT)==PATTERN_LOGICAL_SHIFT){
        uint32_t sf=(instr>>31)&1;
        uint32_t opc=(instr>>29)&3;

        if(!sf) return OP_UNKNOWN;

        if(opc==0) return OP_AND_SHIFT;
        if(opc==1) return OP_ORR_SHIFT;
        if(opc==2) return OP_EOR_SHIFT;
        return OP_ANDS_SHIFT;
    }

    if((instr&MASK_LOGICAL_IMM)==PATTERN_LOGICAL_IMM){
        uint32_t sf=(instr>>31)&1;
        uint32_t opc=(instr>>29)&3;

        if(!sf) return OP_UNKNOWN;

        if(opc==0) return OP_AND_IMM;
        if(opc==1) return OP_ORR_IMM;
        if(opc==2) return OP_EOR_IMM;
        return OP_ANDS_IMM;
    }

    if((instr&MASK_MOVE_WIDE)==PATTERN_MOVE_WIDE){
        uint32_t sf=(instr>>31)&1;
        uint32_t opc=(instr>>29)&3;

        if(!sf) return OP_UNKNOWN;

        if(opc==0) return OP_MOVN;
        if(opc==2) return OP_MOVZ;
        if(opc==3) return OP_MOVK;

        return OP_UNKNOWN;
    }

    if((instr&MASK_UNCOND_BRANCH)==PATTERN_UNCOND_BRANCH){
        return ((instr>>31)&1)?OP_BL:OP_B;
    }

    if((instr&MASK_BRANCH_REG)==PATTERN_BRANCH_REG){
        uint32_t op=(instr>>21)&3;

        if(op==0) return OP_BR;
        if(op==1) return OP_BLR;
        if(op==2) return OP_RET;

        return OP_UNKNOWN;
    }

    if((instr&MASK_COND_BRANCH)==PATTERN_COND_BRANCH)
        return OP_B_COND;

    if((instr&MASK_LOAD_STORE)==PATTERN_LOAD_STORE){
        uint32_t size=(instr>>30)&3;
        uint32_t opc=(instr>>22)&3;

        if(size!=3) return OP_UNKNOWN;

        if(opc==0) return OP_STR;
        if(opc==1) return OP_LDR;
    }

    return OP_UNKNOWN;
}

static int valid_reg(uint32_t r){
    return r<REG_COUNT;
}

static int execute_instruction(uint32_t instr){
    enum opcode op=decode_instruction(instr);

    switch(op){
        case OP_ADD_IMM:
        case OP_ADDS_IMM:
        case OP_SUB_IMM:
        case OP_SUBS_IMM:{
            uint32_t rn=(instr>>5)&0x1f;
            uint32_t rd=instr&0x1f;
            uint64_t imm=(instr>>10)&0xfff;
            uint32_t sh=(instr>>22)&1;

            if(!valid_reg(rn)||!valid_reg(rd))
                return 0;

            if(sh)
                imm<<=12;

            uint64_t a=reg[rn];
            uint64_t result;

            if(op==OP_ADD_IMM||op==OP_ADDS_IMM)
                result=a+imm;
            else
                result=a-imm;

            reg[rd]=result;

            if(op==OP_ADDS_IMM)
                update_add_flags(a,imm,result);

            if(op==OP_SUBS_IMM)
                update_sub_flags(a,imm,result);

            return 1;
        }

        case OP_ADD_SHIFT:
        case OP_ADDS_SHIFT:
        case OP_SUB_SHIFT:
        case OP_SUBS_SHIFT:{
            uint32_t shift=(instr>>22)&3;
            uint32_t rm=(instr>>16)&0x1f;
            uint32_t amount=(instr>>10)&0x3f;
            uint32_t rn=(instr>>5)&0x1f;
            uint32_t rd=instr&0x1f;

            if(!valid_reg(rn)||!valid_reg(rm)||!valid_reg(rd))
                return 0;

            uint64_t operand;

            if(!shift_register(reg[rm],shift,amount,&operand))
                return 0;

            uint64_t a=reg[rn];
            uint64_t result;

            if(op==OP_ADD_SHIFT||op==OP_ADDS_SHIFT)
                result=a+operand;
            else
                result=a-operand;

            reg[rd]=result;

            if(op==OP_ADDS_SHIFT)
                update_add_flags(a,operand,result);

            if(op==OP_SUBS_SHIFT)
                update_sub_flags(a,operand,result);

            return 1;
        }

        case OP_ADD_EXT:
        case OP_ADDS_EXT:
        case OP_SUB_EXT:
        case OP_SUBS_EXT:{
            uint32_t rm=(instr>>16)&0x1f;
            uint32_t option=(instr>>13)&7;
            uint32_t amount=(instr>>10)&7;
            uint32_t rn=(instr>>5)&0x1f;
            uint32_t rd=instr&0x1f;

            if(!valid_reg(rn)||!valid_reg(rm)||!valid_reg(rd))
                return 0;

            uint64_t operand;

            if(!extend_register(reg[rm],option,amount,&operand))
                return 0;

            uint64_t a=reg[rn];
            uint64_t result;

            if(op==OP_ADD_EXT||op==OP_ADDS_EXT)
                result=a+operand;
            else
                result=a-operand;

            reg[rd]=result;

            if(op==OP_ADDS_EXT)
                update_add_flags(a,operand,result);

            if(op==OP_SUBS_EXT)
                update_sub_flags(a,operand,result);

            return 1;
        }

        case OP_AND_IMM:
        case OP_ANDS_IMM:
        case OP_ORR_IMM:
        case OP_EOR_IMM:{
            uint32_t N=(instr>>22)&1;
            uint32_t immr=(instr>>16)&0x3f;
            uint32_t imms=(instr>>10)&0x3f;
            uint32_t rn=(instr>>5)&0x1f;
            uint32_t rd=instr&0x1f;

            if(!valid_reg(rn)||!valid_reg(rd))
                return 0;

            uint64_t mask;

            if(!build_logical_mask(N,immr,imms,&mask))
                return 0;

            uint64_t result;

            if(op==OP_AND_IMM||op==OP_ANDS_IMM)
                result=reg[rn]&mask;
            else if(op==OP_ORR_IMM)
                result=reg[rn]|mask;
            else
                result=reg[rn]^mask;

            reg[rd]=result;

            if(op==OP_ANDS_IMM){
                nzcv=0;
                update_nz(result);
            }

            return 1;
        }

        case OP_AND_SHIFT:
        case OP_ANDS_SHIFT:
        case OP_ORR_SHIFT:
        case OP_EOR_SHIFT:{
            uint32_t shift=(instr>>22)&3;
            uint32_t rm=(instr>>16)&0x1f;
            uint32_t amount=(instr>>10)&0x3f;
            uint32_t rn=(instr>>5)&0x1f;
            uint32_t rd=instr&0x1f;

            if(!valid_reg(rn)||!valid_reg(rm)||!valid_reg(rd))
                return 0;

            uint64_t operand;

            if(!shift_register(reg[rm],shift,amount,&operand))
                return 0;

            uint64_t result;

            if(op==OP_AND_SHIFT||op==OP_ANDS_SHIFT)
                result=reg[rn]&operand;
            else if(op==OP_ORR_SHIFT)
                result=reg[rn]|operand;
            else
                result=reg[rn]^operand;

            reg[rd]=result;

            if(op==OP_ANDS_SHIFT){
                nzcv=0;
                update_nz(result);
            }

            return 1;
        }

        case OP_MOVZ:
        case OP_MOVN:
        case OP_MOVK:{
            uint32_t hw=(instr>>21)&3;
            uint32_t imm16=(instr>>5)&0xffff;
            uint32_t rd=instr&0x1f;

            if(!valid_reg(rd))
                return 0;

            uint32_t shift=hw*16;
            uint64_t value=(uint64_t)imm16<<shift;

            if(op==OP_MOVZ)
                reg[rd]=value;

            else if(op==OP_MOVN)
                reg[rd]=~value;

            else{
                uint64_t mask=0xffffULL<<shift;
                reg[rd]=(reg[rd]&~mask)|value;
            }

            return 1;
        }

        case OP_LDR:{
            uint32_t imm12=(instr>>10)&0xfff;
            uint32_t rn=(instr>>5)&0x1f;
            uint32_t rt=instr&0x1f;

            if(!valid_reg(rn)||!valid_reg(rt))
                return 0;

            uint64_t address=reg[rn]+((uint64_t)imm12<<3);

            if(address+7>=MEMORY_SIZE)
                return 0;

            reg[rt]=mem_read64(address);
            return 1;
        }

        case OP_STR:{
            uint32_t imm12=(instr>>10)&0xfff;
            uint32_t rn=(instr>>5)&0x1f;
            uint32_t rt=instr&0x1f;

            if(!valid_reg(rn)||!valid_reg(rt))
                return 0;

            uint64_t address=reg[rn]+((uint64_t)imm12<<3);

            return mem_write64(address,reg[rt]);
        }

        case OP_B:{
            uint32_t imm26=instr&0x03ffffff;
            int64_t offset=(int64_t)sign_extend(imm26,26)*4;
            uint64_t current_pc=pc-4;

            pc=(uint64_t)((int64_t)current_pc+offset);
            return 1;
        }

        case OP_BL:{
            uint32_t imm26=instr&0x03ffffff;
            int64_t offset=(int64_t)sign_extend(imm26,26)*4;
            uint64_t current_pc=pc-4;

            reg[30]=pc;
            pc=(uint64_t)((int64_t)current_pc+offset);
            return 1;
        }

        case OP_B_COND:{
            uint32_t imm19=(instr>>5)&0x7ffff;
            uint32_t cond=instr&0xf;

            if(condition_holds(cond)){
                int64_t offset=(int64_t)sign_extend(imm19,19)*4;
                uint64_t current_pc=pc-4;

                pc=(uint64_t)((int64_t)current_pc+offset);
            }

            return 1;
        }

        case OP_BR:
        case OP_BLR:
        case OP_RET:{
            uint32_t rn=(instr>>5)&0x1f;

            if(!valid_reg(rn))
                return 0;

            uint64_t target=reg[rn];

            if(op==OP_BLR)
                reg[30]=pc;

            pc=target;
            return 1;
        }

        case OP_UNKNOWN:
        default:
            fprintf(
                stderr,
                "unknown instruction 0x%08x at PC 0x%llx\n",
                instr,
                (unsigned long long)(pc-4)
            );

            return 0;
    }
}

void cpu_reset(void){
    memset(memory,0,sizeof(memory));
    memset(reg,0,sizeof(reg));

    nzcv=0;
    pc=PROGRAM_START;
    program_end=PROGRAM_START;
    cpu_running=0;
}

int cpu_load_words(const uint32_t *words,size_t count){
    if(!words||count==0)
        return 0;

    if(PROGRAM_START+count*4>MEMORY_SIZE)
        return 0;

    cpu_reset();

    for(size_t i=0;i<count;i++){
        uint64_t address=PROGRAM_START+i*4;
        uint32_t word=words[i];

        memory[address]=(uint8_t)(word&0xff);
        memory[address+1]=(uint8_t)((word>>8)&0xff);
        memory[address+2]=(uint8_t)((word>>16)&0xff);
        memory[address+3]=(uint8_t)((word>>24)&0xff);
    }

    program_end=PROGRAM_START+count*4;
    cpu_running=1;

    return 1;
}

int cpu_finished(void){
    return !cpu_running;
}

uint32_t cpu_current_instruction(void){
    if(!cpu_running)
        return 0;

    if(pc<PROGRAM_START||pc>=program_end)
        return 0;

    return mem_read32(pc);
}

int cpu_step(void){
    if(!cpu_running)
        return 0;

    if(pc<PROGRAM_START||pc>=program_end){
        cpu_running=0;
        return 0;
    }

    if((pc&3)!=0){
        fprintf(
            stderr,
            "unaligned PC: 0x%llx\n",
            (unsigned long long)pc
        );

        cpu_running=0;
        return 0;
    }

    uint32_t instr=mem_read32(pc);

    /*
     * Mantemos a semântica que sua CPU já usava:
     *
     * fetch
     * PC += 4
     * execute
     *
     * Assim BL consegue salvar em X30 o endereço
     * da próxima instrução e branches usam pc-4
     * como endereço da instrução atual.
     */
    pc+=4;

    if(!execute_instruction(instr)){
        cpu_running=0;
        return 0;
    }

    /*
     * Programa flat atual:
     * chegar ao final significa finalizar.
     */
    if(pc==program_end)
        cpu_running=0;

    /*
     * Um branch pode gerar endereço inválido.
     */
    if(cpu_running &&
       (pc<PROGRAM_START||
        pc>program_end||
        (pc&3)!=0)){
        fprintf(
            stderr,
            "invalid PC after instruction: 0x%llx\n",
            (unsigned long long)pc
        );

        cpu_running=0;
        return 0;
    }

    return 1;
}

int cpu_run(void){
    while(!cpu_finished()){
        if(!cpu_step())
            return 0;
    }

    return 1;
}

static size_t load_image(const char *path,uint64_t start){
    FILE *file=fopen(path,"rb");

    if(!file)
        return 0;

    if(start>=MEMORY_SIZE){
        fclose(file);
        return 0;
    }

    size_t size=fread(
        &memory[start],
        1,
        MEMORY_SIZE-start,
        file
    );

    fclose(file);

    return size;
}

int run_binary(const char *path){
    cpu_reset();

    size_t size=load_image(path,PROGRAM_START);

    if(size==0){
        fprintf(stderr,"failed to load image: %s\n",path);
        return 0;
    }

    /*
     * A64 usa instruções de 4 bytes.
     */
    if(size%4!=0){
        fprintf(stderr,"binary size is not instruction aligned\n");
        return 0;
    }

    program_end=PROGRAM_START+size;
    cpu_running=1;

    return cpu_run();
}

void dump_registers(void){
    for(int i=0;i<REG_COUNT;i++){
        printf(
            "X%-2d = 0x%016llx\n",
            i,
            (unsigned long long)reg[i]
        );
    }

    printf(
        "PC  = 0x%016llx\n",
        (unsigned long long)pc
    );

    printf(
        "NZCV=%d%d%d%d\n",
        !!(nzcv&FLAG_N),
        !!(nzcv&FLAG_Z),
        !!(nzcv&FLAG_C),
        !!(nzcv&FLAG_V)
    );
}