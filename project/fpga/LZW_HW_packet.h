#ifndef _LZW_HW_PACKET_H
#define _LZW_HW_PACKET_H

#include <iostream>
#include <ctime>
#include <utility>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <math.h>
#include <hls_stream.h>


void LZW_encoding_HW(unsigned char* data_in, unsigned int* chunk_lengths, unsigned int* chunk_numbers,
                     unsigned char* chunk_isdups, uint64_t num_chunks,
                     unsigned char* output, unsigned int* Output_length);

#endif

