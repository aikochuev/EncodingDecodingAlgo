#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include "arcode.h"
#include "bitfile.h"

#ifdef NDEBUG
#define PrintDebug(ARGS) do {} while (0)
#else
#define PrintDebug(ARGS) printf ARGS
#endif

#if !(USHRT_MAX < ULONG_MAX)
#error "Implementation requires USHRT_MAX < ULONG_MAX"
#endif

#define EOF_CHAR    (UCHAR_MAX + 1)

typedef unsigned short probability_t;       /* probability count type */

typedef struct
{
    /* probability ranges for each symbol: [ranges[LOWER(c)], ranges[UPPER(c)]) */
    probability_t ranges[EOF_CHAR + 2];
    probability_t cumulativeProb;   /* sum for all ranges */

    /* lower and upper bounds of current code range */
    probability_t lower;
    probability_t upper;

    probability_t code;             /* current MSBs of encode input stream */
    unsigned char underflowBits;    /* current underflow bit count */
} stats_t;

/* number of bits used to compute running code values */
#define PRECISION           (8 * sizeof(probability_t))

/* 2 bits less than precision. keeps lower and upper bounds from crossing. */
#define MAX_PROBABILITY     (1 << (PRECISION - 2))

/* set bit x to 1 in probability_t.  Bit 0 is MSB */
#define MASK_BIT(x) (probability_t)(1 << (PRECISION - (1 + (x))))

/* indices for a symbol's lower and upper cumulative probability ranges */
#define LOWER(c)    (c)
#define UPPER(c)    ((c) + 1)

/* read write file headers */
static void WriteHeader(bit_file_t *bfpOut, stats_t *stats);
static int ReadHeader(bit_file_t *bfpIn, stats_t *stats);

/* applies symbol's ranges to current upper and lower range bounds */
static void ApplySymbolRange(int symbol, stats_t *stats, char model);

/* routines for encoding*/
static void WriteEncodedBits(bit_file_t *bfpOut, stats_t *stats);
static void WriteRemaining(bit_file_t *bfpOut, stats_t *stats);
static int BuildProbabilityRangeList(FILE *fpIn, stats_t *stats);
static void InitializeAdaptiveProbabilityRangeList(stats_t *stats);

/* routines for decoding */
static void InitializeDecoder(bit_file_t *bfpOut, stats_t *stats);
static probability_t GetUnscaledCode(stats_t *stats);
static int GetSymbolFromProbability(probability_t probability, stats_t *stats);
static void ReadEncodedBits(bit_file_t *bfpIn, stats_t *stats);

int ArEncodeFile(FILE* inFile, FILE* outFile, const model_t model)
{
    int c;
    bit_file_t *bOutFile;
    stats_t stats;

    if(nullptr == inFile)
        inFile = stdin;

    if(outFile == nullptr)
        bOutFile = MakeBitFile(stdout, BF_WRITE);
    else
        bOutFile = MakeBitFile(outFile, BF_WRITE);

    if(nullptr == bOutFile)
    {
        fprintf(stderr, "Error: Creating binary output file\n");
        return -1;
    }

    if(MODEL_STATIC == model)
    {
        if(0 != BuildProbabilityRangeList(inFile, &stats))
        {
            fclose(inFile);
            BitFileClose(bOutFile);
            fprintf(stderr, "Error determining frequency ranges.\n");
            return -1;
        }

        rewind(inFile);

        WriteHeader(bOutFile, &stats);
    }
    else
    {
        InitializeAdaptiveProbabilityRangeList(&stats);
    }

    stats.lower = 0;
    stats.upper = ~0;
    stats.underflowBits = 0;

    while((c = fgetc(inFile)) != EOF)
    {
        ApplySymbolRange(c, &stats, model);
        WriteEncodedBits(bOutFile, &stats);
    }

    ApplySymbolRange(EOF_CHAR, &stats, model);
    WriteEncodedBits(bOutFile, &stats);
    WriteRemaining(bOutFile, &stats);
    outFile = BitFileToFILE(bOutFile);

    return 0;
}

