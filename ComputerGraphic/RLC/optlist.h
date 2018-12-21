
#ifndef OPTLIST_H
#define OPTLIST_H

#define    OL_NOINDEX    -1

typedef struct option_t
{
    char option;
    const char* argument;
    int argIndex;
    struct option_t* next;
} option_t;

option_t* GetOptList(int argc, const char* argv[], const char* options);

void FreeOptList(option_t* list);

char* FindFileName(const char* fullPath);

#endif
