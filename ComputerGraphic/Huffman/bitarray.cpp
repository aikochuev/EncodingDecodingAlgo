#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "bitarray.h"

/* make CHAR_BIT 8 if it's not defined in limits.h */
#ifndef CHAR_BIT
#warning CHAR_BIT not defined.  Assuming 8 bits.
#define CHAR_BIT 8
#endif

/* position of bit within character */
#define BIT_CHAR(bit)         ((bit) / CHAR_BIT)

/* array index for character containing bit */
#define BIT_IN_CHAR(bit)      (1 << (CHAR_BIT - 1 - ((bit)  % CHAR_BIT)))

/* number of characters required to contain number of bits */
#define BITS_TO_CHARS(bits)   ((((bits) - 1) / CHAR_BIT) + 1)

/* most significant bit in a character */
#define MS_BIT                (1 << (CHAR_BIT - 1))

struct bit_array_t
{
    unsigned char *array;       /* pointer to array containing bits */
    unsigned int numBits;       /* number of bits in array */
};

bit_array_t* BitArrayCreate(unsigned int bits)
{

    if(0 == bits)
    {
        errno = EINVAL;
        return nullptr;
    }

    /* allocate structure */
    bit_array_t* ba = new bit_array_t();

    ba->numBits = bits;

    /* allocate array */
    ba->array = new unsigned char();
    return ba;
}

void BitArrayDestroy(bit_array_t *ba)
{
    if (ba != nullptr)
    {
        if (ba->array != nullptr)
        {
            delete ba->array;
        }

        delete ba;
    }
}

void BitArrayClearAll(bit_array_t *ba)
{
    if (ba == nullptr)
    {
        return;         /* nothing to clear */
    }

    /* set bits in all bytes to 0 */
    memset((void *)(ba->array), 0, BITS_TO_CHARS(ba->numBits));
}

void BitArraySetBit(bit_array_t* ba, unsigned int bit)
{
    if (ba == nullptr)
    {
        return;
    }

    if (ba->numBits <= bit)
    {
        return;
    }

    ba->array[BIT_CHAR(bit)] |= BIT_IN_CHAR(bit);
}

void *BitArrayGetBits(bit_array_t* ba)
{
    return ((void *)(ba->array));
}

void BitArrayCopy(bit_array_t *dest, const bit_array_t *src)
{
    if (src == nullptr)
    {
        return;
    }

    if (dest == nullptr)
    {
        return; 
    }

    if (src->numBits != dest->numBits)
    {
        return;
    }

    memcpy((void *)(dest->array), (void *)(src->array),
        BITS_TO_CHARS(src->numBits));
}

bit_array_t *BitArrayDuplicate(const bit_array_t *src)
{
    bit_array_t *ba;

    if (src == nullptr)
    {
        return nullptr;    /* no source array */
    }

    ba = BitArrayCreate(src->numBits);

    if (ba != nullptr)
    {
        ba->numBits = src->numBits;
        BitArrayCopy(ba, src);
    }

    return ba;
}

void BitArrayShiftLeft(bit_array_t *ba, unsigned int shifts)
{
    unsigned i, j;
    int chars;

    chars = shifts / CHAR_BIT;  /* number of whole byte shifts */
    shifts = shifts % CHAR_BIT;     /* number of bit shifts remaining */

    if (ba == nullptr)
    {
        return;         /* nothing to shift */
    }

    if (shifts >= ba->numBits)
    {
        /* all bits have been shifted off */
        BitArrayClearAll(ba);
        return;
    }

    /* first handle big jumps of bytes */
    if (chars > 0)
    {
        for (i = 0; (i + chars) <  BITS_TO_CHARS(ba->numBits); i++)
        {
            ba->array[i] = ba->array[i + chars];
        }

        /* now zero out new bytes on the right */
        for (i = BITS_TO_CHARS(ba->numBits); chars > 0; chars--)
        {
            ba->array[i - chars] = 0;
        }
    }

    /* now we have at most CHAR_BIT - 1 bit shifts across the whole array */
    for (i = 0; i < shifts; i++)
    {
        for (j = 0; j < BIT_CHAR(ba->numBits - 1); j++)
        {
            ba->array[j] <<= 1;

            /* handle shifts across byte bounds */
            if (ba->array[j + 1] & MS_BIT)
            {
                ba->array[j] |= 0x01;
            }
        }

        ba->array[BIT_CHAR(ba->numBits - 1)] <<= 1;
    }
}

void BitArrayShiftRight(bit_array_t *ba, unsigned int shifts)
{
    unsigned i, j;
    unsigned char mask;
    unsigned chars;

    chars = shifts / CHAR_BIT;      /* number of whole byte shifts */
    shifts = shifts % CHAR_BIT;     /* number of bit shifts remaining */

    if (ba == nullptr)
    {
        return;         /* nothing to shift */
    }

    if (shifts >= ba->numBits)
    {
        /* all bits have been shifted off */
        BitArrayClearAll(ba);
        return;
    }

    /* first handle big jumps of bytes */
    if (chars > 0)
    {
        for (i = BIT_CHAR(ba->numBits - 1); i >= chars; i--)
        {
            ba->array[i] = ba->array[i - chars];
        }

        /* now zero out new bytes on the right */
        for (; chars > 0; chars--)
        {
            ba->array[chars - 1] = 0;
        }
    }

    /* now we have at most CHAR_BIT - 1 bit shifts across the whole array */
    for (i = 0; i < shifts; i++)
    {
        for (j = BIT_CHAR(ba->numBits - 1); j > 0; j--)
        {
            ba->array[j] >>= 1;

            /* handle shifts across byte bounds */
            if (ba->array[j - 1] & 0x01)
            {
                ba->array[j] |= MS_BIT;
            }
        }

        ba->array[0] >>= 1;
    }

    /***********************************************************************
    * zero any spare bits that are beyond the end of the bit array so
    * increment and decrement are consistent.
    ***********************************************************************/
    i = ba->numBits % CHAR_BIT;
    if (i != 0)
    {
        mask = UCHAR_MAX << (CHAR_BIT - i);
        ba->array[BIT_CHAR(ba->numBits - 1)] &= mask;
    }
}

int BitArrayCompare(const bit_array_t *ba1, const bit_array_t *ba2)
{
    unsigned i;

    if (ba1 == nullptr)
    {
        if (ba2 == nullptr)
        {
            return 0;                   /* both are nullptr */
        }
        else
        {
            return -(int)(ba2->numBits);     /* ba2 is the only Non-nullptr*/
        }
    }

    if (ba2 == nullptr)
    {
        return (ba1->numBits);          /* ba1 is the only Non-nullptr*/
    }

    if (ba1->numBits != ba2->numBits)
    {
        /* arrays are different sizes */
        return(ba1->numBits - ba2->numBits);
    }

    for(i = 0; i <= BIT_CHAR(ba1->numBits - 1); i++)
    {
        if (ba1->array[i] != ba2->array[i])
        {
            /* found a non-matching unsigned char */
            return(ba1->array[i] - ba2->array[i]);
        }
    }

    return 0;
}
