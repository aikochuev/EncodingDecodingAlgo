#include "pch.h"
#include <stdlib.h>
#include <errno.h>
#include "bitfile.h"

struct bit_file_t
{
    FILE *fp;                   /* file pointer used by stdio functions */
    unsigned char bitBuffer;    /* bits waiting to be read/written */
    unsigned char bitCount;     /* number of bits in bitBuffer */
    BF_MODES mode;              /* open for read, write, or append */
};

bit_file_t *MakeBitFile(FILE *stream, const BF_MODES mode)
{
    bit_file_t *bf;

    if(stream == nullptr)
    {
        /* can't wrapper empty steam */
        errno = EBADF;
        bf = nullptr;
    }
    else
    {
        bf = (bit_file_t *)malloc(sizeof(bit_file_t));

        if(bf == nullptr)
        {
            /* malloc failed */
            errno = ENOMEM;
        }
        else
        {
            /* set structure data */
            bf->fp = stream;
            bf->bitBuffer = 0;
            bf->bitCount = 0;
            bf->mode = mode;
        }
    }

    return (bf);
}
FILE* BitFileToFILE(bit_file_t* stream)
{
    FILE* fp = nullptr;

    if(stream == nullptr)
    {
        return(nullptr);
    }

    if((stream->mode == BF_WRITE) || (stream->mode == BF_APPEND))
    {
        /* write out any unwritten bits */
        if(stream->bitCount != 0)
        {
            (stream->bitBuffer) <<= 8 - (stream->bitCount);
            fputc(stream->bitBuffer, stream->fp);   /* handle error? */
        }
    }

    /* close file */
    fp = stream->fp;

    /* free memory allocated for bit file */
    delete stream;

    return(fp);
}

int BitFileGetChar(bit_file_t* stream)
{
    int returnValue;
    unsigned char tmp;

    if(stream == nullptr)
    {
        return(EOF);
    }

    returnValue = fgetc(stream->fp);

    if(stream->bitCount == 0)
    {
        /* we can just get byte from file */
        return returnValue;
    }

    /* we have some buffered bits to return too */
    if(returnValue != EOF)
    {
        /* figure out what to return */
        tmp = ((unsigned char)returnValue) >> (stream->bitCount);
        tmp |= ((stream->bitBuffer) << (8 - (stream->bitCount)));

        /* put remaining in buffer. count shouldn't change. */
        stream->bitBuffer = returnValue;

        returnValue = tmp;
    }

    return returnValue;
}

int BitFilePutChar(const int c, bit_file_t* stream)
{
    unsigned char tmp;

    if(stream == nullptr)
    {
        return(EOF);
    }

    if(stream->bitCount == 0)
    {
        /* we can just put byte from file */
        return fputc(c, stream->fp);
    }

    /* figure out what to write */
    tmp = ((unsigned char)c) >> (stream->bitCount);
    tmp = tmp | ((stream->bitBuffer) << (8 - stream->bitCount));

    if(fputc(tmp, stream->fp) != EOF)
    {
        /* put remaining in buffer. count shouldn't change. */
        stream->bitBuffer = c;
    }
    else
    {
        return EOF;
    }

    return tmp;
}

int BitFileGetBit(bit_file_t* stream)
{
    int returnValue;

    if(stream == nullptr)
    {
        return(EOF);
    }

    if(stream->bitCount == 0)
    {
        /* buffer is empty, read another character */
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

    /* bit to return is msb in buffer */
    stream->bitCount--;
    returnValue = (stream->bitBuffer) >> (stream->bitCount);

    return (returnValue & 0x01);
}

int BitFilePutBit(const int c, bit_file_t* stream)
{
    int returnValue = c;

    if(stream == nullptr)
    {
        return(EOF);
    }

    stream->bitCount++;
    stream->bitBuffer <<= 1;

    if(c != 0)
    {
        stream->bitBuffer |= 1;
    }

    /* write bit buffer if we have 8 bits */
    if(stream->bitCount == 8)
    {
        if(fputc(stream->bitBuffer, stream->fp) == EOF)
        {
            returnValue = EOF;
        }

        /* reset buffer */
        stream->bitCount = 0;
        stream->bitBuffer = 0;
    }

    return returnValue;
}

int BitFileGetBits(bit_file_t* stream, void* bits, const unsigned int count)
{
    unsigned char *bytes, shifts;
    int offset, remaining, returnValue;

    bytes = (unsigned char *)bits;

    if((stream == nullptr) || (bits == nullptr))
    {
        return(EOF);
    }

    offset = 0;
    remaining = count;

    /* read whole bytes */
    while(remaining >= 8)
    {
        returnValue = BitFileGetChar(stream);

        if(returnValue == EOF)
        {
            return EOF;
        }

        bytes[offset] = (unsigned char)returnValue;
        remaining -= 8;
        offset++;
    }

    if(remaining != 0)
    {
        /* read remaining bits */
        shifts = 8 - remaining;
        bytes[offset] = 0;

        while(remaining > 0)
        {
            returnValue = BitFileGetBit(stream);

            if(returnValue == EOF)
            {
                return EOF;
            }

            bytes[offset] <<= 1;
            bytes[offset] |= (returnValue & 0x01);
            remaining--;
        }

        /* shift last bits into position */
        bytes[offset] <<= shifts;
    }
    return count;
}

int BitFilePutBits(bit_file_t* stream, void* bits, const unsigned int count)
{
    unsigned char* bytes, tmp;
    int offset, remaining, returnValue;

    bytes = (unsigned char*)bits;

    if((stream == nullptr) || (bits == nullptr))
    {
        return(EOF);
    }

    offset = 0;
    remaining = count;

    /* write whole bytes */
    while(remaining >= 8)
    {
        returnValue = BitFilePutChar(bytes[offset], stream);

        if(returnValue == EOF)
        {
            return EOF;
        }

        remaining -= 8;
        offset++;
    }

    if(remaining != 0)
    {
        /* write remaining bits */
        tmp = bytes[offset];

        while(remaining > 0)
        {
            returnValue = BitFilePutBit((tmp & 0x80), stream);

            if(returnValue == EOF)
            {
                return EOF;
            }

            tmp <<= 1;
            remaining--;
        }
    }
    return count;
}