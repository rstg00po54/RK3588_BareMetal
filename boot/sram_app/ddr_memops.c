/* Freestanding C runtime used by the dry-run app only. */
#include <stddef.h>
void *memset(void *dst,int val,size_t n){
    volatile unsigned char *d=dst;
    for(size_t i=0;i<n;i++)d[i]=(unsigned char)val;
    return dst;
}
void *memcpy(void *dst,const void *src,size_t n){
    volatile unsigned char *d=dst;const volatile unsigned char *s=src;
    for(size_t i=0;i<n;i++)d[i]=s[i];
    return dst;
}
