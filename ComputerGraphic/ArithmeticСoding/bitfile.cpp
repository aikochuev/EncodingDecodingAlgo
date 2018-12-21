#include "pch.h"
#include <stdlib.h>
#include <errno.h>
#include "bitfile.h"

typedef int(*num_func_t)(bit_file_t*, void*, const unsigned int, const size_t);

struct bit_file_t
{
    FILE *fp;
    unsigned char bitBuffer;
    unsigned char bitCount;
    num_func_t PutBitsNumFunc;
    num_func_t GetBitsNumFunc;
    BF_MODES mode;
};

typedef enum
{
    BF_UNKNOWN_ENDIAN,
    BF_LITTLE_ENDIAN,
    BF_BIG_ENDIAN
} endian_t;

typedef union
{
    unsigned long word;
    unsigned char bytes[sizeof(unsigned long)];
} endian_test_t;

static endian_t DetermineEndianess(void);

static int BitFilePutBitsLE(bit_file_t* stream, void* bits,
    const unsigned int count, const size_t size);
static int BitFilePutBitsBE(bit_file_t* stream, void* bits,
    const unsigned int count, const size_t size);

static int BitFileGetBitsLE(bit_file_t* stream, void* bits,
    const unsigned int count, const size_t size);
static int BitFileGetBitsBE(bit_file_t* stream, void* bits,
    const unsigned int count, const size_t size);
static int BitFileNotSupported(bit_file_t* stream, void* bits,
    const unsigned int count, const size_t size);

bit_file_t* MakeBitFile(FILE* stream, const BF_MODES mode)
{
    bit_file_t* bf;

    if(stream == nullptr)
    {
        errno = EBADF;
        bf = nullptr;
    }
    else
    {
        bf = new bit_file_t();
        bf->fp = stream;
        bf->bitBuffer = 0;
        bf->bitCount = 0;
        bf->mode = mode;

        switch(DetermineEndianess())
        {
        case BF_LITTLE_ENDIAN:
        {
            bf->PutBitsNumFunc = &BitFilePutBitsLE;
            bf->GetBitsNumFunc = &BitFileGetBitsLE;
            break;
        }
        case BF_BIG_ENDIAN:
        {
            bf->PutBitsNumFunc = &BitFilePutBitsBE;
            bf->GetBitsNumFunc = &BitFileGetBitsBE;
            break;
        }
        case BF_UNKNOWN_ENDIAN:
        default:
        {
            bf->PutBitsNumFunc = BitFileNotSupported;
            bf->GetBitsNumFunc = BitFileNotSupported;
            break;
        }
        }
    }
    return bf;
}

static endian_t DetermineEndianess(void)
{
    endian_t endian;
    endian_test_t endianTest;
    endianTest.word = 1;
    if(endianTest.bytes[0] == 1)
        endian = BF_LITTLE_ENDIAN;
    else if(endianTest.bytes[sizeof(unsigned long) - 1] == 1)
        endian = BF_BIG_ENDIAN;
    else
        endian = BF_UNKNOWN_ENDIAN;
    return endian;
}

FILE* BitFileToFILE(bit_file_t* stream)
{
    if(stream == nullptr)
        return nullptr;

    FILE* fp = nullptr;
    if((stream->bitCount != 0)
        && ((stream->mode == BF_WRITE) || (stream->mode == BF_APPEND)))
    {
        (stream->bitBuffer) <<= 8 - (stream->bitCount);
        fputc(stream->bitBuffer, stream->fp);
    }
    fp = stream->fp;
    delete stream;
    return fp;
}

int BitFileGetChar(bit_file_t* stream)
{
    if(stream == nullptr)
        return EOF;

    int returnValue = fgetc(stream->fp);

    if(stream->bitCount == 0)
        return returnValue;

    if(returnValue != EOF)
    {
        unsigned char tmp = ((unsigned char)returnValue) >> (stream->bitCount);
        tmp |= ((stream->bitBuffer) << (8 - (stream->bitCount)));
        stream->bitBuffer = returnValue;
        returnValue = tmp;
    }
    return returnValue;
}

int BitFilePutChar(const int c, bit_file_t* stream)
{
    if(stream == nullptr)
        return EOF;

    if(stream->bitCount == 0)
        return fputc(c, stream->fp);

    unsigned char tmp = ((unsigned char)c) >> (stream->bitCount);
    tmp = tmp | ((stream->bitBuffer) << (8 - stream->bitCount));

    if(fputc(tmp, stream->fp) != EOF)
        stream->bitBuffer = c;
    else
        return EOF;

    return tmp;
}

int BitFileGetBit(bit_file_t* stream)
{
    if(stream == nullptr)
        return EOF;

    int returnValue;
    if(stream->bitCount == 0)
    {
        if((returnValue = fgetc(stream->fp)) == EOF)
        {
            return EOF;
        }
        else
        {
            stream->bitCount = 8;
            stream->bitBuffer = returnValue;
        }
    }
    stream->bitCount--;
    returnValue = (stream->bitBuffer) >> (stream->bitCount);
    return (returnValue & 0x01);
}

int BitFilePutBit(const int c, bit_file_t* stream)
{
    if(stream == nullptr)
        return EOF;

    stream->bitCount++;
    stream->bitBuffer <<= 1;

    if(c != 0)
        stream->bitBuffer |= 1;

    int returnValue = c;
    if(stream->bitCount == 8)
    {
        if(fputc(stream->bitBuffer, stream->fp) == EOF)
            returnValue = EOF;

        stream->bitCount = 0;
        stream->bitBuffer = 0;
    }

    return returnValue;
}

