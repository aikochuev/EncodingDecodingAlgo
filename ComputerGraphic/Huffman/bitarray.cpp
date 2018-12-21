#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "bitarray.h"

#ifndef CHAR_BIT
#warning CHAR_BIT not defined.Assuming 8 bits.
#define CHAR_BIT 8
#endif

#define BIT_CHAR(bit)         ((bit) / CHAR_BIT)

#define BIT_IN_CHAR(bit)      (1 << (CHAR_BIT - 1 - ((bit)  % CHAR_BIT)))

#define BITS_TO_CHARS(bits)   ((((bits) - 1) / CHAR_BIT) + 1)

#define MS_BIT                (1 << (CHAR_BIT - 1))

struct bit_array_t
{
    unsigned char* array;
    unsigned int numBits;
};

bit_array_t* BitArrayCreate(unsigned int bits)
{

    if(0 == bits)
    {
        errno = EINVAL;
        return nullptr;
    }
    bit_array_t* ba = new bit_array_t();
    ba->numBits = bits;
    ba->array = new unsigned char[BITS_TO_CHARS(bits)];
    return ba;
}

void BitArrayDestroy(bit_array_t* ba)
{
    if(ba != nullptr)
    {
        if(ba->array != nullptr)
            delete ba->array;
        delete ba;
    }
}

void BitArrayClearAll(bit_array_t* ba)
{
    if(ba == nullptr)
        return;

    memset((void*)(ba->array), 0, BITS_TO_CHARS(ba->numBits));
}

void BitArraySetBit(bit_array_t* ba, unsigned int bit)
{
    if(ba == nullptr || ba->numBits <= bit)
        return;

    ba->array[BIT_CHAR(bit)] |= BIT_IN_CHAR(bit);
}

void* BitArrayGetBits(bit_array_t* ba)
{
    return ((void*)(ba->array));
}

void BitArrayCopy(bit_array_t* dest, const bit_array_t* src)
{
    if(src == nullptr || dest == nullptr || src->numBits != dest->numBits)
        return;

    memcpy((void *)(dest->array), (void *)(src->array), BITS_TO_CHARS(src->numBits));
}

bit_array_t* BitArrayDuplicate(const bit_array_t* src)
{
    if(src == nullptr)
        return nullptr;

    bit_array_t* ba = BitArrayCreate(src->numBits);
    if(ba != nullptr)
    {
        ba->numBits = src->numBits;
        BitArrayCopy(ba, src);
    }
    return ba;
}

void BitArrayShiftLeft(bit_array_t* ba, unsigned int shifts)
{
    int chars = shifts / CHAR_BIT;
    shifts = shifts % CHAR_BIT;

    if(ba == nullptr)
        return;

    if(shifts >= ba->numBits)
    {
        BitArrayClearAll(ba);
        return;
    }

    if(chars > 0)
    {
        for(int i = 0; (i + chars) < BITS_TO_CHARS(ba->numBits); i++)
            ba->array[i] = ba->array[i + chars];

        for(int i = BITS_TO_CHARS(ba->numBits); chars > 0; chars--)
            ba->array[i - chars] = 0;
    }

    for(int i = 0; i < shifts; i++)
    {
        for(int j = 0; j < BIT_CHAR(ba->numBits - 1); j++)
        {
            ba->array[j] <<= 1;

            if(ba->array[j + 1] & MS_BIT)
                ba->array[j] |= 0x01;
        }
        ba->array[BIT_CHAR(ba->numBits - 1)] <<= 1;
    }
}

void BitArrayShiftRight(bit_array_t* ba, unsigned int shifts)
{
    if(ba == nullptr)
        return;

    unsigned chars = shifts / CHAR_BIT;
    shifts = shifts % CHAR_BIT;

    if(shifts >= ba->numBits)
    {
        BitArrayClearAll(ba);
        return;
    }

    if(chars > 0)
    {
        for(int i = BIT_CHAR(ba->numBits - 1); i >= chars; i--)
            ba->array[i] = ba->array[i - chars];

        for(; chars > 0; chars--)
            ba->array[chars - 1] = 0;
    }

    for(int i = 0; i < shifts; i++)
    {
        for(int j = BIT_CHAR(ba->numBits - 1); j > 0; j--)
        {
            ba->array[j] >>= 1;
            if(ba->array[j - 1] & 0x01)
                ba->array[j] |= MS_BIT;
        }
        ba->array[0] >>= 1;
    }

    int i = ba->numBits % CHAR_BIT;
    if(i != 0)
    {
        unsigned char mask = UCHAR_MAX << (CHAR_BIT - i);
        ba->array[BIT_CHAR(ba->numBits - 1)] &= mask;
    }
}
