#include "pch.h"
#include "optlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "rle.h"

static void ShowUsage(const char* progName);

void main(int argc, const char* argv[])
{
    FILE* inFile = nullptr;
    FILE* outFile = nullptr;
    option_t* optList = GetOptList(argc, argv, "cdni:o:h?");
    option_t* thisOpt = optList;
    bool encode = true;
    while(thisOpt != nullptr)
    {
        switch(thisOpt->option)
        {
        case 'c':
        {
            encode = true;
            break;
        }
        case 'd':
        {
            encode = false;
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
            ShowUsage(argv[0]);
            FreeOptList(optList);
        }
        }

        optList = thisOpt->next;
        delete thisOpt;
        thisOpt = optList;
    }

    if(inFile == nullptr)
    {
        fprintf(stderr, "Input file must be provided\n");
        ShowUsage(argv[0]);

        if(outFile != nullptr)
            delete outFile;
    }
    else if(outFile == nullptr)
    {
        fprintf(stderr, "Output file must be provided\n");
        ShowUsage(argv[0]);
        delete inFile;
    }

    if(encode)
        RleEncodeFile(inFile, outFile);
    else
        RleDecodeFile(inFile, outFile);

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
