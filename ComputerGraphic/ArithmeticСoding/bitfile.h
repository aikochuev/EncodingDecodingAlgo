#ifndef _BITFILE_H_
#define _BITFILE_H_

#include <stdio.h>

typedef enum
{
    BF_READ = 0,
    BF_WRITE = 1,
    BF_APPEND= 2,
    BF_NO_MODE
} BF_MODES;

struct bit_file_t;
typedef struct bit_file_t bit_file_t;

bit_file_t *MakeBitFile(FILE *stream, const BF_MODES mode);
int BitFileClose(bit_file_t *stream);
FILE *BitFileToFILE(bit_file_t *stream);

int BitFileGetChar(bit_file_t *stream);
int BitFilePutChar(const int c, bit_file_t *stream);

int BitFileGetBit(bit_file_t *stream);
int BitFilePutBit(const int c, bit_file_t *stream);

int BitFileGetBits(bit_file_t *stream, void *bits, const unsigned int count);
int BitFilePutBits(bit_file_t *stream, void *bits, const unsigned int count);

int BitFileGetBitsNum(bit_file_t *stream, void *bits, const unsigned int count,
    const size_t size);
int BitFilePutBitsNum(bit_file_t *stream, void *bits, const unsigned int count,
    const size_t size);

#endif /* _BITFILE_H_ */
