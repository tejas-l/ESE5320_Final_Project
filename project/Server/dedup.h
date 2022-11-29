#ifndef _DEDUP_H
#define _DEDUP_H

#include <unordered_map>
#include "../common/common.h"


uint32_t dedup(chunk_t *chunk);
void dedup_packet_level(packet_t *new_packet, sem_t *sem_sha_dedup, sem_t *sem_dedup_lzw);

#endif