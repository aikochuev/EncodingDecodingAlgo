#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include "arcode.h"
#include "bitfile.h"

#if !(USHRT_MAX < ULONG_MAX)
#error "Implementation requires USHRT_MAX < ULONG_MAX"
#endif

#define EOF_CHAR    (UCHAR_MAX + 1)

typedef unsigned short probability_t;

typedef struct
{
    probability_t ranges[EOF_CHAR + 2];
    probability_t cumulativeProb;

    probability_t lower;
    probability_t upper;

    probability_t code;
    unsigned char underflowBits;
} stats_t;

#define PRECISION           (8 * sizeof(probability_t))

#define MAX_PROBABILITY     (1 << (PRECISION - 2))

#define MASK_BIT(x) (probability_t)(1 << (PRECISION - (1 + (x))))

#define LOWER(c)    (c)
#define UPPER(c)    ((c) + 1)

static void WriteHeader(bit_file_t* bfpOut, stats_t* stats);
static int ReadHeader(bit_file_t* bfpIn, stats_t* stats);

static void ApplySymbolRange(int symbol, stats_t * stats);

static void WriteEncodedBits(bit_file_t* bfpOut, stats_t* stats);
static void WriteRemaining(bit_file_t* bfpOut, stats_t* stats);
static int BuildProbabilityRangeList(FILE* fpIn, stats_t* stats);
static void InitializeAdaptiveProbabilityRangeList(stats_t* stats);

static void InitializeDecoder(bit_file_t* bfpOut, stats_t* stats);
static probability_t GetUnscaledCode(stats_t* stats);
static int GetSymbolFromProbability(probability_t probability, stats_t* stats);
static void ReadEncodedBits(bit_file_t* bfpIn, stats_t* stats);

int ArEncodeFile(FILE* inFile, FILE* outFile)
{
    if(nullptr == inFile)
        inFile = stdin;

    bit_file_t* bOutFile;
    if(outFile == nullptr)
        bOutFile = MakeBitFile(stdout, BF_WRITE);
    else
        bOutFile = MakeBitFile(outFile, BF_WRITE);

    if(nullptr == bOutFile)
    {
        fprintf(stderr, "Error: Creating binary output file\n");
        return -1;
    }

    stats_t stats;
    InitializeAdaptiveProbabilityRangeList(&stats);

    stats.lower = 0;
    stats.upper = ~0;
    stats.underflowBits = 0;

    int c;
    while((c = fgetc(inFile)) != EOF)
    {
        ApplySymbolRange(c, &stats);
        WriteEncodedBits(bOutFile, &stats);
    }

    ApplySymbolRange(EOF_CHAR, &stats);
    WriteEncodedBits(bOutFile, &stats);
    WriteRemaining(bOutFile, &stats);
    outFile = BitFileToFILE(bOutFile);

    return 0;
}

static void SymbolCountToProbabilityRanges(stats_t *stats)
{
    stats->ranges[0] = 0;
    stats->ranges[UPPER(EOF_CHAR)] = 1;
    stats->cumulativeProb++;

    for(int c = 1; c <= UPPER(EOF_CHAR); c++)
        stats->ranges[c] += stats->ranges[c - 1];
}

static int BuildProbabilityRangeList(FILE *fpIn, stats_t *stats)
{
    if(fpIn == nullptr)
        return -1;

    unsigned long countArray[EOF_CHAR];
    for(int c = 0; c < EOF_CHAR; c++)
        countArray[c] = 0;

    int c;
    unsigned long totalCount = 0;
    while((c = fgetc(fpIn)) != EOF)
    {
        if(totalCount == ULONG_MAX)
        {
            fprintf(stderr, "Error: file too large\n");
            return -1;
        }

        countArray[c]++;
        totalCount++;
    }

    if(totalCount >= MAX_PROBABILITY)
    {
        unsigned long rescaleValue = (totalCount / MAX_PROBABILITY) + 1;
        for(c = 0; c < EOF_CHAR; c++)
        {
            if(countArray[c] > rescaleValue)
                countArray[c] /= rescaleValue;
            else if(countArray[c] != 0)
                countArray[c] = 1;
        }
    }

    stats->ranges[0] = 0;
    stats->cumulativeProb = 0;
    for(c = 0; c < EOF_CHAR; c++)
    {
        stats->ranges[UPPER(c)] = countArray[c];
        stats->cumulativeProb += countArray[c];
    }

    SymbolCountToProbabilityRanges(stats);
    return 0;
}

