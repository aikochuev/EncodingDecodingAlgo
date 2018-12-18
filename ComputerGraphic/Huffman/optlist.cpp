#include "pch.h"
#include "optlist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static option_t *MakeOpt(const char option, const char* argument, const int index);
static size_t MatchOpt(const char argument, const char* options);

option_t *GetOptList(const int argc, const char* argv[], const char* options)
{
    int nextArg;
    size_t optIndex;
    size_t argIndex;

    /* start with first argument and nothing found */
    nextArg = 1;
    option_t* head = nullptr;
    option_t* tail = nullptr;

    /* loop through all of the command line arguments */
    while (nextArg < argc)
    {
        argIndex = 1;

        while ((strlen(argv[nextArg]) > argIndex) && ('-' == argv[nextArg][0]))
        {
            /* attempt to find a matching option */
            optIndex = MatchOpt(argv[nextArg][argIndex], options);

            if (options[optIndex] == argv[nextArg][argIndex])
            {
                /* we found the matching option */
                if (nullptr == head)
                {
                    head = MakeOpt(options[optIndex], nullptr, OL_NOINDEX);
                    tail = head;
                }
                else
                {
                    tail->next = MakeOpt(options[optIndex], nullptr, OL_NOINDEX);
                    tail = tail->next;
                }

                if (':' == options[optIndex + 1])
                {
                    /* the option found should have a text arguement */
                    argIndex++;

                    if (strlen(argv[nextArg]) > argIndex)
                    {
                        /* no space between argument and option */
                        tail->argument = &(argv[nextArg][argIndex]);
                        tail->argIndex = nextArg;
                    }
                    else if (nextArg < argc)
                    {
                        /* there must be space between the argument option */
                        nextArg++;
                        tail->argument = argv[nextArg];
                        tail->argIndex = nextArg;
                    }

                    break; /* done with argv[nextArg] */
                }
            }

            argIndex++;
        }

        nextArg++;
    }
    return head;
}

static option_t *MakeOpt(const char option, const char* argument, const int index)
{
    option_t *opt = new option_t();
    opt->option = option;
    opt->argument = argument;
    opt->argIndex = index;
    opt->next = nullptr;

    return opt;
}

void FreeOptList(option_t *list)
{
    option_t *head = list;
    list = nullptr;
    option_t *next = nullptr;
    while (head != nullptr)
    {
        next = head->next;
        delete head;
        head = next;
    }
    return;
}

static size_t MatchOpt(char argument, const char* options)
{
    size_t optIndex = 0;

    /* attempt to find a matching option */
    while ((options[optIndex] != '\0') &&
        (options[optIndex] != argument))
    {
        do
        {
            optIndex++;
        }
        while ((options[optIndex] != '\0') &&
            (':' == options[optIndex]));
    }

    return optIndex;
}

char *FindFileName(const char* fullPath)
{
    int i;
    const char* start;                          /* start of file name */
    const char* tmp;
    const char delim[3] = {'\\', '/', ':'};     /* path deliminators */

    start = fullPath;

    /* find the first character after all file path delimiters */
    for (i = 0; i < 3; i++)
    {
        tmp = strrchr(start, delim[i]);

        if (tmp != nullptr)
        {
            start = tmp + 1;
        }
    }

    return (char *)start;
}
