
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
    byte_t codeLen;     /* number of bits used in code (1 - 255) */
    bit_array_t *code;  /* code used for symbol (left justified) */
} code_list_t;

static int MakeCodeList(huffman_node_t *ht, code_list_t *codeList);

static void WriteHeader(huffman_node_t *ht, bit_file_t *bfp);
static int ReadHeader(huffman_node_t **ht, bit_file_t *bfp);

int HuffmanEncodeFile(FILE *inFile, FILE *outFile)
{
    huffman_node_t *huffmanTree;        // root of Huffman tree
    code_list_t codeList[NUM_CHARS];    // table for quick encode
    bit_file_t *bOutFile;
    int c;

    /* validate input and output files */
    if((nullptr == inFile) || (nullptr == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    bOutFile = MakeBitFile(outFile, BF_WRITE);

    if(nullptr == bOutFile)
    {
        perror("Making Output File a BitFile");
        return -1;
    }

    /* build tree */
    if((huffmanTree = GenerateTreeFromFile(inFile)) == nullptr)
    {
        outFile = BitFileToFILE(bOutFile);
        return -1;
    }

    /* build a list of codes for each symbol */

    /* initialize code list */
    for(c = 0; c < NUM_CHARS; c++)
    {
        codeList[c].code = nullptr;
        codeList[c].codeLen = 0;
    }

    if(0 != MakeCodeList(huffmanTree, codeList))
    {
        outFile = BitFileToFILE(bOutFile);
        return -1;
    }

    /* write out encoded file */

    /* write header for rebuilding of tree */
    WriteHeader(huffmanTree, bOutFile);

    /* read characters from file and write them to encoded file */
    rewind(inFile);         /* start another pass on the input file */

    while((c = fgetc(inFile)) != EOF)
    {
        BitFilePutBits(bOutFile,
            BitArrayGetBits(codeList[c].code),
            codeList[c].codeLen);
    }

    /* now write EOF */
    BitFilePutBits(bOutFile,
        BitArrayGetBits(codeList[EOF_CHAR].code),
        codeList[EOF_CHAR].codeLen);

    /* free the code list */
    for(c = 0; c < NUM_CHARS; c++)
    {
        if(codeList[c].code != nullptr)
        {
            BitArrayDestroy(codeList[c].code);
        }
    }

    /* clean up */
    outFile = BitFileToFILE(bOutFile);          /* make file normal again */
    FreeHuffmanTree(huffmanTree);               /* free allocated memory */

    return 0;
}

int HuffmanDecodeFile(FILE *inFile, FILE *outFile)
{
    huffman_node_t *huffmanArray[NUM_CHARS];    /* array of all leaves */
    huffman_node_t *huffmanTree;
    huffman_node_t *currentNode;
    int i, c;
    bit_file_t *bInFile;

    /* validate input and output files */
    if((nullptr == inFile) || (nullptr == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    bInFile = MakeBitFile(inFile, BF_READ);

    if(nullptr == bInFile)
    {
        perror("Making Input File a BitFile");
        return -1;
    }

    /* allocate array of leaves for all possible characters */
    for(i = 0; i < NUM_CHARS; i++)
    {
        if((huffmanArray[i] = AllocHuffmanNode(i)) == nullptr)
        {
            /* allocation failed clear existing allocations */
            for(i--; i >= 0; i--)
            {
                delete huffmanArray[i];
            }

            inFile = BitFileToFILE(bInFile);
            return -1;
        }
    }

    /* populate leaves with frequency information from file header */
    if(0 != ReadHeader(huffmanArray, bInFile))
    {
        for(i = 0; i < NUM_CHARS; i++)
        {
            delete huffmanArray[i];
        }

        inFile = BitFileToFILE(bInFile);
        return -1;
    }

    /* put array of leaves into a huffman tree */
    if((huffmanTree = BuildHuffmanTree(huffmanArray, NUM_CHARS)) == nullptr)
    {
        FreeHuffmanTree(huffmanTree);
        inFile = BitFileToFILE(bInFile);
        return -1;
    }

    /* now we should have a tree that matches the tree used on the encode */
    currentNode = huffmanTree;

    while((c = BitFileGetBit(bInFile)) != EOF)
    {
        /* traverse the tree finding matches for our characters */
        if(c != 0)
        {
            currentNode = currentNode->right;
        }
        else
        {
            currentNode = currentNode->left;
        }

        if(currentNode->value != COMPOSITE_NODE)
        {
            /* we've found a character */
            if(currentNode->value == EOF_CHAR)
            {
                /* we've just read the EOF */
                break;
            }

            fputc(currentNode->value, outFile); /* write out character */
            currentNode = huffmanTree;          /* back to top of tree */
        }
    }

    /* clean up */
    inFile = BitFileToFILE(bInFile);            /* make file normal again */
    FreeHuffmanTree(huffmanTree);     /* free allocated memory */

    return 0;
}

static int MakeCodeList(huffman_node_t *ht, code_list_t *codeList)
{
    bit_array_t *code;
    byte_t depth = 0;

    if((code = BitArrayCreate(EOF_CHAR)) == nullptr)
    {
        perror("Unable to allocate bit array");
        return -1;
    }

    BitArrayClearAll(code);

    for(;;)
    {
        /* follow this branch all the way left */
        while(ht->left != nullptr)
        {
            BitArrayShiftLeft(code, 1);
            ht = ht->left;
            depth++;
        }

        if(ht->value != COMPOSITE_NODE)
        {
            /* enter results in list */
            codeList[ht->value].codeLen = depth;
            codeList[ht->value].code = BitArrayDuplicate(code);
            if(codeList[ht->value].code == nullptr)
            {
                perror("Unable to allocate bit array");
                BitArrayDestroy(code);
                return -1;
            }

            /* now left justify code */
            BitArrayShiftLeft(codeList[ht->value].code, EOF_CHAR - depth);
        }

        while(ht->parent != nullptr)
        {
            if(ht != ht->parent->right)
            {
                /* try the parent's right */
                BitArraySetBit(code, (EOF_CHAR - 1));
                ht = ht->parent->right;
                break;
            }
            else
            {
                /* parent's right tried, go up one level yet */
                depth--;
                BitArrayShiftRight(code, 1);
                ht = ht->parent;
            }
        }

        if(ht->parent == nullptr)
        {
            /* we're at the top with nowhere to go */
            break;
        }
    }

    BitArrayDestroy(code);
    return 0;
}

static void WriteHeader(huffman_node_t *ht, bit_file_t *bfp)
{
    unsigned int i;

    for(;;)
    {
        /* follow this branch all the way left */
        while(ht->left != nullptr)
        {
            ht = ht->left;
        }

        if((ht->value != COMPOSITE_NODE) &&
            (ht->value != EOF_CHAR))
        {
            /* write symbol and count to header */
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
                /* parent's right tried, go up one level yet */
                ht = ht->parent;
            }
        }

        if(ht->parent == nullptr)
        {
            /* we're at the top with nowhere to go */
            break;
        }
    }

    /* now write end of table char 0 count 0 */
    BitFilePutChar(0, bfp);
    for(i = 0; i < sizeof(count_t); i++)
    {
        BitFilePutChar(0, bfp);
    }
}

static int ReadHeader(huffman_node_t **ht, bit_file_t *bfp)
{
    count_t count;
    int c;
    int status = -1;        /* in case of premature EOF */

    while((c = BitFileGetChar(bfp)) != EOF)
    {
        BitFileGetBits(bfp, (void *)(&count), 8 * sizeof(count_t));

        if((count == 0) && (c == 0))
        {
            /* we just read end of table marker */
            status = 0;
            break;
        }

        ht[c]->count = count;
        ht[c]->ignore = 0;
    }

    /* add assumed EOF */
    ht[EOF_CHAR]->count = 1;
    ht[EOF_CHAR]->ignore = 0;

    if(0 != status)
    {
        /* we hit EOF before we read a full header */
        fprintf(stderr, "error: malformed file header.\n");
        errno = EILSEQ;     /* Illegal byte sequence seems reasonable */
    }

    return status;
}
