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
        //chunk->SHA_signature = SHA_result;
        chunk->number = num_unique_chunk;
        SHA_map[chunk->SHA_signature] = chunk->number;
        
        num_unique_chunk++;
        return 0;
    }else{
        // return the number of chunk this chunk is duplicate of
        return SHA_map[chunk->SHA_signature];
    }
}

void dedup_packet_level(packet_t *new_packet)
{
    static std::unordered_map<std::string, uint32_t> SHA_map;
    static uint32_t num_unique_chunk = 0;

    chunk_t * const chunkList = new_packet->chunk_list;
    uint64_t num_chunks_dedup = new_packet->num_chunks;

    for(uint64_t i = 0; i < num_chunks_dedup; i++){

        //auto find_SHA = SHA_map.find(chunkList[i].SHA_signature);
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
    return;
}