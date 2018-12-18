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

int ArEncodeFile(FILE *inFile, FILE *outFile, const model_t model)
{
    int c;
    bit_file_t *bOutFile;               /* encoded output */
    stats_t stats;                      /* statistics for symbols and file */

    /* open input and output files */
    if (NULL == inFile)
    {
        inFile = stdin;
    }

    if (outFile == NULL)
    {
        bOutFile = MakeBitFile(stdout, BF_WRITE);
    }
    else
    {
        bOutFile = MakeBitFile(outFile, BF_WRITE);
    }

    if (NULL == bOutFile)
    {
        fprintf(stderr, "Error: Creating binary output file\n");
        return -1;
    }

    if (MODEL_STATIC == model)
    {
        /* create list of probability ranges by counting symbols in file */
        if (0 != BuildProbabilityRangeList(inFile, &stats))
        {
            fclose(inFile);
            BitFileClose(bOutFile);
            fprintf(stderr, "Error determining frequency ranges.\n");
            return -1;
        }

        rewind(inFile);

        /* write information required to decode file to encoded file */
        WriteHeader(bOutFile, &stats);
    }
    else
    {
        /* initialize probability ranges assuming uniform distribution */
        InitializeAdaptiveProbabilityRangeList(&stats);
    }

    /* initialize coder start with full probability range [0%, 100%) */
    stats.lower = 0;
    stats.upper = ~0;                                  /* all ones */
    stats.underflowBits = 0;

    /* encode symbols one at a time */
    while ((c = fgetc(inFile)) != EOF)
    {
        ApplySymbolRange(c, &stats, model);
        WriteEncodedBits(bOutFile, &stats);
    }

    ApplySymbolRange(EOF_CHAR, &stats, model);   /* encode an EOF */
    WriteEncodedBits(bOutFile, &stats);
    WriteRemaining(bOutFile, &stats);           /* write out lsbs */
    outFile = BitFileToFILE(bOutFile);          /* make file normal again */

    return 0;
}

static void SymbolCountToProbabilityRanges(stats_t *stats)
{
    int c;

    stats->ranges[0] = 0;                  /* absolute lower bound is 0 */
    stats->ranges[UPPER(EOF_CHAR)] = 1;    /* add 1 EOF character */
    stats->cumulativeProb++;

    /* assign upper and lower probability ranges */
    for (c = 1; c <= UPPER(EOF_CHAR); c++)
    {
        stats->ranges[c] += stats->ranges[c - 1];
    }

    /* dump list of ranges */
    PrintDebug(("Ranges:\n"));
    for (c = 0; c < UPPER(EOF_CHAR); c++)
    {
        PrintDebug(("%02X\t%d\t%d\n", c, stats->ranges[LOWER(c)],
            stats->ranges[UPPER(c)]));
    }

    return;
}

static int BuildProbabilityRangeList(FILE *fpIn, stats_t *stats)
{
    int c;
    unsigned long countArray[EOF_CHAR];
    unsigned long totalCount = 0;
    unsigned long rescaleValue;

    if (fpIn == NULL)
    {
        return -1;
    }

    /* start with no symbols counted */
    for (c = 0; c < EOF_CHAR; c++)
    {
        countArray[c] = 0;
    }

    while ((c = fgetc(fpIn)) != EOF)
    {
        if (totalCount == ULONG_MAX)
        {
            fprintf(stderr, "Error: file too large\n");
            return -1;
        }

        countArray[c]++;
        totalCount++;
    }

    /* rescale counts to be < MAX_PROBABILITY */
    if (totalCount >= MAX_PROBABILITY)
    {
        rescaleValue = (totalCount / MAX_PROBABILITY) + 1;

        for (c = 0; c < EOF_CHAR; c++)
        {
            if (countArray[c] > rescaleValue)
            {
                countArray[c] /= rescaleValue;
            }
            else if (countArray[c] != 0)
            {
                countArray[c] = 1;
            }
        }
    }

    /* copy scaled symbol counts to range list */
    stats->ranges[0] = 0;
    stats->cumulativeProb = 0;
    for (c = 0; c < EOF_CHAR; c++)
    {
        stats->ranges[UPPER(c)] = countArray[c];
        stats->cumulativeProb += countArray[c];
    }

    /* convert counts to a range of probabilities */
    SymbolCountToProbabilityRanges(stats);
    return 0;
}

