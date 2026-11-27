/* Native host utility for OpenBoot SRAM v0.4.0+; run needs API 2.
 * No serial access or storage writes. Explicit run on a trusted USB link.
 */
#define _POSIX_C_SOURCE 200809L
#include <libusb.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

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
static int api_info(libusb_device_handle *h,uint32_t info[4])
{
    unsigned char buf[16];
    if(!req(h,0xc0,0x58,0,buf,sizeof buf))return 0;
    for(unsigned i=0;i<4;i++)info[i]=get32(buf+4*i);
    return info[0]==2 &&
        ((info[1]==0xff001000U && info[2]==0xff011000U) ||
         (info[1]==0xff011000U && info[2]==0xff001000U));
}
static double monotime(void)
{
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;
    return (double)t.tv_sec+(double)t.tv_nsec/1e9;
}
/* Refuse an ambiguous test VID/PID instead of selecting an arbitrary board. */
static libusb_device_handle *open_one(libusb_context *ctx)
{
    libusb_device **list=NULL;
    ssize_t n=libusb_get_device_list(ctx,&list);
    if(n<0)return NULL;
    libusb_device *found=NULL;
    unsigned count=0;
    for(ssize_t i=0;i<n;i++){
        struct libusb_device_descriptor d;
        if(!libusb_get_device_descriptor(list[i],&d) &&
           d.idVendor==0x1209 && d.idProduct==1){
            found=list[i];++count;
        }
    }
    libusb_device_handle *h=NULL;
    if(count==1 && libusb_open(found,&h)<0)h=NULL;
    libusb_free_device_list(list,1);
    return h;
}
static int wait_slot(libusb_context *ctx,uint32_t target,uint8_t bus,
                     const uint8_t *ports,int depth)
{
    /* Do not resend RUN on a timeout. Wait for the new firmware instead. */
    double deadline=monotime()+20.0;
    for(unsigned i=0;i<80 && monotime()<deadline;i++){
        struct timespec delay={0,250000000};
        nanosleep(&delay,NULL);
        libusb_device_handle *h=open_one(ctx);
        if(!h)continue;
        libusb_device *dev=libusb_get_device(h);
        uint8_t path[8];
        int d=libusb_get_port_numbers(dev,path,sizeof path);
        int cfg=0;
        uint32_t info[4];
        int ok=libusb_get_bus_number(dev)==bus && d==depth &&
            d>0 && !memcmp(path,ports,(size_t)d) &&
            !libusb_get_configuration(h,&cfg) && cfg==1 &&
            ping(h) && api_info(h,info) && info[1]==target;
        libusb_close(h);
        if(ok){
            printf("SRAM USB RUN PASS; active slot=0x%08" PRIx32 "\n",target);
            return 1;
        }
    }
    fputs("RUN not confirmed: check USB passthrough and UART. Do not resend blindly.\n",stderr);
    return 0;
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
    double start=monotime();
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
    double elapsed=monotime()-start;
    if(elapsed>0)printf("Transfer + verify: %.3f s, %.1f KiB/s\n",elapsed,(double)n/1024.0/elapsed);
    return 1;
}
static void usage(const char *name)
{
    printf("Usage:\n  %s ping\n  %s status\n  %s upload sram.bin\n"
           "  %s run sram.bin\n  %s selftest\n",name,name,name,name,name);
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
    if(argc==3 && !strcmp(argv[1],"run"))mode=4;
    if(!mode){usage(argv[0]);return 2;}
    size_t n=0;uint32_t crc=0;
    if(mode>=3){
        if(!read_image(argv[2],&n))return 1;
        crc=crc32(image,n);
        printf("image=%s size=%zu CRC32=%08" PRIx32 "\n",argv[2],n,crc);
        fflush(stdout);
    }
    libusb_context *ctx=NULL;
    int rc=libusb_init(&ctx),ok=0;
    if(rc<0){fprintf(stderr,"libusb_init: %s\n",libusb_error_name(rc));return 1;}
    libusb_device_handle *h=open_one(ctx);
    if(!h){fputs("Need exactly one accessible 1209:0001 device\n",stderr);goto done;}
    int cfg=0;
    rc=libusb_get_configuration(h,&cfg);
    if(rc<0 || cfg!=1){fputs("USB not configured\n",stderr);goto done;}
    if(!ping(h))goto done;
    if(mode==1){puts("SRAM USB EP0 PING PASS");ok=1;}
    else if(mode==2){uint32_t s[4];if(status(h,s)){print_status(s);ok=1;}}
    else if(mode==3){
        ok=upload(h,n,crc);
        if(ok)puts("Use UART: loadstat, then run.");
    }else{
        uint32_t info[4];
        unsigned char dummy=0;
        if(!api_info(h,info) || !info[3]){
            fputs("Remote run needs SRAM API 2 and usbremote ON. Use manual upload on v0.4.0.\n",stderr);
            goto done;
        }
        libusb_device *dev=libusb_get_device(h);
        uint8_t bus=libusb_get_bus_number(dev),ports[8];
        int depth=libusb_get_port_numbers(dev,ports,sizeof ports);
        if(depth<=0){fputs("Cannot identify USB port; refuse automatic RUN\n",stderr);goto done;}
        uint32_t target=info[2];
        if(!req(h,0x40,0x56,0,&dummy,0)||!upload(h,n,crc))goto done;
        printf("Starting slot 0x%08" PRIx32 "; waiting for re-enumeration...\n",target);
        fflush(stdout);
        int ack=req(h,0x40,0x57,0,&dummy,0);
        libusb_close(h);h=NULL;
        if(!ack)fputs("RUN acknowledgement uncertain; checking new slot, not retrying\n",stderr);
        ok=wait_slot(ctx,target,bus,ports,depth);
    }
done:
    if(h)libusb_close(h);
    libusb_exit(ctx);
    if(!ok && mode==3)
        fputs("No automatic retry. Re-arm with usbload in UART before retrying.\n",stderr);
    return ok?0:1;
}
