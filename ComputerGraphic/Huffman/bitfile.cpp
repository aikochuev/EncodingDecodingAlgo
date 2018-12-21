#include "pch.h"
#include <stdlib.h>
#include <errno.h>
#include "bitfile.h"

struct bit_file_t
{
    FILE *fp;
    unsigned char bitBuffer;
    unsigned char bitCount;
    BF_MODES mode;
};

bit_file_t* MakeBitFile(FILE* stream, const BF_MODES mode)
{
    if(stream == nullptr)
    {
        errno = EBADF;
        return nullptr;
    }
    else
    {
        bit_file_t* bf = new bit_file_t();
        bf->fp = stream;
        bf->bitBuffer = 0;
        bf->bitCount = 0;
        bf->mode = mode;
        return bf;
    }
    return nullptr;
}

FILE* BitFileToFILE(bit_file_t* stream)
{
    if(stream == nullptr)
        return(nullptr);

    if((stream->bitCount != 0)
        && ((stream->mode == BF_WRITE) || (stream->mode == BF_APPEND)))
    {
        (stream->bitBuffer) <<= 8 - (stream->bitCount);
        fputc(stream->bitBuffer, stream->fp);
    }
    FILE* fp = stream->fp;
    delete stream;
    return fp;
}

int BitFileGetChar(bit_file_t* stream)
{
    if(stream == nullptr)
        return(EOF);

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
        return(EOF);

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
        return(EOF);

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
        return(EOF);

    stream->bitCount++;
    stream->bitBuffer <<= 1;

    int returnValue = c;
    if(c != 0)
        stream->bitBuffer |= 1;

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
    unsigned char* bytes = (unsigned char*)bits;

    if((stream == nullptr) || (bits == nullptr))
        return EOF;

    int offset = 0;
    int remaining = count;
    int returnValue;
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