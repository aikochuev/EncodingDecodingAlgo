#include "pch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "huffman.h"
#include <experimental/filesystem>

void main(int argc, const char* argv[])
{
    if(argc == 0)
        return;

    std::experimental::filesystem::path filePath = argv[1];
    std::string ext = filePath.extension().string();

    if(ext.compare(".Rlc") == 0 || ext.compare(".Arc") == 0)
        return;

    bool encode = ext.compare(".Huffman") != 0;
    FILE* inFile = fopen(filePath.string().c_str(), "rb");
    if(encode)
        filePath.replace_extension(".Huffman");
    else
        filePath.replace_extension("_decHuffman.pgm");
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
        HuffmanEncodeFile(inFile, outFile);
    else
        HuffmanDecodeFile(inFile, outFile);

    fclose(inFile);
    fclose(outFile);
}

