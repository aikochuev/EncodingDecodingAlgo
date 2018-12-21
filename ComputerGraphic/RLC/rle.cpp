#include "pch.h"
#include <stdio.h>
#include <limits.h>
#include <errno.h>

#define MIN_RUN     3                   /* minimum run length to encode */
#define MAX_RUN     (128 + MIN_RUN - 1) /* maximum run length to encode */
#define MAX_COPY    128                 /* maximum characters to copy */

/* maximum that can be read before copy block is written */
#define MAX_READ    (MAX_COPY + MIN_RUN - 1)

int RleEncodeFile(FILE* inFile, FILE* outFile)
{
    if((nullptr == inFile) || (nullptr == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    int currChar = fgetc(inFile);
    unsigned char count = 0;
    unsigned char charBuf[MAX_READ];
    while(currChar != EOF)
    {
        charBuf[count] = (unsigned char)currChar;
        count++;
        if(count >= MIN_RUN)
        {
            int i;
            for(i = 2; i <= MIN_RUN; i++)
            {
                if(currChar != charBuf[count - i])
                {
                    i = 0;
                    break;
                }
            }

            if(i != 0)
            {
                int nextChar;
                if(count > MIN_RUN)
                {
                    fputc(count - MIN_RUN - 1, outFile);
                    fwrite(charBuf, sizeof(unsigned char), count - MIN_RUN,
                        outFile);
                }

                count = MIN_RUN;
                while((nextChar = fgetc(inFile)) == currChar)
                {
                    count++;
                    if(MAX_RUN == count)
                        break;
                }

                fputc((char)((int)(MIN_RUN - 1) - (int)(count)), outFile);
                fputc(currChar, outFile);

                if((nextChar != EOF) && (count != MAX_RUN))
                {
                    charBuf[0] = nextChar;
                    count = 1;
                }
                else
                {
                    count = 0;
                }
            }
        }

        if(MAX_READ == count)
        {
            int i;
            fputc(MAX_COPY - 1, outFile);
            fwrite(charBuf, sizeof(unsigned char), MAX_COPY, outFile);
            count = MAX_READ - MAX_COPY;

            for(i = 0; i < count; i++)
                charBuf[i] = charBuf[MAX_COPY + i];
        }
        currChar = fgetc(inFile);
    }

    if(0 != count)
    {
        if(count <= MAX_COPY)
        {
            fputc(count - 1, outFile);
            fwrite(charBuf, sizeof(unsigned char), count, outFile);
        }
        else
        {
            fputc(MAX_COPY - 1, outFile);
            fwrite(charBuf, sizeof(unsigned char), MAX_COPY, outFile);

            count -= MAX_COPY;
            fputc(count - 1, outFile);
            fwrite(&charBuf[MAX_COPY], sizeof(unsigned char), count, outFile);
        }
    }
    return 0;
}

int RleDecodeFile(FILE* inFile, FILE* outFile)
{
    if((nullptr == inFile) || (nullptr == outFile))
    {
        errno = ENOENT;
        return -1;
    }

    int countChar;
    int currChar;
    while((countChar = fgetc(inFile)) != EOF)
    {
        countChar = (char)countChar;

        if(countChar < 0)
        {
            countChar = (MIN_RUN - 1) - countChar;
            if(EOF == (currChar = fgetc(inFile)))
            {
                fprintf(stderr, "Run block is too short!\n");
                countChar = 0;
            }

            while(countChar > 0)
            {
                fputc(currChar, outFile);
                countChar--;
            }
        }
        else
        {
            for(countChar++; countChar > 0; countChar--)
            {
                if((currChar = fgetc(inFile)) != EOF)
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
