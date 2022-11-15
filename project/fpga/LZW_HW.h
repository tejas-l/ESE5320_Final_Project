#ifndef _LZW_H
#define _LZW_H

#include <stdint.h>
#include <stdio.h>
// uint64_t compress(std::vector<int> &compressed_data);
//std::vector<int> LZW_encoding(chunk_t* chunk);
void LZW_encoding_HW(unsigned char* data_in, unsigned int len, unsigned int* data_out); 

#endif