static void WriteHeader(bit_file_t *bfpOut, stats_t *stats)
{
    int c;
    probability_t previous = 0;         /* symbol count so far */

    PrintDebug(("HEADER:\n"));

    for(c = 0; c <= (EOF_CHAR - 1); c++)
    {
        if (stats->ranges[UPPER(c)] > previous)
        {
            /* some of these symbols will be encoded */
            BitFilePutChar((char)c, bfpOut);
            previous = (stats->ranges[UPPER(c)] - previous); /* symbol count */
            PrintDebug(("%02X\t%d\n", c, previous));

            /* write out PRECISION - 2 bit count */
            BitFilePutBitsNum(bfpOut, &previous, (PRECISION - 2),
                sizeof(probability_t));

            /* current upper range is previous for the next character */
            previous = stats->ranges[UPPER(c)];
        }
    }

    /* now write end of table (zero count) */
    BitFilePutChar(0x00, bfpOut);
    previous = 0;
    BitFilePutBits(bfpOut, (void *)&previous, PRECISION - 2);
}

static void InitializeAdaptiveProbabilityRangeList(stats_t *stats)
{
    int c;

    stats->ranges[0] = 0;      /* absolute lower range */

    /* assign upper and lower probability ranges assuming uniformity */
    for (c = 1; c <= UPPER(EOF_CHAR); c++)
    {
        stats->ranges[c] = stats->ranges[c - 1] + 1;
    }

    stats->cumulativeProb = UPPER(EOF_CHAR);

    /* dump list of ranges */
    PrintDebug(("Ranges:\n"));
    for (c = 0; c < UPPER(EOF_CHAR); c++)
    {
        PrintDebug(("%02X\t%d\t%d\n", c, stats->ranges[LOWER(c)],
            stats->ranges[UPPER(c)]));
    }

    return;
}

static void ApplySymbolRange(int symbol, stats_t *stats, char model)
{
    unsigned long range;        /* must be able to hold max upper + 1 */
    unsigned long rescaled;     /* range rescaled for range of new symbol */
                                /* must hold range * max upper */

    /* for updating dynamic models */
    int i;
    probability_t original;     /* range value prior to rescale */
    probability_t delta;        /* range for individual symbol */

    /***********************************************************************
    * Calculate new upper and lower ranges.  Since the new upper range is
    * dependant of the old lower range, compute the upper range first.
    ***********************************************************************/
    range = (unsigned long)(stats->upper - stats->lower) + 1;

    /* scale upper range of the symbol being coded */
    rescaled = (unsigned long)(stats->ranges[UPPER(symbol)]) * range;
    rescaled /= (unsigned long)(stats->cumulativeProb);

    /* new upper = old lower + rescaled new upper - 1*/
    stats->upper = stats->lower + (probability_t)rescaled - 1;

    /* scale lower range of the symbol being coded */
    rescaled = (unsigned long)(stats->ranges[LOWER(symbol)]) * range;
    rescaled /= (unsigned long)(stats->cumulativeProb);

    /* new lower = old lower + rescaled new upper */
    stats->lower = stats->lower + (probability_t)rescaled;

    if (!model)
    {
        /* add new symbol to model */
        stats->cumulativeProb++;

        for (i = UPPER(symbol); i <= UPPER(EOF_CHAR); i++)
        {
            stats->ranges[i] += 1;
        }

        /* halve current values if cumulativeProb is too large */
        if (stats->cumulativeProb >= MAX_PROBABILITY)
        {
            stats->cumulativeProb = 0;
            original = 0;

            for (i = 1; i <= UPPER(EOF_CHAR); i++)
            {
                delta = stats->ranges[i] - original;
                original = stats->ranges[i];

                if (delta <= 2)
                {
                    /* prevent probability from being 0 */
                    stats->ranges[i] = stats->ranges[i - 1] + 1;
                }
                else
                {
                    stats->ranges[i] = stats->ranges[i - 1] + (delta / 2);
                }

                stats->cumulativeProb +=
                    (stats->ranges[i] - stats->ranges[i - 1]);
            }
        }
    }

    assert(stats->lower <= stats->upper);
}

