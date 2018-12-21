#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include "arcode.h"
#include <experimental/filesystem>

void main(int argc, const char* argv[])
{
    if(argc == 0)
        return;

    std::experimental::filesystem::path filePath = argv[1];
    std::string ext = filePath.extension().string();
    bool encode = ext.compare(".Arc") != 0;

    FILE* inFile = fopen(filePath.string().c_str(), "rb");
    if(encode)
        filePath.replace_extension(".Arc");
    else
        filePath.replace_extension("_decArc.pgm");
    FILE* outFile = fopen(filePath.string().c_str(), "wb");

    if(inFile == nullptr)
    {
        if(outFile != nullptr)
            delete outFile;
        return;
    }
    else if(outFile == nullptr)
    {
        delete inFile;
        return;
    }

    if(encode)
        ArEncodeFile(inFile, outFile);
    else
        ArDecodeFile(inFile, outFile);

    fclose(inFile);
    fclose(outFile);
}