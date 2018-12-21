#include "pch.h"
#include "optlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "huffman.h"

typedef enum
{
    COMPRESS,
    DECOMPRESS
} mode_t;

static void ShowUsage(FILE* stream, const char* progPath);

void main(int argc, const char* argv[])
{
    option_t* optList = GetOptList(argc, argv, "cdni:o:h?");
    option_t* thisOpt = optList;

    FILE* inFile = nullptr;
    FILE* outFile = nullptr;
    mode_t mode;
    while(thisOpt != nullptr)
    {
        switch(thisOpt->option)
        {
        case 'c':
        {
            mode = COMPRESS;
            break;
        }
        case 'd':
        {
            mode = DECOMPRESS;
            break;
        }
        case 'i':
        {
            if(inFile != nullptr)
            {
                fprintf(stderr, "Multiple input files not allowed.\n");
                fclose(inFile);

                if(outFile != nullptr)
                    fclose(outFile);

                FreeOptList(optList);
            }
            else if((inFile = fopen(thisOpt->argument, "rb")) == nullptr)
            {
                perror("Opening Input File");

                if(outFile != nullptr)
                    fclose(outFile);

                FreeOptList(optList);
            }
            break;
        }
        case 'o':
        {
            if(outFile != nullptr)
            {
                fprintf(stderr, "Multiple output files not allowed.\n");
                fclose(outFile);

                if(inFile != nullptr)
                    fclose(inFile);

                FreeOptList(optList);
            }
            else if((outFile = fopen(thisOpt->argument, "wb")) == nullptr)
            {
                perror("Opening Output File");

                if(inFile != nullptr)
                    fclose(inFile);

                FreeOptList(optList);
            }
            break;
        }
        case 'h':
        case '?':
        {
            ShowUsage(stdout, argv[0]);
            FreeOptList(optList);
        }
        }
        optList = thisOpt->next;
        delete thisOpt;
        thisOpt = optList;
    }

    if((inFile == nullptr) || (outFile == nullptr))
    {
        fprintf(stderr, "Input and output files must be provided\n\n");
        ShowUsage(stderr, argv[0]);
    }

    if(mode == COMPRESS)
        HuffmanEncodeFile(inFile, outFile);
    else
        HuffmanDecodeFile(inFile, outFile);

    fclose(inFile);
    fclose(outFile);
}

static void ShowUsage(FILE* stream, const char* progPath)
{
    fprintf(stream, "Usage: %s <options>\n\n", FindFileName(progPath));
    fprintf(stream, "options:\n");
    fprintf(stream, "  -C : Encode/Decode using a canonical code.\n");
    fprintf(stream, "  -c : Encode input file to output file.\n");
    fprintf(stream, "  -d : Decode input file to output file.\n");
    fprintf(stream,
        "  -t : Generate code tree for input file to output file.\n");
    fprintf(stream, "  -i<filename> : Name of input file.\n");
    fprintf(stream, "  -o<filename> : Name of output file.\n");
    fprintf(stream, "  -h|?  : Print out command line options.\n\n");
}