static void WriteHeader(bit_file_t* bfpOut, stats_t* stats)
{
    probability_t previous = 0;
    for(int c = 0; c <= (EOF_CHAR - 1); c++)
    {
        if(stats->ranges[UPPER(c)] > previous)
        {
            BitFilePutChar((char)c, bfpOut);
            previous = (stats->ranges[UPPER(c)] - previous);
            BitFilePutBitsNum(bfpOut, &previous, (PRECISION - 2), sizeof(probability_t));
            previous = stats->ranges[UPPER(c)];
        }
    }

    BitFilePutChar(0x00, bfpOut);
    previous = 0;
    BitFilePutBits(bfpOut, (void*)&previous, PRECISION - 2);
}

static void InitializeAdaptiveProbabilityRangeList(stats_t* stats)
{
    stats->ranges[0] = 0;
    for(int c = 1; c <= UPPER(EOF_CHAR); c++)
        stats->ranges[c] = stats->ranges[c - 1] + 1;

    stats->cumulativeProb = UPPER(EOF_CHAR);
}

static void ApplySymbolRange(int symbol, stats_t* stats)
{
    unsigned long range = (unsigned long)(stats->upper - stats->lower) + 1;
    unsigned long rescaled = (unsigned long)(stats->ranges[UPPER(symbol)]) * range;
    rescaled /= (unsigned long)(stats->cumulativeProb);

    stats->upper = stats->lower + (probability_t)rescaled - 1;

    rescaled = (unsigned long)(stats->ranges[LOWER(symbol)]) * range;
    rescaled /= (unsigned long)(stats->cumulativeProb);

    stats->lower = stats->lower + (probability_t)rescaled;
    stats->cumulativeProb++;

    for(int i = UPPER(symbol); i <= UPPER(EOF_CHAR); i++)
        stats->ranges[i] += 1;

    if(stats->cumulativeProb >= MAX_PROBABILITY)
    {
        stats->cumulativeProb = 0;
        probability_t original = 0;
        probability_t delta;
        for(int i = 1; i <= UPPER(EOF_CHAR); i++)
        {
            delta = stats->ranges[i] - original;
            original = stats->ranges[i];

            if(delta <= 2)
                stats->ranges[i] = stats->ranges[i - 1] + 1;
            else
                stats->ranges[i] = stats->ranges[i - 1] + (delta / 2);

            stats->cumulativeProb += (stats->ranges[i] - stats->ranges[i - 1]);
        }
    }
}

static void WriteEncodedBits(bit_file_t* bfpOut, stats_t* stats)
{
    while(true)
    {
        if((stats->upper & MASK_BIT(0)) == (stats->lower & MASK_BIT(0)))
        {
            BitFilePutBit((stats->upper & MASK_BIT(0)) != 0, bfpOut);

            while(stats->underflowBits > 0)
            {
                BitFilePutBit((stats->upper & MASK_BIT(0)) == 0, bfpOut);
                stats->underflowBits--;
            }
        }
        else if((stats->lower & MASK_BIT(1)) && !(stats->upper & MASK_BIT(1)))
        {
            stats->underflowBits += 1;
            stats->lower &= ~(MASK_BIT(0) | MASK_BIT(1));
            stats->upper |= MASK_BIT(1);
        }
        else
        {
            return;
        }
        stats->lower <<= 1;
        stats->upper <<= 1;
        stats->upper |= 1;
    }
}

static void WriteRemaining(bit_file_t* bfpOut, stats_t* stats)
{
    BitFilePutBit((stats->lower & MASK_BIT(1)) != 0, bfpOut);
    for(stats->underflowBits++; stats->underflowBits > 0; stats->underflowBits--)
        BitFilePutBit((stats->lower & MASK_BIT(1)) == 0, bfpOut);
}

