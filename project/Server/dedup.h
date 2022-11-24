#ifndef _DEDUP_H
#define _DEDUP_H

#include <unordered_map>
#include "../common/common.h"


uint32_t dedup(chunk_t *chunk);
void dedup_packet_level(packet_t *new_packet);

#endif