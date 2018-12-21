#ifndef BIT_ARRAY_H
#define BIT_ARRAY_H

struct bit_array_t;
typedef struct bit_array_t bit_array_t;

bit_array_t* BitArrayCreate(unsigned int bits);
void BitArrayDestroy(bit_array_t* ba);

void BitArrayClearAll(bit_array_t* ba);
void BitArraySetBit(bit_array_t* ba, unsigned int bit);

void *BitArrayGetBits(bit_array_t* ba);

void BitArrayCopy(bit_array_t* dest, const bit_array_t* src);
bit_array_t* BitArrayDuplicate(const bit_array_t* src);

void BitArrayShiftLeft(bit_array_t* ba, unsigned int shifts);
void BitArrayShiftRight(bit_array_t* ba, unsigned int shifts);

#endif
