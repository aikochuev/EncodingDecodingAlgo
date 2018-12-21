#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include "optlist.h"
#include "arcode.h"

static void ShowUsage(const char* progName);

void main(int argc, const char* argv[])
{
    option_t* optList = GetOptList(argc, argv, "acdi:o:h?");
    option_t* thisOpt = optList;
    FILE* inFile = nullptr;
    FILE* outFile = nullptr;
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
                exit(EXIT_FAILURE);
            }
            else if((inFile = fopen(thisOpt->argument, "rb")) == nullptr)
            {
                perror("Opening Input File");
                if(outFile != nullptr)
                    fclose(outFile);

                FreeOptList(optList);
                exit(EXIT_FAILURE);
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
                exit(EXIT_FAILURE);
            }
            else if((outFile = fopen(thisOpt->argument, "wb")) == nullptr)
            {
                perror("Opening Output File");

                if(inFile != nullptr)
                    fclose(inFile);

                FreeOptList(optList);
                exit(EXIT_FAILURE);
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

    if(nullptr == inFile)
    {
        fprintf(stderr, "Input file must be provided\n");
        ShowUsage(argv[0]);

        if(outFile != nullptr)
            fclose(outFile);

        exit(EXIT_FAILURE);
    }
    else if(nullptr == outFile)
    {
        fprintf(stderr, "Input file must be provided\n");
        ShowUsage(argv[0]);
        fclose(inFile);
        exit(EXIT_FAILURE);
    }

    if(encode)
        ArEncodeFile(inFile, outFile);
    else
        ArDecodeFile(inFile, outFile);

    fclose(inFile);
    fclose(outFile);
}

static void ShowUsage(const char* progName)
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