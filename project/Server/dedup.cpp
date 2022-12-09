#include "dedup.h"

extern int offset;
extern unsigned char* file;

uint32_t dedup(chunk_t *chunk)
{
    static std::unordered_map<std::string, int> SHA_map;

    static uint32_t num_unique_chunk = 0;

    auto find_SHA = SHA_map.find(chunk->SHA_signature);
    if(find_SHA == SHA_map.end()){
        // add new chunk to the map and return 0
        chunk->number = num_unique_chunk;
        SHA_map[chunk->SHA_signature] = chunk->number;
        
        num_unique_chunk++;
        return 0;
    }else{
        // return the number of chunk this chunk is duplicate of
        return SHA_map[chunk->SHA_signature];
    }
}

void dedup_packet_level(packet_t **packet_ring_buf, sem_t *sem_sha_dedup, sem_t *sem_dedup_lzw, volatile int *sem_done)
{
    static std::unordered_map<std::string, uint32_t> SHA_map;
    static uint32_t num_unique_chunk = 0;
    static int packet_num = 0;

    while(1){
        // wait for semaphore
        sem_wait(sem_sha_dedup);

        if(*sem_done == 1){
            LOG(LOG_DEBUG,"EXITING DEDUP THREAD\n");
            sem_post(sem_dedup_lzw);
            return;
        }

        packet_t *new_packet = packet_ring_buf[packet_num];
        packet_num++;
        if(packet_num == NUM_PACKETS){
            packet_num = 0; // ring buffer calculations
        }

        LOG(LOG_INFO_1, "semaphore received, starting dedup, packet number: %d\n",packet_num);

        chunk_t * const chunkList = new_packet->chunk_list;
        const uint64_t num_chunks_dedup = new_packet->num_chunks;

        for(uint64_t i = 0; i < num_chunks_dedup; i++){

            if(SHA_map.find(chunkList[i].SHA_signature) == SHA_map.end()){
                // add new chunk to the map and return 0
                chunkList[i].number = num_unique_chunk;
                SHA_map[chunkList[i].SHA_signature] = num_unique_chunk;

                num_unique_chunk++;
                chunkList[i].is_duplicate = 0;

            }else{
                chunkList[i].is_duplicate = 1;
                chunkList[i].number = SHA_map[chunkList[i].SHA_signature];
            }
        }

        // release semaphore
        sem_post(sem_dedup_lzw);

        LOG(LOG_INFO_1, "releasing semaphore for lzw from dedup\n");
    }

    return;
}