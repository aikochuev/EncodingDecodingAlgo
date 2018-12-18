#ifndef _RLE_H_
#define _RLE_H_

/* traditional RLE encodeing/decoding */
int RleEncodeFile(FILE *inFile, FILE *outFile);
int RleDecodeFile(FILE *inFile, FILE *outFile);

/* variant of packbits RLE encodeing/decoding */
int VPackBitsEncodeFile(FILE *inFile, FILE *outFile);
int VPackBitsDecodeFile(FILE *inFile, FILE *outFile);

#endif  /* ndef _RLE_H_ */
