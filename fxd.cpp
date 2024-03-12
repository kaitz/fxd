/* fxd - file dictionary transform.

    Copyright (C) 2023 Kaido Orav

    LICENSE

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details at
    Visit <http://www.gnu.org/copyleft/gpl.html>.

To compress:   fxd e input output
To decompress: fxd d input output

This program contains changed parts from fallowing open source programs:
paq8hp12->textfilter.hpp

Transform is not tested. Only upto 2G input.

*/

#define PROGNAME "fxd"  // Please change this if you change the program.

#include <stdio.h>
#include <string.h>
#include <time.h>

#define NDEBUG  // remove for debugging (turns on Array bound checks)
#include <assert.h>

#define VERSION 1
// 8, 16, 32, 64 bit unsigned types (adjust as appropriate)
typedef unsigned char  U8;
typedef unsigned short U16;
typedef unsigned int   U32;
typedef unsigned long long int U64;

// min, max functions
#ifndef min
inline int min(int a, int b) {return a<b?a:b;}
inline int max(int a, int b) {return a<b?b:a;}
#endif

// TextFilter 3.0 for PAQ (based on WRT 4.6) by P.Skibinski, 02.03.2006, inikep@o2.pl
#include "textfilter.hpp"
WRT wrt;

void put64(U64 x,FILE *out) {
    putc((x >> 56) & 255,out);
    putc((x >> 48) & 255,out);
    putc((x >> 40) & 255,out);
    putc((x >> 32) & 255,out);
    putc((x >> 24) & 255,out); 
    putc((x >> 16) & 255,out); 
    putc((x >> 8) & 255,out); 
    putc(x & 255,out);
}

U64 get64(FILE *in) {
    return ((U64)getc(in) << 56) | ((U64)getc(in) << 48) | ((U64)getc(in) << 40) | ((U64)getc(in) << 32) | (getc(in) << 24) | (getc(in) << 16) | (getc(in) << 8) | (getc(in));
}

int main(int argc, char** argv) {   
    // Check arguments
    if (argc<4 || (argv[1][0]!='e' && argv[1][0]!='d')) {
        printf(
        "fxd v%d file dictionary transform (C) 2023, Kaido Orav.\n"
        "Licensed under GPL, http://www.gnu.org/copyleft/gpl.html\n"
        "\n"
        "To compress:   fxd e input output dict\n"
        "To decompress: fxd d input output dict\n",VERSION);
        return 1;
    }

    // Get start time
    clock_t start=clock();

    // Open input file
    FILE *in=fopen(argv[2], "rb");
    if (!in) exit(1);
    FILE *out=0, *dict;
    dict=fopen(argv[4], "rb");
    if (argv[1][0]=='e') {        
        // Encode
        // Header: signature, version, file size
        fseeko(in, 0, SEEK_END);
        U64 size=ftello(in);
        fseeko(in, 0, SEEK_SET);
        out=fopen(argv[3], "wb");
        if (!out) exit(1);
        fprintf(out, "dF%c", VERSION);
        put64(size,out);
        wrt.WRT_start_encoding(in,out,dict); 
    } else {
        // Decode
        // Check header signature, version, file size
        if (getc(in)!='d' || getc(in)!='F' || getc(in)!=VERSION)
            printf("Not a fxd file."),exit(1);
        U64 size=get64(in);
        if (size==0) printf("Bad file size."),exit(1);
        out=fopen(argv[3], "wb");
        if (!out) exit(1);
        
        while (size-->0)
            putc(wrt.WRT_decode_char(in,3+8,dict), out);
    }

    printf("%ld -> %ld in %1.2f sec. %d MB memory.\n", 
    ftello(in), ftello(out), double(clock()-start)/CLOCKS_PER_SEC, 0);//U32(getPeakMemory()/1024)/1024
    fclose(in);
    fclose(out);
    return 0;
}
