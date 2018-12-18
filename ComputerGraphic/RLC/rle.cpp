#include "pch.h"
#include <stdio.h>
#include <limits.h>
#include <errno.h>

int RleEncodeFile(FILE *inFile, FILE *outFile)
{
    int currChar;                       /* current characters */
    int prevChar;                       /* previous characters */
    unsigned char count;                /* number of characters in a run */

    /* validate input and output files */
    if ((NULL == inFile) || (NULL == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    /* encode inFile */
    prevChar = EOF;     /* force next char to be different */
    count = 0;

    /* read input until there's nothing left */
    while ((currChar = fgetc(inFile)) != EOF)
    {
        fputc(currChar, outFile);

        /* check for run */
        if (currChar == prevChar)
        {
            /* we have a run.  count run length */
            count = 0;

            while ((currChar = fgetc(inFile)) != EOF)
            {
                if (currChar == prevChar)
                {
                    count++;

                    if (count == UCHAR_MAX)
                    {
                        /* count is as long as it can get */
                        fputc(count, outFile);

                        /* force next char to be different */
                        prevChar = EOF;
                        break;
                    }
                }
                else
                {
                    /* run ended */
                    fputc(count, outFile);
                    fputc(currChar, outFile);
                    prevChar = currChar;
                    break;
                }
            }
        }
        else
        {
            /* no run */
            prevChar = currChar;
        }

        if (currChar == EOF)
        {
            /* run ended because of EOF */
            fputc(count, outFile);
            break;
        }
    }

    return 0;
}

int RleDecodeFile(FILE *inFile, FILE *outFile)
{
    int currChar;                       /* current characters */
    int prevChar;                       /* previous characters */
    unsigned char count;                /* number of characters in a run */

    /* validate input and output files */
    if ((NULL == inFile) || (NULL == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    /* decode inFile */
    prevChar = EOF;     /* force next char to be different */

    /* read input until there's nothing left */
    while ((currChar = fgetc(inFile)) != EOF)
    {
        fputc(currChar, outFile);

        /* check for run */
        if (currChar == prevChar)
        {
            /* we have a run.  write it out. */
            count = fgetc(inFile);
            while (count > 0)
            {
                fputc(currChar, outFile);
                count--;
            }

            prevChar = EOF;     /* force next char to be different */
        }
        else
        {
            /* no run */
            prevChar = currChar;
        }
    }

    return 0;
}