int BitFileGetBits(bit_file_t* stream, void* bits, const unsigned int count)
{
    if((stream == nullptr) || (bits == nullptr))
        return EOF;

    int offset = 0;
    int remaining = count;
    int returnValue;
    unsigned char* bytes = (unsigned char*)bits;
    while(remaining >= 8)
    {
        returnValue = BitFileGetChar(stream);
        if(returnValue == EOF)
            return EOF;

        bytes[offset] = (unsigned char)returnValue;
        remaining -= 8;
        offset++;
    }

    if(remaining != 0)
    {
        unsigned char shifts = 8 - remaining;
        bytes[offset] = 0;

        while(remaining > 0)
        {
            returnValue = BitFileGetBit(stream);
            if(returnValue == EOF)
                return EOF;

            bytes[offset] <<= 1;
            bytes[offset] |= (returnValue & 0x01);
            remaining--;
        }
        bytes[offset] <<= shifts;
    }
    return count;
}

int BitFilePutBits(bit_file_t* stream, void* bits, const unsigned int count)
{
    unsigned char* bytes = (unsigned char*)bits;
    if((stream == nullptr) || (bits == nullptr))
        return(EOF);

    int offset = 0;
    int remaining = count;
    int returnValue;
    while(remaining >= 8)
    {
        returnValue = BitFilePutChar(bytes[offset], stream);
        if(returnValue == EOF)
            return EOF;

        remaining -= 8;
        offset++;
    }

    if(remaining != 0)
    {
        unsigned char tmp = bytes[offset];
        while(remaining > 0)
        {
            returnValue = BitFilePutBit((tmp & 0x80), stream);

            if(returnValue == EOF)
                return EOF;

            tmp <<= 1;
            remaining--;
        }
    }

    return count;
}

static int BitFileGetBitsLE(bit_file_t* stream, void* bits, const unsigned int count, const size_t size)
{
    int returnValue;
    unsigned char *bytes = (unsigned char*)bits;
    int offset = 0;
    int remaining = count;
    while(remaining >= 8)
    {
        returnValue = BitFileGetChar(stream);
        if(returnValue == EOF)
            return EOF;

        bytes[offset] = (unsigned char)returnValue;
        remaining -= 8;
        offset++;
    }

    if(remaining != 0)
    {
        while(remaining > 0)
        {
            returnValue = BitFileGetBit(stream);
            if(returnValue == EOF)
                return EOF;

            bytes[offset] <<= 1;
            bytes[offset] |= (returnValue & 0x01);
            remaining--;
        }

    }
    return count;
}

static int BitFileGetBitsBE(bit_file_t* stream, void* bits, const unsigned int count, const size_t size)
{
    if(count > (size * 8))
        return EOF;

    unsigned char* bytes = (unsigned char*)bits;
    int offset = size - 1;
    int remaining = count;
    int returnValue;
    while(remaining >= 8)
    {
        returnValue = BitFileGetChar(stream);
        if(returnValue == EOF)
            return EOF;

        bytes[offset] = (unsigned char)returnValue;
        remaining -= 8;
        offset--;
    }

    if(remaining != 0)
    {
        while(remaining > 0)
        {
            returnValue = BitFileGetBit(stream);
            if(returnValue == EOF)
                return EOF;

            bytes[offset] <<= 1;
            bytes[offset] |= (returnValue & 0x01);
            remaining--;
        }
    }
    return count;
}

int BitFilePutBitsNum(bit_file_t* stream, void* bits, const unsigned int count, const size_t size)
{
    if((stream == nullptr) || (bits == nullptr))
        return EOF;

    if(nullptr == stream->PutBitsNumFunc)
        return ENOTSUP;

    return (stream->PutBitsNumFunc)(stream, bits, count, size);
}

static int BitFilePutBitsLE(bit_file_t* stream, void* bits, const unsigned int count, const size_t size)
{
    unsigned char* bytes = (unsigned char*)bits;
    int offset = 0;
    int remaining = count;
    int returnValue;
    while(remaining >= 8)
    {
        returnValue = BitFilePutChar(bytes[offset], stream);
        if(returnValue == EOF)
            return EOF;

        remaining -= 8;
        offset++;
    }

    if(remaining != 0)
    {
        unsigned char tmp = bytes[offset];
        tmp <<= (8 - remaining);

        while(remaining > 0)
        {
            returnValue = BitFilePutBit((tmp & 0x80), stream);
            if(returnValue == EOF)
                return EOF;

            tmp <<= 1;
            remaining--;
        }
    }
    return count;
}

static int BitFilePutBitsBE(bit_file_t* stream, void* bits, const unsigned int count, const size_t size)
{
    if(count > (size * 8))
        return EOF;

    unsigned char* bytes = (unsigned char*)bits;
    int offset = size - 1;
    int remaining = count;
    int returnValue;
    while(remaining >= 8)
    {
        returnValue = BitFilePutChar(bytes[offset], stream);

        if(returnValue == EOF)
            return EOF;

        remaining -= 8;
        offset--;
    }

    if(remaining != 0)
    {
        unsigned char tmp = bytes[offset];
        tmp <<= (8 - remaining);

        while(remaining > 0)
        {
            returnValue = BitFilePutBit((tmp & 0x80), stream);
            if(returnValue == EOF)
                return EOF;

            tmp <<= 1;
            remaining--;
        }
    }
    return count;
}

static int BitFileNotSupported(bit_file_t* stream, void *bits, const unsigned int count, const size_t size)
{
    (void)stream;
    (void)bits;
    (void)count;
    (void)size;

    return -ENOTSUP;
}
