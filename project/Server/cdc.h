#ifndef _CDC_H
#define _CDC_H

#include "../common/common.h"

void CDC(unsigned char *buff, chunk_t *chunk, int packet_length, int last_index); 
void CDC_packet_level(packet_t **packet_ring_buf, sem_t *sem_cdc, sem_t *sem_cdc_sha, volatile int *sem_done);
#endif