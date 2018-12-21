
#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "huflocal.h"
#include "huffman.h"
#include "bitarray.h"
#include "bitfile.h"

typedef struct code_list_t
{
    byte_t codeLen;
    bit_array_t* code;
} code_list_t;

static int MakeCodeList(huffman_node_t* ht, code_list_t* codeList);

static void WriteHeader(huffman_node_t* ht, bit_file_t* bfp);
static int ReadHeader(huffman_node_t** ht, bit_file_t* bfp);

int HuffmanEncodeFile(FILE* inFile, FILE* outFile)
{
    if((nullptr == inFile) || (nullptr == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    bit_file_t* bOutFile = MakeBitFile(outFile, BF_WRITE);

    if(nullptr == bOutFile)
    {
        perror("Making Output File a BitFile");
        return -1;
    }

    huffman_node_t* huffmanTree = GenerateTreeFromFile(inFile);
    if(huffmanTree == nullptr)
    {
        outFile = BitFileToFILE(bOutFile);
        return -1;
    }

    
    code_list_t codeList[NUM_CHARS];
    for(int i = 0; i < NUM_CHARS; i++)
    {
        codeList[i].code = nullptr;
        codeList[i].codeLen = 0;
    }

    if(0 != MakeCodeList(huffmanTree, codeList))
    {
        outFile = BitFileToFILE(bOutFile);
        return -1;
    }

    WriteHeader(huffmanTree, bOutFile);
    rewind(inFile);
    int c;
    while((c = fgetc(inFile)) != EOF)
        BitFilePutBits(bOutFile, BitArrayGetBits(codeList[c].code), codeList[c].codeLen);

    BitFilePutBits(bOutFile, BitArrayGetBits(codeList[EOF_CHAR].code), codeList[EOF_CHAR].codeLen);

    for(int i = 0; i < NUM_CHARS; i++)
        if(codeList[i].code != nullptr)
            BitArrayDestroy(codeList[i].code);

    outFile = BitFileToFILE(bOutFile);
    FreeHuffmanTree(huffmanTree);
    return 0;
}

int HuffmanDecodeFile(FILE* inFile, FILE* outFile)
{
    if((nullptr == inFile) || (nullptr == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    bit_file_t* bInFile = MakeBitFile(inFile, BF_READ);
    if(nullptr == bInFile)
    {
        perror("Making Input File a BitFile");
        return -1;
    }

    huffman_node_t *huffmanArray[NUM_CHARS];
    for(int i = 0; i < NUM_CHARS; i++)
    {
        if((huffmanArray[i] = AllocHuffmanNode(i)) == nullptr)
        {
            for(i--; i >= 0; i--)
                delete huffmanArray[i];

            inFile = BitFileToFILE(bInFile);
            return -1;
        }
    }

    if(0 != ReadHeader(huffmanArray, bInFile))
    {
        for(int i = 0; i < NUM_CHARS; i++)
            delete huffmanArray[i];

        inFile = BitFileToFILE(bInFile);
        return -1;
    }

    huffman_node_t *huffmanTree;
    if((huffmanTree = BuildHuffmanTree(huffmanArray, NUM_CHARS)) == nullptr)
    {
        FreeHuffmanTree(huffmanTree);
        inFile = BitFileToFILE(bInFile);
        return -1;
    }

    int c;
    huffman_node_t* currentNode = huffmanTree;
    while((c = BitFileGetBit(bInFile)) != EOF)
    {
        if(c != 0)
            currentNode = currentNode->right;
        else
            currentNode = currentNode->left;

        if(currentNode->value != COMPOSITE_NODE)
        {
            if(currentNode->value == EOF_CHAR)
                break;

            fputc(currentNode->value, outFile);
            currentNode = huffmanTree;
        }
    }
    inFile = BitFileToFILE(bInFile);
    FreeHuffmanTree(huffmanTree);
    return 0;
}

static int MakeCodeList(huffman_node_t* ht, code_list_t* codeList)
{
    bit_array_t* code;
    if((code = BitArrayCreate(EOF_CHAR)) == nullptr)
    {
        perror("Unable to allocate bit array");
        return -1;
    }

    BitArrayClearAll(code);
    byte_t depth = 0;
    while(true)
    {
        while(ht->left != nullptr)
        {
            BitArrayShiftLeft(code, 1);
            ht = ht->left;
            depth++;
        }

        if(ht->value != COMPOSITE_NODE)
        {
            codeList[ht->value].codeLen = depth;
            codeList[ht->value].code = BitArrayDuplicate(code);
            if(codeList[ht->value].code == nullptr)
            {
                perror("Unable to allocate bit array");
                BitArrayDestroy(code);
                return -1;
            }
            BitArrayShiftLeft(codeList[ht->value].code, EOF_CHAR - depth);
        }

        while(ht->parent != nullptr)
        {
            if(ht != ht->parent->right)
            {
                BitArraySetBit(code, (EOF_CHAR - 1));
                ht = ht->parent->right;
                break;
            }
            else
            {
                depth--;
                BitArrayShiftRight(code, 1);
                ht = ht->parent;
            }
        }
        if(ht->parent == nullptr)
            break;
    }
    BitArrayDestroy(code);
    return 0;
}

static void WriteHeader(huffman_node_t* ht, bit_file_t* bfp)
{
    while(true)
    {
        while(ht->left != nullptr)
            ht = ht->left;

        if((ht->value != COMPOSITE_NODE) && (ht->value != EOF_CHAR))
        {
            BitFilePutChar(ht->value, bfp);
            BitFilePutBits(bfp, (void *)&(ht->count), 8 * sizeof(count_t));
        }

        while(ht->parent != nullptr)
        {
            if(ht != ht->parent->right)
            {
                ht = ht->parent->right;
                break;
            }
            else
            {
                ht = ht->parent;
            }
        }

        if(ht->parent == nullptr)
            break;
    }

    BitFilePutChar(0, bfp);
    for(int i = 0; i < sizeof(count_t); i++)
        BitFilePutChar(0, bfp);
}

static int ReadHeader(huffman_node_t **ht, bit_file_t *bfp)
{
    count_t count;
    int c;
    int status = -1;
    while((c = BitFileGetChar(bfp)) != EOF)
    {
        BitFileGetBits(bfp, (void *)(&count), 8 * sizeof(count_t));

        if((count == 0) && (c == 0))
        {
            status = 0;
            break;
        }

        ht[c]->count = count;
        ht[c]->ignore = 0;
    }

    ht[EOF_CHAR]->count = 1;
    ht[EOF_CHAR]->ignore = 0;

    if(0 != status)
    {
        fprintf(stderr, "error: malformed file header.\n");
        errno = EILSEQ;
    }
    return status;
}