static void WriteEncodedBits(bit_file_t *bfpOut, stats_t *stats)
{
    for (;;)
    {
        if ((stats->upper & MASK_BIT(0)) == (stats->lower & MASK_BIT(0)))
        {
            /* MSBs match, write them to output file */
            BitFilePutBit((stats->upper & MASK_BIT(0)) != 0, bfpOut);

            /* we can write out underflow bits too */
            while (stats->underflowBits > 0)
            {
                BitFilePutBit((stats->upper & MASK_BIT(0)) == 0, bfpOut);
                stats->underflowBits--;
            }
        }
        else if ((stats->lower & MASK_BIT(1)) && !(stats->upper & MASK_BIT(1)))
        {
            /***************************************************************
            * Possible underflow condition: neither MSBs nor second MSBs
            * match.  It must be the case that lower and upper have MSBs of
            * 01 and 10.  Remove 2nd MSB from lower and upper.
            ***************************************************************/
            stats->underflowBits += 1;
            stats->lower &= ~(MASK_BIT(0) | MASK_BIT(1));
            stats->upper |= MASK_BIT(1);

            /***************************************************************
            * The shifts below make the rest of the bit removal work.  If
            * you don't believe me try it yourself.
            ***************************************************************/
        }
        else
        {
            /* nothing left to do */
            return ;
        }

        stats->lower <<= 1;
        stats->upper <<= 1;
        stats->upper |= 1;
    }
}

static void WriteRemaining(bit_file_t *bfpOut, stats_t *stats)
{
    BitFilePutBit((stats->lower & MASK_BIT(1)) != 0, bfpOut);

    /* write out any unwritten underflow bits */
    for (stats->underflowBits++; stats->underflowBits > 0;
        stats->underflowBits--)
    {
        BitFilePutBit((stats->lower & MASK_BIT(1)) == 0, bfpOut);
    }
}

