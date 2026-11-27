/*
 * OFFLINE ONLY: execute the vendor parameter generator in Unicorn.
 * No USB, /dev/mem, device access, or physical register writes.
 * The binary remains an input file, not linked into our firmware.
 * Supports only the exact v1.18 blob and first cold LPDDR4X candidate.
 */
#include <unicorn/unicorn.h>
#include <unicorn/arm64.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE UINT64_C(0xff001000)
#define SRAM UINT64_C(0xff000000)
#define SRAM_SIZE 0x100000U
#define R UINT64_C(0xff080000)
#define P UINT64_C(0xff081000)
#define T UINT64_C(0xff082000)
#define STACK UINT64_C(0xff0f0000)
#define STOP UINT64_C(0xff090000)
#define SIZE 75320U
static unsigned char blob[SIZE];
static int bad;
static uint64_t last_pc;

static int check(uc_err e,const char *where)
{
    if(e==UC_ERR_OK)return 1;
    fprintf(stderr,"%s: %s\n",where,uc_strerror(e));
    return 0;
}
static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
           ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void trace(uc_engine *uc,uint64_t addr,uint32_t size,void *user)
{
    (void)size;(void)user;
    last_pc=addr;
    /* Do not execute data, return into scratch, or jump to BootROM. */
    if(addr<BASE || addr>=BASE+0x10530){
        fprintf(stderr,"Unexpected instruction address 0x%08" PRIx64 "\n",addr);
        bad=1;uc_emu_stop(uc);
    }
}
static bool invalid(uc_engine *uc,uc_mem_type type,uint64_t addr,
                    int size,int64_t value,void *user)
{
    (void)uc;(void)value;(void)user;
    fprintf(stderr,"Blocked unmapped/protected access type=%d PC=0x%08" PRIx64
            " addr=0x%08" PRIx64 " size=%d\n",(int)type,last_pc,addr,size);
    bad=1;return false;
}
static int dump(uc_engine *uc,const char *name,uint64_t addr,unsigned n)
{
    unsigned char b[0x358];
    if(n>sizeof b || !check(uc_mem_read(uc,addr,b,n),"read output"))return 0;
    printf("[%s] addr=0x%08" PRIx64 " bytes=%u\n",name,addr,n);
    for(unsigned i=0;i<n;i+=4)printf("%s+%03x=%08" PRIx32 "\n",name,i,le32(b+i));
    return 1;
}
int main(int argc,char **argv)
{
    if(argc!=2){
        fprintf(stderr,"Usage: %s exact-v1.18-ddr.bin\n",argv[0]);return 2;
    }
    FILE *f=fopen(argv[1],"rb");
    if(!f){perror(argv[1]);return 1;}
    size_t n=fread(blob,1,sizeof blob,f);
    int extra=fgetc(f),io_error=ferror(f);
    fclose(f);
    if(n!=SIZE || extra!=EOF || io_error ||
       le32(blob+0x115d0)!=0x12345678U ||
       le32(blob+0x6dc4)!=0xa9bc7bfdU ||
       le32(blob+0xd9f0)!=0xa9b97bfdU ||
       le32(blob+0x11618)!=0x00005100U){
        fputs("Unexpected binary layout; refusing to run\n",stderr);return 1;
    }
    uc_engine *uc=NULL;
    if(!check(uc_open(UC_ARCH_ARM64,UC_MODE_ARM,&uc),"uc_open"))return 1;
    int ok=0;
    uc_hook code,mem;
    if(!check(uc_mem_map(uc,SRAM,SRAM_SIZE,UC_PROT_ALL),"map synthetic SRAM")||
       !check(uc_mem_write(uc,BASE,blob,SIZE),"load blob")||
       !check(uc_hook_add(uc,&code,UC_HOOK_CODE,(void *)trace,NULL,1,0),"code hook")||
       !check(uc_hook_add(uc,&mem,UC_HOOK_MEM_INVALID,(void *)invalid,NULL,1,0),"memory hook"))
        goto done;
    /* Main cold call at 0x9bfc..0x9c18: w5=0 initializes geometry. */
    uint64_t x[6]={R,P,T,528,8,0},sp=STACK,lr=STOP,pc=0,ret=0;
    const int regs[]={UC_ARM64_REG_X0,UC_ARM64_REG_X1,UC_ARM64_REG_X2,
                     UC_ARM64_REG_X3,UC_ARM64_REG_X4,UC_ARM64_REG_X5};
    for(unsigned i=0;i<6;i++)if(!check(uc_reg_write(uc,regs[i],&x[i]),"arg"))goto done;
    if(!check(uc_reg_write(uc,UC_ARM64_REG_SP,&sp),"SP")||
       !check(uc_reg_write(uc,UC_ARM64_REG_X30,&lr),"LR"))goto done;
    uc_err err=uc_emu_start(uc,BASE+0x6dc4,STOP,5000000,2000000);
    if(!check(err,"emulate generator")||bad||
       !check(uc_reg_read(uc,UC_ARM64_REG_PC,&pc),"PC")||
       !check(uc_reg_read(uc,UC_ARM64_REG_X0,&ret),"return")||
       pc!=STOP || (uint32_t)ret!=0){
        fprintf(stderr,"Generator not confirmed: PC=%08" PRIx64 " ret=%08" PRIx32 "\n",pc,(uint32_t)ret);
        goto done;
    }
    unsigned char r[0x240],p[0x94],t[0x2ac];
    if(!check(uc_mem_read(uc,R,r,sizeof r),"read R")||
       !check(uc_mem_read(uc,P,p,sizeof p),"read P")||
       !check(uc_mem_read(uc,T,t,sizeof t),"read T"))goto done;
    if(le32(r+0x1b0)!=528 || le32(r+0x1b4)!=8 ||
       le32(r+0x228)!=(uint32_t)P || le32(r+0x22c)!=0 ||
       le32(r+0x230)!=(uint32_t)T || le32(r+0x234)!=0 ||
       le32(p+0x28)!=8 || le32(p+0x2c)!=528){
        fputs("Generator returned but output identity checks failed\n",stderr);goto done;
    }
    puts("OFFLINE GENERATOR RETURN PASS (not DDR hardware initialization)");
    /* Independently decoded 0x7010 geometry, prior to any training. */
    static const uint32_t geometry[15]={2,11,3,1,0,0,17,17,0,0,0,0,0,0,0};
    for(unsigned ch=0;ch<4;++ch)
        for(unsigned i=0;i<15;++i)
            if(le32(r+ch*0x6c+i*4)!=geometry[i]){
                fprintf(stderr,"Initial geometry mismatch: ch%u +%02x actual=%08" PRIx32 " expected=%08" PRIx32 "\n",
                        ch,i*4,le32(r+ch*0x6c+i*4),geometry[i]);
                goto done;
            }
    puts("Initial geometry matches independently decoded template");
    printf("Timing bucket: T+1e8=%" PRIu32 " T+1ec=%" PRIu32
           " T+1f0=%" PRIu32 " T+1f4=%" PRIu32 "\n",
           le32(t+0x1e8),le32(t+0x1ec),le32(t+0x1f0),le32(t+0x1f4));
    ok=dump(uc,"R",R,0x358)&&dump(uc,"P",P,0x94)&&dump(uc,"T",T,0x2ac);
done:
    uc_close(uc);
    return ok?0:1;
}
