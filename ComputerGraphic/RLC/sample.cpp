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

static void ShowUsage(const char* progName);

void main(int argc, const char* argv[])
{
    FILE* inFile = nullptr;
    FILE* outFile = nullptr;
    
    option_t* optList = GetOptList(argc, argv, "cvdni:o:h?");
    option_t* thisOpt = optList;
    int mode = mode_none;
    while (thisOpt != nullptr)
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
                if (inFile != nullptr)
                {
                    fprintf(stderr, "Multiple input files not allowed.\n");
                    fclose(inFile);

                    if (outFile != nullptr)
                        fclose(outFile);

                    FreeOptList(optList);
                }
                else if ((inFile = fopen(thisOpt->argument, "rb")) == nullptr)
                {
                    perror("Opening Input File");

                    if (outFile != nullptr)
                        fclose(outFile);

                    FreeOptList(optList);
                }
                break;

            case 'o':       /* output file name */
                if (outFile != nullptr)
                {
                    fprintf(stderr, "Multiple output files not allowed.\n");
                    fclose(outFile);

                    if (inFile != nullptr)
                        fclose(inFile);

                    FreeOptList(optList);
                }
                else if ((outFile = fopen(thisOpt->argument, "wb")) == nullptr)
                {
                    perror("Opening Output File");

                    if (inFile != nullptr)
                        fclose(inFile);

                    FreeOptList(optList);
                }
                break;

            case 'h':
            case '?':
                ShowUsage(argv[0]);
                FreeOptList(optList);
        }

        optList = thisOpt->next;
        delete thisOpt;
        thisOpt = optList;
    }

    /* validate command line */
    if (inFile == nullptr)
    {
        fprintf(stderr, "Input file must be provided\n");
        ShowUsage(argv[0]);

        if (outFile != nullptr)
            delete outFile;
    }
    else if (outFile == nullptr)
    {
        fprintf(stderr, "Output file must be provided\n");
        ShowUsage(argv[0]);
        delete inFile;
    }

    switch (mode)
    {
        case mode_encode_normal:
            RleEncodeFile(inFile, outFile);
            break;

        case mode_decode_normal:
            RleDecodeFile(inFile, outFile);
            break;

        case mode_encode_packbits:
            VPackBitsEncodeFile(inFile, outFile);
            break;

        case mode_decode_packbits:
            VPackBitsDecodeFile(inFile, outFile);
            break;

        default:
            fprintf(stderr, "Illegal encoding/decoding option\n");
            ShowUsage(argv[0]);
    }

    fclose(inFile);
    fclose(outFile);
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