int ArDecodeFile(FILE *inFile, FILE *outFile, const model_t model)
{
    int c;
    probability_t unscaled;
    bit_file_t *bInFile;
    stats_t stats;                      /* statistics for symbols and file */

    /* handle file pointers */
    if (NULL == outFile)
    {
        outFile = stdout;
    }

    if (NULL == inFile)
    {
        fprintf(stderr, "Error: Invalid input file\n");
        return -1;
    }

    bInFile = MakeBitFile(inFile, BF_READ);

    if (NULL == bInFile)
    {
        fprintf(stderr, "Error: Unable to create binary input file\n");
        return -1;
    }

    if (MODEL_STATIC == model)
    {
        /* build probability ranges from header in file */
        if (0 != ReadHeader(bInFile, &stats))
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
    for (;;)
    {
        /* get the unscaled probability of the current symbol */
        unscaled = GetUnscaledCode(&stats);

        /* figure out which symbol has the above probability */
        if((c = GetSymbolFromProbability(unscaled, &stats)) == -1)
        {
            /* error: unknown symbol */
            break;
        }

        if (c == EOF_CHAR)
        {
            /* no more symbols */
            break;
        }

        fputc((char)c, outFile);

        /* factor out symbol */
        ApplySymbolRange(c, &stats, model);
        ReadEncodedBits(bInFile, &stats);
    }

    inFile = BitFileToFILE(bInFile);        /* make file normal again */

    return 0;
}

static int ReadHeader(bit_file_t *bfpIn, stats_t *stats)
{
    int c;
    probability_t count;

    PrintDebug(("HEADER:\n"));
    stats->cumulativeProb = 0;

    for (c = 0; c <= UPPER(EOF_CHAR); c++)
    {
        stats->ranges[UPPER(c)] = 0;
    }

    /* read [character, probability] sets */
    for (;;)
    {
        c = BitFileGetChar(bfpIn);
        count = 0;

        /* read (PRECISION - 2) bit count */
        if (BitFileGetBitsNum(bfpIn, &count, (PRECISION - 2),
            sizeof(probability_t)) == EOF)
        {
            /* premature EOF */
            fprintf(stderr, "Error: unexpected EOF\n");
            return -1;
        }

        PrintDebug(("%02X\t%d\n", c, count));

        if (count == 0)
        {
            /* 0 count means end of header */
            break;
        }

        stats->ranges[UPPER(c)] = count;
        stats->cumulativeProb += count;
    }

    /* convert counts to range list */
    SymbolCountToProbabilityRanges(stats);
    return 0;
}

static void InitializeDecoder(bit_file_t *bfpIn, stats_t *stats)
{
    int i;

    stats->code = 0;

    /* read PERCISION MSBs of code one bit at a time */
    for (i = 0; i < (int)PRECISION; i++)
    {
        stats->code <<= 1;

        /* treat EOF like 0 */
        if(BitFileGetBit(bfpIn) == 1)
        {
            stats->code |= 1;
        }
    }

    /* start with full probability range [0%, 100%) */
    stats->lower = 0;
    stats->upper = ~0;      /* all ones */
}

static probability_t GetUnscaledCode(stats_t *stats)
{
    unsigned long range;        /* must be able to hold max upper + 1 */
    unsigned long unscaled;

    range = (unsigned long)(stats->upper - stats->lower) + 1;

    /* reverse the scaling operations from ApplySymbolRange */
    unscaled = (unsigned long)(stats->code - stats->lower) + 1;
    unscaled = unscaled * (unsigned long)(stats->cumulativeProb) - 1;
    unscaled /= range;

    return ((probability_t)unscaled);
}

static int GetSymbolFromProbability(probability_t probability, stats_t *stats)
{
    int first, last, middle;    /* indicies for binary search */

    first = 0;
    last = UPPER(EOF_CHAR);
    middle = last / 2;

    /* binary search */
    while (last >= first)
    {
        if (probability < stats->ranges[LOWER(middle)])
        {
            /* lower bound is higher than probability */
            last = middle - 1;
            middle = first + ((last - first) / 2);
            continue;
        }

        if (probability >= stats->ranges[UPPER(middle)])
        {
            /* upper bound is lower than probability */
            first = middle + 1;
            middle = first + ((last - first) / 2);
            continue;
        }

        /* we must have found the right value */
        return middle;
    }

    /* error: none of the ranges include the probability */
    fprintf(stderr, "Unknown Symbol: %d (max: %d)\n", probability,
        stats->ranges[UPPER(EOF_CHAR)]);
    return -1;
}

static void ReadEncodedBits(bit_file_t *bfpIn, stats_t *stats)
{
    int nextBit;        /* next bit from encoded input */

    for (;;)
    {
        if ((stats->upper & MASK_BIT(0)) == (stats->lower & MASK_BIT(0)))
        {
            /* MSBs match, allow them to be shifted out*/
        }
        else if ((stats->lower & MASK_BIT(1)) && !(stats->upper & MASK_BIT(1)))
        {
            /***************************************************************
            * Possible underflow condition: neither MSBs nor second MSBs
            * match.  It must be the case that lower and upper have MSBs of
            * 01 and 10.  Remove 2nd MSB from lower and upper.
            ***************************************************************/
            stats->lower   &= ~(MASK_BIT(0) | MASK_BIT(1));
            stats->upper  |= MASK_BIT(1);
            stats->code ^= MASK_BIT(1);

            /* the shifts below make the rest of the bit removal work */
        }
        else
        {
            /* nothing to shift out */
            return;
        }

        stats->lower <<= 1;
        stats->upper <<= 1;
        stats->upper |= 1;
        stats->code <<= 1;

        if ((nextBit = BitFileGetBit(bfpIn)) == EOF)
        {
            /* either all bits are shifted out or error occurred */
        }
        else
        {
            stats->code |= nextBit;     /* add next encoded bit to code */
        }
    }

    return;
}
