#ifndef _DEDUP_H
#define _DEDUP_H

#include <unordered_map>
#include "../common/common.h"


uint32_t dedup(std::string SHA_result,chunk_t *chunk);

#endif