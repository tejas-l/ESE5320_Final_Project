#ifndef _LZW_H
#define _LZW_H

#include "../common/common.h"


uint64_t compress(std::vector<int> &compressed_data);
//std::vector<int> LZW_encoding(chunk_t* chunk);
uint64_t LZW_encoding(chunk_t* chunk);

#endif
