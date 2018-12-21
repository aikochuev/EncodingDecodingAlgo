#ifndef _HUFFMAN_LOCAL_H
#define _HUFFMAN_LOCAL_H

#include <limits.h>

#if (UCHAR_MAX != 0xFF)
#error This program expects unsigned char to be 1 byte
#endif

#if (UINT_MAX != 0xFFFFFFFF)
#error This program expects unsigned int to be 4 bytes
#endif

typedef unsigned char byte_t;
typedef unsigned int count_t;

typedef struct huffman_node_t
{
    int value;
    count_t count;

    char ignore;
    int level;

    struct huffman_node_t *left, *right, *parent;
} huffman_node_t;

#define NONE    -1

#define COUNT_T_MAX     UINT_MAX

#define COMPOSITE_NODE      -1
#define NUM_CHARS   (UCHAR_MAX + 2)
#define EOF_CHAR    (NUM_CHARS - 1)

#define max(a, b) ((a)>(b)?(a):(b))

huffman_node_t* GenerateTreeFromFile(FILE* inFile);
huffman_node_t* BuildHuffmanTree(huffman_node_t** ht, int elements);
huffman_node_t* AllocHuffmanNode(int value);
void FreeHuffmanTree(huffman_node_t* ht);

#endif
