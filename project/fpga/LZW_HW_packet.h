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

/* data type for chunk */
typedef struct chunk{
    unsigned char *start;
    unsigned int length;
    //std::string SHA_signature;
    uint32_t number; 
    //CHANGE
    uint32_t chunk_num_total;
    uint8_t is_duplicate;
} chunk_t;


/* data type for passing packet with chunk list */
typedef struct packet{
    chunk_t chunk_list[(MAX_PACKET_SIZE / MIN_CHUNK_SIZE) + 5];
    unsigned char *buffer;
    int length;
    uint64_t num_chunks;
} packet_t;

void LZW_encoding_HW(unsigned char* data_in, unsigned int* chunk_lengths, unsigned int* chunk_numbers,
                     unsigned char* chunk_isdups, uint64_t num_chunks,
                     unsigned char* output, unsigned int* Output_length);

#endif

