#ifndef _ENCODER_H_
#define _ENCODER_H_


#include "LZW.h"
#include "cdc.h"
#include "SHA_block.h"
#include "SHA256_NEON.h"
#include "dedup.h"

#include "../common/common.h"
#include "Utilities.h"


// max number of elements we can get from ethernet
#define NUM_ELEMENTS 16384
#define HEADER 2

//void compression_flow(unsigned char *buffer, int length, chunk_t *new_cdc_chunk, LZW_kernel_call &lzw_kernel);
void handle_input(int argc, char* argv[], int* blocksize);


#endif
