// TextFilter 3.0 for PAQ (based on WRT 4.6) by P.Skibinski, 02.03.2006, inikep@o2.pl

#include <stdio.h>

#define CHAR_FIRSTUPPER   64   // for encode lower word with first capital letter
#define CHAR_UPPERWORD     7   // for encode upper word
#define CHAR_LOWERWORD     6   // for encode lower word with a few capital letter
#define CHAR_NOSPACE       8   // the same as CHAR_PUNCTUATION
#define CHAR_ESCAPE       12   // for encode reserved chars (CHAR_ESCAPE,CHAR_FIRSTUPPER,...)

#define BINARY_FIRST     128
#define BINARY_LAST      255
#define WORD_MIN_SIZE      1

#define TOLOWER(c)    ((c>='A' && c<='Z')?(c+32):c)
#define TOUPPER(c)    ((c>='a' && c<='z')?(c-32):c)

#define HASH_TABLE_SIZE        (1<<21)
int word_hash[HASH_TABLE_SIZE];

bool fileCorrupted=false;

typedef unsigned int  uint;
typedef unsigned char uc;

static uint flen( FILE* f ) {
    fseek( f, 0, SEEK_END );
    uint len = ftell(f);
    fseek( f, 0, SEEK_SET );
    return len;
}

class WRT {
public:

WRT() : dict(NULL), dictlen(NULL), dictmem(NULL) { 
};
~WRT() { 
   WRT_deinitialize(); 
}

enum EWordType { LOWERWORD, FIRSTUPPER, UPPERWORD };
enum EUpperType { UFALSE, UTRUE, FORCE };

int tryShorterBound=6,s_size,WRTd_c,WRTd_qstart,WRTd_qend,WRTd_type=0;
int fftelld,originalFileLen;

bool WRTd_upper;
unsigned char WRTd_s[1024];
unsigned char WRTd_queue[128];
EUpperType upperWord;

#define DECODE_GETC(c,file)\
{\
    if (fftelld<originalFileLen) \
    { \
        c=getc(file); \
        fftelld++; \
    } \
    else \
        c=EOF; \
}


#define HASH_DOUBLE_MULT    29
#define HASH_MULT        23

int sizeDict;
unsigned char** dict;
unsigned char* dictlen;
unsigned char* dictmem;

int reservedSet[256]; 
int addSymbols[256]; 
int sym2codeword[256]; 
int codeword2sym[256]; 

int dictionary=1<<30,dict1size,dict2size,dict3size,dict1plus2;
int dict12size;


// convert upper string to lower
inline void toLower(unsigned char* s,int s_size) {
    for (int i=0; i<s_size; i++)
        s[i]=TOLOWER(s[i]);
}

// convert lower string to upper
inline void toUpper(unsigned char* s,int s_size) {
    for (int i=0; i<s_size; i++)
        s[i]=TOUPPER(s[i]); 
}

// make hash from string
inline unsigned int stringHash(const unsigned char *ptr, int len) {
    unsigned int hash;
    for (hash = 0; len>0; len--, ptr++)
        hash = HASH_MULT * hash + *ptr;
 
    return hash&(HASH_TABLE_SIZE-1);
}

// check if word "s" does exist in the dictionary using hash "h" 
inline int checkHashExactly(const unsigned char* s,int s_size,int h) {
    int i;

    i=word_hash[h];
    if (i>0) {
        if (dictlen[i]!=s_size || memcmp(dict[i],s,s_size)!=0) {
            i=word_hash[(h+s_size*HASH_DOUBLE_MULT)&(HASH_TABLE_SIZE-1)];
            if (i>0) {
                if (dictlen[i]!=s_size || memcmp(dict[i],s,s_size)!=0) {
                    i=word_hash[(h+s_size*HASH_DOUBLE_MULT*HASH_DOUBLE_MULT)&(HASH_TABLE_SIZE-1)];
                    if (i>0) {
                        if (dictlen[i]!=s_size || memcmp(dict[i],s,s_size)!=0)
                            i=-1;
                    } else
                        i=-1;
                }
            } else
                i=-1;
        }
    } else
        i=-1;

    if (i>dictionary)
        i=-1;

    return i;
}

// check if word "s" (prefix of original word) does exist in the dictionary using hash "h" 
inline int checkHash(const unsigned char* s,int s_size,int h) {
    int i;

    i=word_hash[h];
    if (i>0) {
        if (dictlen[i]>s_size || memcmp(dict[i],s,s_size)!=0) {
            i=word_hash[(h+s_size*HASH_DOUBLE_MULT)&(HASH_TABLE_SIZE-1)];
            if (i>0) {
                if (dictlen[i]>s_size || memcmp(dict[i],s,s_size)!=0) {
                    i=word_hash[(h+s_size*HASH_DOUBLE_MULT*HASH_DOUBLE_MULT)&(HASH_TABLE_SIZE-1)];
                    if (i>0) {
                        if (dictlen[i]>s_size || memcmp(dict[i],s,s_size)!=0)
                            i=-1;
                    } else
                        i=-1;
                }
            } else
                i=-1;
        }
    } else
        i=-1;

    if (i>dictionary)
        i=-1;

    return i;
}


// check if word "s" or prefix of word "s" does exist in the dictionary using hash "h" 
inline int findShorterWord(const unsigned char* s,int s_size) {
    int ret, i, best;
    unsigned int hash;

    hash = 0;
    for (i=0; i<WORD_MIN_SIZE+tryShorterBound; i++)
        hash = HASH_MULT * hash + s[i];
 
    best=-1;
    for (; i<s_size; i++) {
        ret=checkHash(s,i,hash&(HASH_TABLE_SIZE-1));    
        if (ret>=0)
            best=ret;
        hash = HASH_MULT * hash + s[i];
    }

    return best;
}

inline int findShorterWordRev(const unsigned char* s,int s_size) {
    int ret, i;

    for (i=s_size-1; i>=WORD_MIN_SIZE+tryShorterBound; i--) {
        ret=checkHash(s+s_size-i,i,stringHash(s+s_size-i,i));    
        if (ret>=0)
            return ret;
    }
    return -1;
}

// encode word (should be lower case) using n-gram array (when word doesn't exist in the dictionary)
inline void encodeAsText(unsigned char* s,int s_size,FILE* fileout) {
    int i;
    for (i=0; i<s_size; ) {
        putc(s[i],fileout); //ENCODE_PUTC(,fileout);
        i++;
    }
}


inline void encodeCodeWord(int i,FILE* fileout) {
    int first,second,third;

    first=i-1;

    if (first>=80*49) {//bound3)
    
        first-=80*49; //bound3;

        third=first/dict12size;        
        first=first%dict12size;
        second=first/dict1size;        
        first=first%dict1size;

        putc(sym2codeword[dict1plus2+third],fileout);
        putc(sym2codeword[dict1size+second],fileout);
        putc(sym2codeword[first],fileout);
    } else
        if (first>=dict1size) {
            first-=dict1size;

            second=first/dict1size;        
            first=first%dict1size;

            putc(sym2codeword[dict1size+second],fileout);
            putc(sym2codeword[first],fileout);
        } else {
            putc(sym2codeword[first],fileout);
        }
}

// encode word "s" using dictionary
inline void encodeWord(FILE* fileout,unsigned char* s,int s_size,EWordType wordType) {
    int i,j,d,e;
    int size=0;
    int flagToEncode=-1;

    if (s_size<1) {
        return;
    }

    s[s_size]=0;

    if (wordType!=LOWERWORD) {
        if (wordType==FIRSTUPPER) {
            flagToEncode=CHAR_FIRSTUPPER;
            s[0]=TOLOWER(s[0]);
        } else { // wordType==UPPERWORD
            flagToEncode=CHAR_UPPERWORD;
            toLower(s,s_size);
        }
    }

    if (s_size>=WORD_MIN_SIZE) {
        i=checkHashExactly(s,s_size,stringHash(s,s_size));

        if (i<0) {
            // try to find shorter version of word in dictionary
            i=findShorterWord(s,s_size);
            j=findShorterWordRev(s,s_size);

            d=e=0;
            if (i>=0) d=dictlen[i]-(i>80)-(i>3920)-1;
            if (j>=0) e=dictlen[j]-(j>80)-(j>3920)-1;
            if (d>=e) { 
                if (d> 0) size=dictlen[i]; 
            } else {
                    i=j;
                    if (e> 0) {
                        if (wordType!=LOWERWORD) {
                            putc(flagToEncode,fileout);
                            wordType=LOWERWORD;
                        }
                        
                        s[s_size-dictlen[i]]=0;
                        encodeAsText(s,s_size-dictlen[i],fileout);
                        putc(CHAR_NOSPACE,fileout);
                        s+=dictlen[i];
                        s_size-=dictlen[i];
                    }
                }
        }
    } else
        i=-1;

    if (i>=0) {
        if (wordType!=LOWERWORD) {
            putc(flagToEncode,fileout);
        }
        encodeCodeWord(i,fileout);
        if (size>0)encodeAsText(s+size,s_size-size,fileout);
    } else {
        if (wordType!=LOWERWORD) {
            putc(flagToEncode,fileout);
        }
 
        encodeAsText(s,s_size,fileout);
    }
}

// decode word using dictionary
#define DECODE_WORD(dictNo,i)\
{\
        switch (dictNo)\
        {\
            case 3:\
                i+=80*49; /*bound3;*/\
                break;\
            case 2:\
                i+=dict1size;\
                break;\
        }\
\
        if (i>=0 && i<sizeDict)\
        {\
            i++;\
            s_size=dictlen[i];\
            memcpy(s,dict[i],s_size+1);\
        }\
        else\
        {\
            printf("File is corrupted!\n");\
            fileCorrupted=true;\
        }\
}


inline int decodeCodeWord(FILE* file, unsigned char* s,int& c) {
    int i,dictNo,s_size=0;

    if (codeword2sym[c]<dict1size) {
        i=codeword2sym[c];
        dictNo=1;
        DECODE_WORD(dictNo, i);
        return s_size;
    }

    i=dict1size*(codeword2sym[c]-dict1size);

    DECODE_GETC(c,file);

    if (codeword2sym[c]<dict1size) {
        i+=codeword2sym[c];
        dictNo=2;
        DECODE_WORD(dictNo, i);
        return s_size;
    }

    i=(i-dict12size)*dict2size;
    i+=dict1size*(codeword2sym[c]-dict1size);

    DECODE_GETC(c,file);
    i+=codeword2sym[c];
    dictNo=3;
    DECODE_WORD(dictNo, i);
    return s_size;
}

unsigned char* loadDictionary(FILE* file,unsigned char* mem,int word_count) {
    unsigned char* word;
    int i,j,c,collision,bound;

    collision=0;
    bound=sizeDict+word_count;

    while (!feof(file)) {
        word=mem;
        do {
            c=getc(file);
           word[0]=c;
            word++;
        } while (c>32);

        if (c==EOF)
            break;
        if (c=='\r') 
            c=getc(file);
        
        word[-1]=0;
        i=word-mem-1;
        
        dictlen[sizeDict]=i;
        dict[sizeDict]=mem;


        j=stringHash(mem,i);
        mem+=(i/4+1)*4;
        

        if (word_hash[j]!=0) {
            if (dictlen[sizeDict]!=dictlen[word_hash[j]] || memcmp(dict[sizeDict],dict[word_hash[j]],dictlen[sizeDict])!=0) {
                c=(j+i*HASH_DOUBLE_MULT)&(HASH_TABLE_SIZE-1);
                if (word_hash[c]!=0) {
                    if (dictlen[sizeDict]!=dictlen[word_hash[c]] || memcmp(dict[sizeDict],dict[word_hash[c]],dictlen[sizeDict])!=0) {
                        c=(j+i*HASH_DOUBLE_MULT*HASH_DOUBLE_MULT)&(HASH_TABLE_SIZE-1);
                        if (word_hash[c]!=0) {
                            collision++;
                        } else {
                                word_hash[c]=sizeDict++;
                        }
                    }
                } else {
                            word_hash[c]=sizeDict++;
                }
            }
        } else {
                word_hash[j]=sizeDict++;
        }
        
        if (sizeDict>dictionary || sizeDict>=bound) {
            sizeDict--;
            break;
        }
        
    }

    return mem;
}

void initializeCodeWords() {
    int c,charsUsed;

    for (c=0; c<256; c++){
        addSymbols[c]=0;
        codeword2sym[c]=0;
        sym2codeword[c]=0;
        reservedSet[c]=0;
    }

    for (c=BINARY_FIRST; c<=BINARY_LAST; c++)
        addSymbols[c]=1;


    for (c=0; c<256; c++) {
        if ( (addSymbols[c])
            || c==CHAR_ESCAPE || c==CHAR_LOWERWORD || c==CHAR_FIRSTUPPER || c==CHAR_UPPERWORD
            || c==CHAR_NOSPACE)
            reservedSet[c]=1;
        else
            reservedSet[c]=0;
    }

        charsUsed=0;
        for (c=0; c<256; c++) {
            if (addSymbols[c]) {
                codeword2sym[c]=charsUsed;
                sym2codeword[charsUsed]=c;
                charsUsed++;

            }
        }
        dict1size=80;
        dict2size=32;
        dict3size=16;

        int dict123size=dict1size*dict2size*dict3size;
        dict12size=dict1size*dict2size;
        dict1plus2=dict1size+dict2size;

        dictionary=dict123size + dict1size*(dict2size+dict3size+1);
        
        dict=(unsigned char**)calloc(sizeof(unsigned char*)*(dictionary+1),1);
        dictlen=(unsigned char*)calloc(sizeof(unsigned char)*(dictionary+1),1);
 
}

// read dictionary from files to arrays
bool initialize(FILE* file) {
    unsigned char* mem;

    WRT_deinitialize();
    sizeDict=0;

    memset(&word_hash[0],0,HASH_TABLE_SIZE*sizeof(word_hash[0]));

    if (file==NULL) {
        initializeCodeWords();
        //parse without dictionary
        return true;
    }else{

        int fileLen=flen(file);

        dictmem=(unsigned char*)calloc(fileLen*2,1);
        mem=dictmem;
        sizeDict=1;

        if (!dictmem) {
            initializeCodeWords();
            return true;
        }

        initializeCodeWords();

        if (dict==NULL || dictlen==NULL)
            return false;

        mem=loadDictionary(file,mem,dictionary);
        fclose(file);

    }
    return true;
}

void WRT_deinitialize() {
    if (dict) {
        free(dict);
        dict=NULL;
    }
    if (dictlen) {
        free(dictlen);
        dictlen=NULL;
    }
    if (dictmem) {
        free(dictmem);
        dictmem=NULL;
    }

    sizeDict=0;
}

// preprocess the file
void WRT_encode(FILE* file,FILE* fileout) {
    unsigned char s[1024];
    EWordType wordType;
    int c,next_c;

    s_size=0;
    wordType=LOWERWORD;

    c=getc(file);

    while (!feof(file)) {
        if (fileCorrupted)
            return;

        if (reservedSet[c]) {
            encodeWord(fileout,s,s_size,wordType);
            s_size=0;
            putc(CHAR_ESCAPE,fileout);
            putc(c,fileout);

            c=getc(file);
            continue;
        }

        if (c>='a' && c<='z') {
            if (s_size==0) {
                wordType=LOWERWORD;
            } else {
                if (wordType==UPPERWORD) {
                    encodeWord(fileout,s,s_size,wordType);
                        putc(CHAR_LOWERWORD,fileout);

                    wordType=LOWERWORD;
                    s_size=1;
                    s[0]=c;
                    c=getc(file);
                    continue;
                }
            }

            s[s_size++]=c;
            if (s_size>=(sizeof(s)-1)) {
                encodeWord(fileout,s,s_size,wordType);
                s_size=0;
            }
            c=getc(file);
            continue;
        }

        if (c<='Z' && c>='A' ) {
            if (s_size==0) {
                wordType=FIRSTUPPER;
            } else {
                if (wordType==FIRSTUPPER) {
                    if (s_size==1)
                        wordType=UPPERWORD;
                    else {
                        encodeWord(fileout,s,s_size,wordType);

                        wordType=FIRSTUPPER;
                        s_size=1;
                        s[0]=c;
                        c=getc(file);
                        continue;
                    }
                } else if (wordType==LOWERWORD) {
                    encodeWord(fileout,s,s_size,wordType);

                    wordType=FIRSTUPPER;
                    s_size=1;
                    s[0]=c;
                    c=getc(file);
                    continue;
                }
            }

            s[s_size++]=c;
            if (s_size>=(sizeof(s)-1)) {
                encodeWord(fileout,s,s_size,wordType);
                s_size=0;
            }

            c=getc(file);
            continue;
        }

        encodeWord(fileout,s,s_size,wordType);
        s_size=0;
        next_c=getc(file);

            if (c!=EOF) {
               putc(c,fileout);
            }
        c=next_c;
    }
    encodeWord(fileout,s,s_size,wordType);
    s_size=0;
}

inline void WRT_decode(FILE* file) {
        if (addSymbols[WRTd_c] ) {
            s_size=decodeCodeWord(file,WRTd_s,WRTd_c);
            if (WRTd_upper) {
                WRTd_upper=false;
                WRTd_s[0]=TOUPPER(WRTd_s[0]);
            }

            if (upperWord!=UFALSE)
                toUpper(WRTd_s,s_size);

            for (int i=0; i<s_size; i++) {
                WRTd_queue[WRTd_qend++]=WRTd_s[i];
            }

            DECODE_GETC(WRTd_c,file);
            return;
        }

        if (reservedSet[WRTd_c]) {
            if (WRTd_c==CHAR_ESCAPE) {
                WRTd_upper=false;
                upperWord=UFALSE;

                DECODE_GETC(WRTd_c,file);
                 WRTd_queue[WRTd_qend++]=WRTd_c;

                DECODE_GETC(WRTd_c,file);
                return;
            }

            if (WRTd_c==CHAR_NOSPACE) {
                if (upperWord==FORCE)
                    upperWord=UTRUE;
                DECODE_GETC(WRTd_c,file);
                return;
            }

            if (WRTd_c==CHAR_FIRSTUPPER) {
                WRTd_upper=true;
                upperWord=UFALSE;
                DECODE_GETC(WRTd_c,file);
                return;
            }
            
            if (WRTd_c==CHAR_UPPERWORD) {
                upperWord=FORCE;
                DECODE_GETC(WRTd_c,file);
                return;
            }
            
            if (WRTd_c==CHAR_LOWERWORD) {
                WRTd_upper=false;
                upperWord=UFALSE;
                DECODE_GETC(WRTd_c,file);
                if (WRTd_c==' ') {  // skip space
                    DECODE_GETC(WRTd_c,file);
                }
                return;
            }
        }

        if (upperWord!=UFALSE) {
            if (upperWord==FORCE)
                upperWord=UTRUE;

            if ((WRTd_c>='a' && WRTd_c<='z') )
                WRTd_c=TOUPPER(WRTd_c);
            else
                upperWord=UFALSE;
        }
        else
        if (WRTd_upper==true) {
            WRTd_upper=false;
            WRTd_c=TOUPPER(WRTd_c);
        }

        WRTd_queue[WRTd_qend++]=WRTd_c;

        DECODE_GETC(WRTd_c,file);
        return;
}

void WRT_start_encoding(FILE* file,FILE* fileout,FILE* dict) {
    int pos=ftell(fileout);
    fprintf(fileout, "%c%c%c%c", 0,0,0,0);

    WRT_deinitialize();

    if (!initialize(dict))
        return;

    WRT_encode(file,fileout); 

    unsigned int fileLen=ftell(fileout)+4;
    fseek(fileout, pos, SEEK_SET );
    fprintf(fileout, "%c%c%c%c", fileLen>>24, fileLen>>16, fileLen>>8, fileLen);
    fseek(fileout, fileLen-4, SEEK_SET );
}

void WRT_start_decoding(FILE* file,int header,FILE* dict) {
    int i;
    unsigned int fileLen;

    for (i=0, fileLen=0; i<4; i++)
        fileLen=fileLen*256+fgetc(file);

    i=0;
   
    header+=4;
    i+=header; // WRT4
    
    WRT_deinitialize();

    if (!initialize(dict))
        return;
        
    int EOLlen= 4;

    fseek(file, i, SEEK_SET ); // skip "WRTx" header
    EOLlen+=i;    // header + fileLen

    originalFileLen=fileLen-EOLlen;
    fftelld=0;
    WRTd_upper=false;
    upperWord=UFALSE;
    s_size=0;

    DECODE_GETC(WRTd_c,file);
}

int WRT_decode_char(FILE* file,int header,FILE* dict) {
    switch (WRTd_type) {
        default:
        case 0:
            WRT_start_decoding(file,header,dict);
            WRTd_qstart=WRTd_qend=0;
            WRTd_type=1;
        case 1:
            if (WRTd_c!=EOF) {
                while (WRTd_qstart>=WRTd_qend && WRTd_c!=EOF) {
                    WRTd_qstart=WRTd_qend=0;
                    WRT_decode(file);
                    if (fileCorrupted)
                        WRTd_type=2;
                }
                if (WRTd_qstart<WRTd_qend)
                    return WRTd_queue[WRTd_qstart++];
            }
            WRTd_type=2;
        case 2:
            if (WRTd_qstart<WRTd_qend)
                return WRTd_queue[WRTd_qstart++];
            else
                return -1;
    }
}

};
