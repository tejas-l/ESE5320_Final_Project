#ifndef _LZW_HW_STREAMING_H
#define _LZW_HW_STREAMING_H

#include <iostream>
#include <ctime>
#include <utility>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <math.h>
#include <hls_stream.h>

void LZW_encoding_HW(unsigned char* data_in, unsigned int len, unsigned char* output, unsigned int* Output_length );

#endif