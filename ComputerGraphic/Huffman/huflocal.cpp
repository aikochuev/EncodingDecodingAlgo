#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include "huflocal.h"
#include "huffman.h"

huffman_node_t* GenerateTreeFromFile(FILE* inFile)
{
    huffman_node_t* huffmanArray[NUM_CHARS];
    for(int i = 0; i < NUM_CHARS; i++)
    {
        if((huffmanArray[i] = AllocHuffmanNode(i)) == nullptr)
        {
            for(i--; i >= 0; i--)
                delete huffmanArray[i];

            return nullptr;
        }
    }

    huffmanArray[EOF_CHAR]->count = 1;
    huffmanArray[EOF_CHAR]->ignore = 0;
    int c;
    while((c = fgetc(inFile)) != EOF)
    {
        if(huffmanArray[c]->count < COUNT_T_MAX)
        {
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
    return BuildHuffmanTree(huffmanArray, NUM_CHARS);
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
    ht->value = COMPOSITE_NODE;
    ht->ignore = 0;
    ht->count = left->count + right->count;
    ht->level = max(left->level, right->level) + 1;
    ht->left = left;
    ht->left->parent = ht;
    ht->right = right;
    ht->right->parent = ht;
    ht->parent = nullptr;

    return ht;
}

void FreeHuffmanTree(huffman_node_t* ht)
{
    if(ht->left != nullptr)
        FreeHuffmanTree(ht->left);

    if(ht->right != nullptr)
        FreeHuffmanTree(ht->right);

    delete ht;
}

static int FindMinimumCount(huffman_node_t** ht, int elements)
{
    int currentIndex = NONE;
    unsigned int currentCount = UINT_MAX;
    int currentLevel = INT_MAX;
    for(int i = 0; i < elements; i++)
    {
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

huffman_node_t* BuildHuffmanTree(huffman_node_t** ht, int elements)
{
    int min1;
    int min2;
    while(true)
    {
        min1 = FindMinimumCount(ht, elements);
        if(min1 == NONE)
            break;
        ht[min1]->ignore = 1;

        min2 = FindMinimumCount(ht, elements);
        if(min2 == NONE)
            break;
        ht[min2]->ignore = 1;

        if((ht[min1] = AllocHuffmanCompositeNode(ht[min1], ht[min2])) == nullptr)
            return nullptr;

        ht[min2] = nullptr;
    }
    return ht[min1];
}
