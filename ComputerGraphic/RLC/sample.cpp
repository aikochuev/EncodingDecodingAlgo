#include "pch.h"
#include "optlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "rle.h"

typedef enum
{
    mode_none = 0,
    mode_encode_normal = 1,
    mode_decode_normal = (1 << 1),
    mode_packbits = (1 << 2),
    mode_encode_packbits = (1 << 2) | 1,
    mode_decode_packbits = (1 << 2) | (1 << 1)
} mode_t;

static void ShowUsage(const char *progName);

int main(int argc, const char* argv[])
{
    FILE *inFile;
    FILE *outFile;
    int mode;
    int result;

    /* initialize data */
    inFile = NULL;
    outFile = NULL;
    mode = mode_none;

    /* parse command line */
    option_t* optList = GetOptList(argc, argv, "cvdni:o:h?");
    option_t* thisOpt = optList;

    while (thisOpt != NULL)
    {
        switch(thisOpt->option)
        {
            case 'c':       /* compression mode */
                mode |= mode_encode_normal;
                break;

            case 'd':       /* decompression mode */
                mode |= mode_decode_normal;
                break;

            case 'v':       /* use packbits variant */
                mode |= mode_packbits;
                break;

            case 'i':       /* input file name */
                if (inFile != NULL)
                {
                    fprintf(stderr, "Multiple input files not allowed.\n");
                    fclose(inFile);

                    if (outFile != NULL)
                    {
                        fclose(outFile);
                    }

                    FreeOptList(optList);
                    return EINVAL;
                }
                else if ((inFile = fopen(thisOpt->argument, "rb")) == NULL)
                {
                    perror("Opening Input File");

                    if (outFile != NULL)
                    {
                        fclose(outFile);
                    }

                    FreeOptList(optList);
                    return errno;
                }
                break;

            case 'o':       /* output file name */
                if (outFile != NULL)
                {
                    fprintf(stderr, "Multiple output files not allowed.\n");
                    fclose(outFile);

                    if (inFile != NULL)
                    {
                        fclose(inFile);
                    }

                    FreeOptList(optList);
                    return EINVAL;
                }
                else if ((outFile = fopen(thisOpt->argument, "wb")) == NULL)
                {
                    perror("Opening Output File");

                    if (inFile != NULL)
                    {
                        fclose(inFile);
                    }

                    FreeOptList(optList);
                    return errno;
                }
                break;

            case 'h':
            case '?':
                ShowUsage(argv[0]);
                FreeOptList(optList);
                return 0;
        }

        optList = thisOpt->next;
        free(thisOpt);
        thisOpt = optList;
    }

    /* validate command line */
    if (inFile == NULL)
    {
        fprintf(stderr, "Input file must be provided\n");
        ShowUsage(argv[0]);

        if (outFile != NULL)
        {
            free(outFile);
        }

        return EINVAL;
    }
    else if (outFile == NULL)
    {
        fprintf(stderr, "Output file must be provided\n");
        ShowUsage(argv[0]);

        if (inFile != NULL)
        {
            free(inFile);
        }

        return EINVAL;
    }

    /* we have valid parameters encode or decode */
    switch (mode)
    {
        case mode_encode_normal:
            result = RleEncodeFile(inFile, outFile);
            break;

        case mode_decode_normal:
            result = RleDecodeFile(inFile, outFile);
            break;

        case mode_encode_packbits:
            result = VPackBitsEncodeFile(inFile, outFile);
            break;

        case mode_decode_packbits:
            result = VPackBitsDecodeFile(inFile, outFile);
            break;

        default:
            fprintf(stderr, "Illegal encoding/decoding option\n");
            ShowUsage(argv[0]);
            result = EINVAL;
    }

    fclose(inFile);
    fclose(outFile);
    return result;
}

static void ShowUsage(const char *progName)
{
    printf("Usage: %s <options>\n\n", FindFileName(progName));
    printf("options:\n");
    printf("  -c : Encode input file to output file.\n");
    printf("  -d : Decode input file to output file.\n");
    printf("  -v : Use variant of packbits algorithm.\n");
    printf("  -i <filename> : Name of input file.\n");
    printf("  -o <filename> : Name of output file.\n");
    printf("  -h | ?  : Print out command line options.\n\n");
    printf("Default: sample -c\n");
}
