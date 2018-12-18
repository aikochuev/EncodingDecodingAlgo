#ifndef _ARCODE_H_
#define _ARCODE_H_

typedef enum
{
    MODEL_ADAPTIVE = 0,
    MODEL_STATIC = 1
} model_t;

 /* encode/decode routines from inFile to outFile.  returns 0 on success */
int ArEncodeFile(FILE *inFile, FILE *outFile, const model_t model);
int ArDecodeFile(FILE *inFile, FILE *outFile, const model_t model);

#endif  /* ndef _ARCODE_H_ */
