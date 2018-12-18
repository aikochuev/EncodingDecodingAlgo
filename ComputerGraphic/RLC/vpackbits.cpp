#include "pch.h"
#include <stdio.h>
#include <limits.h>
#include <errno.h>

#define MIN_RUN     3                   /* minimum run length to encode */
#define MAX_RUN     (128 + MIN_RUN - 1) /* maximum run length to encode */
#define MAX_COPY    128                 /* maximum characters to copy */

/* maximum that can be read before copy block is written */
#define MAX_READ    (MAX_COPY + MIN_RUN - 1)

int VPackBitsEncodeFile(FILE *inFile, FILE *outFile)
{
    int currChar;                       /* current character */
    unsigned char charBuf[MAX_READ];    /* buffer of already read characters */
    unsigned char count;                /* number of characters in a run */

    /* validate input and output files */
    if ((NULL == inFile) || (NULL == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    /* prime the read loop */
    currChar = fgetc(inFile);
    count = 0;

    /* read input until there's nothing left */
    while (currChar != EOF)
    {
        charBuf[count] = (unsigned char)currChar;

        count++;

        if (count >= MIN_RUN)
        {
            int i;

            /* check for run  charBuf[count - 1] .. charBuf[count - MIN_RUN]*/
            for (i = 2; i <= MIN_RUN; i++)
            {
                if (currChar != charBuf[count - i])
                {
                    /* no run */
                    i = 0;
                    break;
                }
            }

            if (i != 0)
            {
                /* we have a run write out buffer before run*/
                int nextChar;

                if (count > MIN_RUN)
                {
                    /* block size - 1 followed by contents */
                    fputc(count - MIN_RUN - 1, outFile);
                    fwrite(charBuf, sizeof(unsigned char), count - MIN_RUN,
                        outFile);
                }


                /* determine run length (MIN_RUN so far) */
                count = MIN_RUN;

                while ((nextChar = fgetc(inFile)) == currChar)
                {
                    count++;
                    if (MAX_RUN == count)
                    {
                        /* run is at max length */
                        break;
                    }
                }

                /* write out encoded run length and run symbol */
                fputc((char)((int)(MIN_RUN - 1) - (int)(count)), outFile);
                fputc(currChar, outFile);

                if ((nextChar != EOF) && (count != MAX_RUN))
                {
                    /* make run breaker start of next buffer */
                    charBuf[0] = nextChar;
                    count = 1;
                }
                else
                {
                    /* file or max run ends in a run */
                    count = 0;
                }
            }
        }

        if (MAX_READ == count)
        {
            int i;

            /* write out buffer */
            fputc(MAX_COPY - 1, outFile);
            fwrite(charBuf, sizeof(unsigned char), MAX_COPY, outFile);

            /* start a new buffer */
            count = MAX_READ - MAX_COPY;

            /* copy excess to front of buffer */
            for (i = 0; i < count; i++)
            {
                charBuf[i] = charBuf[MAX_COPY + i];
            }
        }

        currChar = fgetc(inFile);
    }

    /* write out last buffer */
    if (0 != count)
    {
        if (count <= MAX_COPY)
        {
            /* write out entire copy buffer */
            fputc(count - 1, outFile);
            fwrite(charBuf, sizeof(unsigned char), count, outFile);
        }
        else
        {
            /* we read more than the maximum for a single copy buffer */
            fputc(MAX_COPY - 1, outFile);
            fwrite(charBuf, sizeof(unsigned char), MAX_COPY, outFile);

            /* write out remainder */
            count -= MAX_COPY;
            fputc(count - 1, outFile);
            fwrite(&charBuf[MAX_COPY], sizeof(unsigned char), count, outFile);
        }
    }

    return 0;
}

int VPackBitsDecodeFile(FILE *inFile, FILE *outFile)
{
    int countChar;                      /* run/copy count */
    int currChar;                       /* current character */

    /* validate input and output files */
    if ((NULL == inFile) || (NULL == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    /* decode inFile */

    /* read input until there's nothing left */
    while ((countChar = fgetc(inFile)) != EOF)
    {
        countChar = (char)countChar;    /* force sign extension */

        if (countChar < 0)
        {
            /* we have a run write out  2 - countChar copies */
            countChar = (MIN_RUN - 1) - countChar;

            if (EOF == (currChar = fgetc(inFile)))
            {
                fprintf(stderr, "Run block is too short!\n");
                countChar = 0;
            }

            while (countChar > 0)
            {
                fputc(currChar, outFile);
                countChar--;
            }
        }
        else
        {
            /* we have a block of countChar + 1 symbols to copy */
            for (countChar++; countChar > 0; countChar--)
            {
                if ((currChar = fgetc(inFile)) != EOF)
                {
                    fputc(currChar, outFile);
                }
                else
                {
                    fprintf(stderr, "Copy block is too short!\n");
                    break;
                }
            }
        }
    }

    return 0;
}
