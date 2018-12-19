#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include "huflocal.h"
#include "huffman.h"

huffman_node_t *GenerateTreeFromFile(FILE *inFile)
{
    huffman_node_t *huffmanArray[NUM_CHARS];    /* array of all leaves */
    huffman_node_t *huffmanTree;                /* root of huffman tree */
    int c;

    /* allocate array of leaves for all possible characters */
    for(c = 0; c < NUM_CHARS; c++)
    {
        if((huffmanArray[c] = AllocHuffmanNode(c)) == nullptr)
        {
            /* allocation failed clear existing allocations */
            for(c--; c >= 0; c--)
            {
                delete huffmanArray[c];
            }
            return nullptr;
        }
    }

    /* assume that there will be exactly 1 EOF */
    huffmanArray[EOF_CHAR]->count = 1;
    huffmanArray[EOF_CHAR]->ignore = 0;

    /* count occurrence of each character */
    while((c = fgetc(inFile)) != EOF)
    {
        if(huffmanArray[c]->count < COUNT_T_MAX)
        {
            /* increment count for character and include in tree */
            huffmanArray[c]->count++;
            huffmanArray[c]->ignore = 0;
        }
        else
        {
            fprintf(stderr,
                "Input file contains too many 0x%02X to count.\n", c);
            return nullptr;
        }
    }

    /* put array of leaves into a huffman tree */
    huffmanTree = BuildHuffmanTree(huffmanArray, NUM_CHARS);

    return huffmanTree;
}

huffman_node_t* AllocHuffmanNode(int value)
{
    huffman_node_t* ht = new huffman_node_t();
    ht->value = value;
    ht->ignore = 1;
    ht->count = 0;
    ht->level = 0;
    ht->left = nullptr;
    ht->right = nullptr;
    ht->parent = nullptr;

    return ht;
}

static huffman_node_t* AllocHuffmanCompositeNode(huffman_node_t *left, huffman_node_t *right)
{
    huffman_node_t* ht = new huffman_node_t();
    ht->value = COMPOSITE_NODE;     /* represents multiple chars */
    ht->ignore = 0;
    ht->count = left->count + right->count;     /* sum of children */
    ht->level = max(left->level, right->level) + 1;

    /* attach children */
    ht->left = left;
    ht->left->parent = ht;
    ht->right = right;
    ht->right->parent = ht;
    ht->parent = nullptr;

    return ht;
}

void FreeHuffmanTree(huffman_node_t *ht)
{
    if(ht->left != nullptr)
    {
        FreeHuffmanTree(ht->left);
    }

    if(ht->right != nullptr)
    {
        FreeHuffmanTree(ht->right);
    }

    delete ht;
}

static int FindMinimumCount(huffman_node_t **ht, int elements)
{
    int i;                      /* array index */
    int currentIndex;           /* index with lowest count seen so far */
    unsigned int currentCount;  /* lowest count seen so far */
    int currentLevel;           /* level of lowest count seen so far */

    currentIndex = NONE;
    currentCount = UINT_MAX;
    currentLevel = INT_MAX;

    /* sequentially search array */
    for(i = 0; i < elements; i++)
    {
        /* check for lowest count (or equally as low, but not as deep) */
        if((ht[i] != nullptr) && (!ht[i]->ignore) &&
            (ht[i]->count < currentCount ||
            (ht[i]->count == currentCount && ht[i]->level < currentLevel)))
        {
            currentIndex = i;
            currentCount = ht[i]->count;
            currentLevel = ht[i]->level;
        }
    }

    return currentIndex;
}

huffman_node_t *BuildHuffmanTree(huffman_node_t **ht, int elements)
{
    int min1, min2;     /* two nodes with the lowest count */

    /* keep looking until no more nodes can be found */
    for(;;)
    {
        /* find node with lowest count */
        min1 = FindMinimumCount(ht, elements);

        if(min1 == NONE)
        {
            /* no more nodes to combine */
            break;
        }

        ht[min1]->ignore = 1;       /* remove from consideration */

        /* find node with second lowest count */
        min2 = FindMinimumCount(ht, elements);

        if(min2 == NONE)
        {
            /* no more nodes to combine */
            break;
        }

        ht[min2]->ignore = 1;       /* remove from consideration */

        /* combine nodes into a tree */
        if((ht[min1] = AllocHuffmanCompositeNode(ht[min1], ht[min2])) == nullptr)
        {
            return nullptr;
        }

        ht[min2] = nullptr;
    }

    return ht[min1];
}