int ArDecodeFile(FILE* inFile, FILE* outFile)
{
    if(nullptr == outFile)
        outFile = stdout;

    if(nullptr == inFile)
    {
        fprintf(stderr, "Error: Invalid input file\n");
        return -1;
    }

    bit_file_t* bInFile = MakeBitFile(inFile, BF_READ);

    if(nullptr == bInFile)
    {
        fprintf(stderr, "Error: Unable to create binary input file\n");
        return -1;
    }

    stats_t stats;
    InitializeAdaptiveProbabilityRangeList(&stats);
    InitializeDecoder(bInFile, &stats);

    int c;
    probability_t unscaled;
    while(true)
    {
        unscaled = GetUnscaledCode(&stats);
        if((c = GetSymbolFromProbability(unscaled, &stats)) == -1
            || c == EOF_CHAR)
            break;

        fputc((char)c, outFile);
        ApplySymbolRange(c, &stats);
        ReadEncodedBits(bInFile, &stats);
    }
    inFile = BitFileToFILE(bInFile);
    return 0;
}

static int ReadHeader(bit_file_t* bfpIn, stats_t* stats)
{
    stats->cumulativeProb = 0;
    int c;
    for(c = 0; c <= UPPER(EOF_CHAR); c++)
        stats->ranges[UPPER(c)] = 0;

    probability_t count;
    while(true)
    {
        c = BitFileGetChar(bfpIn);
        count = 0;
        if(count == 0)
            break;

        stats->ranges[UPPER(c)] = count;
        stats->cumulativeProb += count;
    }
    SymbolCountToProbabilityRanges(stats);
    return 0;
}

static void InitializeDecoder(bit_file_t* bfpIn, stats_t* stats)
{
    stats->code = 0;
    for(int i = 0; i < (int)PRECISION; i++)
    {
        stats->code <<= 1;
        if(BitFileGetBit(bfpIn) == 1)
            stats->code |= 1;
    }
    stats->lower = 0;
    stats->upper = ~0;
}

static probability_t GetUnscaledCode(stats_t* stats)
{
    unsigned long range = (unsigned long)(stats->upper - stats->lower) + 1;
    unsigned long unscaled = (unsigned long)(stats->code - stats->lower) + 1;
    unscaled = unscaled * (unsigned long)(stats->cumulativeProb) - 1;
    unscaled /= range;
    return ((probability_t)unscaled);
}

static int GetSymbolFromProbability(probability_t probability, stats_t* stats)
{
    int first = 0;
    int last = UPPER(EOF_CHAR);
    int middle = last / 2;

    while(last >= first)
    {
        if(probability < stats->ranges[LOWER(middle)])
        {
            last = middle - 1;
            middle = first + ((last - first) / 2);
            continue;
        }
        if(probability >= stats->ranges[UPPER(middle)])
        {
            first = middle + 1;
            middle = first + ((last - first) / 2);
            continue;
        }
        return middle;
    }
    fprintf(stderr, "Unknown Symbol: %d (max: %d)\n", probability, stats->ranges[UPPER(EOF_CHAR)]);
    return -1;
}

static void ReadEncodedBits(bit_file_t* bfpIn, stats_t* stats)
{
    int nextBit;
    while(true)
    {
        if((stats->upper & MASK_BIT(0)) == (stats->lower & MASK_BIT(0)))
        {

        }
        else if((stats->lower & MASK_BIT(1)) && !(stats->upper & MASK_BIT(1)))
        {
            stats->lower &= ~(MASK_BIT(0) | MASK_BIT(1));
            stats->upper |= MASK_BIT(1);
            stats->code ^= MASK_BIT(1);
        }
        else
        {
            return;
        }

        stats->lower <<= 1;
        stats->upper <<= 1;
        stats->upper |= 1;
        stats->code <<= 1;

        if((nextBit = BitFileGetBit(bfpIn)) != EOF)
            stats->code |= nextBit;
    }
}
