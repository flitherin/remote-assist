#ifndef MEDIA_CODEC_H
#define MEDIA_CODEC_H
#include <stddef.h>
static void media_hex(char *out,const unsigned char *in,size_t size) {
    static const char digits[]="0123456789abcdef";
    for(size_t i=0;i<size;i++) { out[2*i]=digits[in[i]>>4]; out[2*i+1]=digits[in[i]&15]; }
    out[2*size]=0;
}
static int media_unhex(unsigned char *out,size_t capacity,const char *in,size_t *size) {
    size_t n=0;
    while(*in) {
        unsigned int value=0;
        if(n==capacity) return 0;
        for(int j=0;j<2;j++) {
            unsigned char c=(unsigned char)*in;
            if(!c) return 0;
            ++in;
            if(c>='0' && c<='9') value=value*16+c-'0';
            else if(c>='a' && c<='f') value=value*16+c-'a'+10;
            else return 0;
        }
        out[n++]=(unsigned char)value;
    }
    *size=n; return 1;
}
#endif
