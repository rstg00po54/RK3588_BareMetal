/* Native host utility for OpenBoot SRAM v0.4.0+.
 * No serial access, no storage writes, no remote execution.
 */
#include <libusb.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_IMAGE=65536, CHUNK=64, TIMEOUT=3000 };
static unsigned char image[MAX_IMAGE+1];

static uint32_t crc32(const unsigned char *p,size_t n)
{
    uint32_t crc=UINT32_C(0xffffffff);
    while(n--){
        crc^=*p++;
        for(unsigned k=0;k<8;k++)
            crc=(crc>>1)^(UINT32_C(0xedb88320)&(0U-(crc&1U)));
    }
    return crc^UINT32_C(0xffffffff);
}
static void put32(unsigned char *p,uint32_t v)
{
    for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(8*i));
}
static uint32_t get32(const unsigned char *p)
{
    uint32_t v=0;
    for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);
    return v;
}
static int req(libusb_device_handle *h,uint8_t bm,uint8_t cmd,
               uint16_t offset,unsigned char *data,uint16_t n)
{
    int rc=libusb_control_transfer(h,bm,cmd,offset,0,data,n,TIMEOUT);
    if(rc<0){
        fprintf(stderr,"request 0x%02x offset 0x%04x: %s\n",
                (unsigned)cmd,(unsigned)offset,libusb_error_name(rc));
        return 0;
    }
    if(rc!=n){
        fprintf(stderr,"request 0x%02x: short transfer %d/%u\n",
                (unsigned)cmd,rc,(unsigned)n);
        return 0;
    }
    return 1;
}
static int ping(libusb_device_handle *h)
{
    unsigned char buf[8];
    if(!req(h,0xc0,0x51,0,buf,sizeof buf))return 0;
    if(memcmp(buf,"SRAMUSB1",sizeof buf)){
        fprintf(stderr,"Unexpected device signature\n");return 0;
    }
    return 1;
}
static int status(libusb_device_handle *h,uint32_t s[4])
{
    unsigned char buf[16];
    if(!req(h,0xc0,0x55,0,buf,sizeof buf))return 0;
    for(unsigned i=0;i<4;i++)s[i]=get32(buf+4*i);
    return 1;
}
static void print_status(const uint32_t s[4])
{
    printf("state=%" PRIu32 " size=%" PRIu32 " received=%" PRIu32
           " CRC32=%08" PRIx32 "\n",s[0],s[1],s[2],s[3]);
}
static int read_image(const char *path,size_t *n)
{
    FILE *f=fopen(path,"rb");
    if(!f){perror(path);return 0;}
    *n=fread(image,1,sizeof image,f);
    int failed=ferror(f);
    fclose(f);
    if(failed){fprintf(stderr,"Image read failed\n");return 0;}
    if(*n<4 || *n>MAX_IMAGE || (*n&3)){
        fprintf(stderr,"Raw image must be 4..65536 bytes, word aligned\n");return 0;
    }
    if(!memcmp(image,"BOOT",4)||!memcmp(image,"LDR ",4)||
       !memcmp(image,"RKNS",4)||!memcmp(image,"\x7f""ELF",4)){
        fprintf(stderr,"Use raw sram.bin, not ELF or loader container\n");return 0;
    }
    return 1;
}
static int upload(libusb_device_handle *h,size_t n,uint32_t crc)
{
    uint32_t s[4];
    unsigned char hdr[8],dummy=0;
    if(!status(h,s))return 0;
    print_status(s);
    if(s[0]!=0){
        fprintf(stderr,"Not ready. Re-arm using usbload in UART.\n");return 0;
    }
    put32(hdr,(uint32_t)n);put32(hdr+4,crc);
    if(!req(h,0x40,0x52,0,hdr,sizeof hdr))return 0;
    for(size_t off=0;off<n;off+=CHUNK){
        size_t count=n-off<CHUNK?n-off:CHUNK;
        if(!req(h,0x40,0x53,(uint16_t)off,image+off,(uint16_t)count))return 0;
        if((off+count)%4096==0 || off+count==n){
            printf("staged %zu/%zu\n",off+count,n);fflush(stdout);
        }
    }
    if(!req(h,0x40,0x54,0,&dummy,0)||!status(h,s))return 0;
    print_status(s);
    if(s[0]!=2 || s[1]!=n || s[2]!=n || s[3]!=crc){
        fprintf(stderr,"Target verification failed\n");return 0;
    }
    puts("SRAM USB UPLOAD PASS; CRC32 verified on target.");
    puts("Use UART: loadstat, then run. No remote execution.");
    return 1;
}
static void usage(const char *name)
{
    printf("Usage:\n  %s ping\n  %s status\n  %s upload sram.bin\n"
           "  %s selftest\n",name,name,name,name);
}
int main(int argc,char **argv)
{
    if(argc==2 && !strcmp(argv[1],"selftest")){
        unsigned char b[4];
        put32(b,UINT32_C(0x12345678));
        if(crc32((const unsigned char *)"123456789",9)!=UINT32_C(0xcbf43926)||
           memcmp(b,"\x78\x56\x34\x12",4)||get32(b)!=UINT32_C(0x12345678)){
            fputs("SELFTEST FAIL\n",stderr);return 1;
        }
        puts("CRC32 / little-endian SELFTEST PASS");return 0;
    }
    if(argc==2 && (!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){
        usage(argv[0]);return 0;
    }
    int mode=0;
    if(argc==2 && !strcmp(argv[1],"ping"))mode=1;
    if(argc==2 && !strcmp(argv[1],"status"))mode=2;
    if(argc==3 && !strcmp(argv[1],"upload"))mode=3;
    if(!mode){usage(argv[0]);return 2;}
    size_t n=0;uint32_t crc=0;
    if(mode==3){
        if(!read_image(argv[2],&n))return 1;
        crc=crc32(image,n);
        printf("image=%s size=%zu CRC32=%08" PRIx32 "\n",argv[2],n,crc);
        fflush(stdout);
    }
    libusb_context *ctx=NULL;
    int rc=libusb_init(&ctx),ok=0;
    if(rc<0){fprintf(stderr,"libusb_init: %s\n",libusb_error_name(rc));return 1;}
    libusb_device_handle *h=libusb_open_device_with_vid_pid(ctx,0x1209,0x0001);
    if(!h){fputs("1209:0001 not found or permission denied\n",stderr);goto done;}
    int cfg=0;
    rc=libusb_get_configuration(h,&cfg);
    if(rc<0 || cfg!=1){fputs("USB not configured\n",stderr);goto done;}
    if(!ping(h))goto done;
    if(mode==1){puts("SRAM USB EP0 PING PASS");ok=1;}
    else if(mode==2){uint32_t s[4];if(status(h,s)){print_status(s);ok=1;}}
    else ok=upload(h,n,crc);
done:
    if(h)libusb_close(h);
    libusb_exit(ctx);
    if(!ok && mode==3)
        fputs("No automatic retry. Re-arm with usbload in UART before retrying.\n",stderr);
    return ok?0:1;
}