static void SymbolCountToProbabilityRanges(stats_t *stats)
{
    int c;
    stats->ranges[0] = 0;
    stats->ranges[UPPER(EOF_CHAR)] = 1;
    stats->cumulativeProb++;

    for(c = 1; c <= UPPER(EOF_CHAR); c++)
        stats->ranges[c] += stats->ranges[c - 1];

    PrintDebug(("Ranges:\n"));
    for(c = 0; c < UPPER(EOF_CHAR); c++)
        PrintDebug(("%02X\t%d\t%d\n", c, stats->ranges[LOWER(c)], stats->ranges[UPPER(c)]));
}

static int BuildProbabilityRangeList(FILE *fpIn, stats_t *stats)
{
    int c;
    unsigned long countArray[EOF_CHAR];
    unsigned long totalCount = 0;
    unsigned long rescaleValue;

    if(fpIn == nullptr)
        return -1;

    /* start with no symbols counted */
    for(c = 0; c < EOF_CHAR; c++)
        countArray[c] = 0;

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
        rescaleValue = (totalCount / MAX_PROBABILITY) + 1;
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

static void WriteHeader(bit_file_t *bfpOut, stats_t *stats)
{
    int c;
    probability_t previous = 0;

    PrintDebug(("HEADER:\n"));

    for(c = 0; c <= (EOF_CHAR - 1); c++)
    {
        if(stats->ranges[UPPER(c)] > previous)
        {
            BitFilePutChar((char)c, bfpOut);
            previous = (stats->ranges[UPPER(c)] - previous);
            PrintDebug(("%02X\t%d\n", c, previous));

            BitFilePutBitsNum(bfpOut, &previous, (PRECISION - 2), sizeof(probability_t));

            previous = stats->ranges[UPPER(c)];
        }
    }

    BitFilePutChar(0x00, bfpOut);
    previous = 0;
    BitFilePutBits(bfpOut, (void *)&previous, PRECISION - 2);
}

static void InitializeAdaptiveProbabilityRangeList(stats_t *stats)
{
    int c;
    stats->ranges[0] = 0;

    for(c = 1; c <= UPPER(EOF_CHAR); c++)
        stats->ranges[c] = stats->ranges[c - 1] + 1;

    stats->cumulativeProb = UPPER(EOF_CHAR);

    PrintDebug(("Ranges:\n"));
    for(c = 0; c < UPPER(EOF_CHAR); c++)
    {
        PrintDebug(("%02X\t%d\t%d\n", c, stats->ranges[LOWER(c)],
            stats->ranges[UPPER(c)]));
    }
}

static void ApplySymbolRange(int symbol, stats_t *stats, char model)
{
    unsigned long range;
    unsigned long rescaled;

    int i;
    probability_t original;
    probability_t delta;

    range = (unsigned long)(stats->upper - stats->lower) + 1;

    rescaled = (unsigned long)(stats->ranges[UPPER(symbol)]) * range;
    rescaled /= (unsigned long)(stats->cumulativeProb);

    stats->upper = stats->lower + (probability_t)rescaled - 1;

    rescaled = (unsigned long)(stats->ranges[LOWER(symbol)]) * range;
    rescaled /= (unsigned long)(stats->cumulativeProb);

    stats->lower = stats->lower + (probability_t)rescaled;

    if(!model)
    {
        stats->cumulativeProb++;

        for(i = UPPER(symbol); i <= UPPER(EOF_CHAR); i++)
        {
            stats->ranges[i] += 1;
        }

        if(stats->cumulativeProb >= MAX_PROBABILITY)
        {
            stats->cumulativeProb = 0;
            original = 0;

            for(i = 1; i <= UPPER(EOF_CHAR); i++)
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
    assert(stats->lower <= stats->upper);
}

static void WriteEncodedBits(bit_file_t *bfpOut, stats_t *stats)
{
    for(;;)
    {
        if((stats->upper & MASK_BIT(0)) == (stats->lower & MASK_BIT(0)))
        {
            /* MSBs match, write them to output file */
            BitFilePutBit((stats->upper & MASK_BIT(0)) != 0, bfpOut);

            /* we can write out underflow bits too */
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

static void WriteRemaining(bit_file_t *bfpOut, stats_t *stats)
{
    BitFilePutBit((stats->lower & MASK_BIT(1)) != 0, bfpOut);
    for(stats->underflowBits++; stats->underflowBits > 0; stats->underflowBits--)
        BitFilePutBit((stats->lower & MASK_BIT(1)) == 0, bfpOut);
}

int ArDecodeFile(FILE *inFile, FILE *outFile, const model_t model)
{
    int c;
    probability_t unscaled;
    bit_file_t *bInFile;
    stats_t stats;

    if(nullptr == outFile)
        outFile = stdout;

    if(nullptr == inFile)
    {
        fprintf(stderr, "Error: Invalid input file\n");
        return -1;
    }

    bInFile = MakeBitFile(inFile, BF_READ);

    if(nullptr == bInFile)
    {
        fprintf(stderr, "Error: Unable to create binary input file\n");
        return -1;
    }

    if(MODEL_STATIC == model)
    {
        /* build probability ranges from header in file */
        if(0 != ReadHeader(bInFile, &stats))
        {
            BitFileClose(bInFile);
            fclose(outFile);
            return -1;
        }
    }
    else
    {
        /* initialize ranges for adaptive model */
        InitializeAdaptiveProbabilityRangeList(&stats);
    }

    /* read start of code and initialize bounds, and adaptive ranges */
    InitializeDecoder(bInFile, &stats);

    /* decode one symbol at a time */
    for(;;)
    {
        unscaled = GetUnscaledCode(&stats);

        if((c = GetSymbolFromProbability(unscaled, &stats)) == -1)
            break;

        if(c == EOF_CHAR)
            break;

        fputc((char)c, outFile);

        ApplySymbolRange(c, &stats, model);
        ReadEncodedBits(bInFile, &stats);
    }

    inFile = BitFileToFILE(bInFile);

    return 0;
}

static int ReadHeader(bit_file_t *bfpIn, stats_t *stats)
{
    int c;
    probability_t count;

    PrintDebug(("HEADER:\n"));
    stats->cumulativeProb = 0;

    for(c = 0; c <= UPPER(EOF_CHAR); c++)
    {
        stats->ranges[UPPER(c)] = 0;
    }

    for(;;)
    {
        c = BitFileGetChar(bfpIn);
        count = 0;

        if(BitFileGetBitsNum(bfpIn, &count, (PRECISION - 2), sizeof(probability_t)) == EOF)
        {
            fprintf(stderr, "Error: unexpected EOF\n");
            return -1;
        }

        PrintDebug(("%02X\t%d\n", c, count));

        if(count == 0)
            break;

        stats->ranges[UPPER(c)] = count;
        stats->cumulativeProb += count;
    }

    SymbolCountToProbabilityRanges(stats);
    return 0;
}

static void InitializeDecoder(bit_file_t *bfpIn, stats_t *stats)
{
    int i;
    stats->code = 0;

    for(i = 0; i < (int)PRECISION; i++)
    {
        stats->code <<= 1;
        if(BitFileGetBit(bfpIn) == 1)
            stats->code |= 1;
    }

    stats->lower = 0;
    stats->upper = ~0;
}

static probability_t GetUnscaledCode(stats_t *stats)
{
    unsigned long range;
    unsigned long unscaled;

    range = (unsigned long)(stats->upper - stats->lower) + 1;

    unscaled = (unsigned long)(stats->code - stats->lower) + 1;
    unscaled = unscaled * (unsigned long)(stats->cumulativeProb) - 1;
    unscaled /= range;

    return ((probability_t)unscaled);
}

static int GetSymbolFromProbability(probability_t probability, stats_t *stats)
{
    int first, last, middle;

    first = 0;
    last = UPPER(EOF_CHAR);
    middle = last / 2;

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

static void ReadEncodedBits(bit_file_t *bfpIn, stats_t *stats)
{
    int nextBit;        /* next bit from encoded input */

    for(;;)
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
