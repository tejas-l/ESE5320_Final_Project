#include "dedup.h"

extern int offset;
extern unsigned char* file;

uint32_t dedup(std::string SHA_result,chunk_t *chunk)
{
    static std::unordered_map<std::string, int> SHA_map;

    static uint32_t num_unique_chunk = 0;

    auto find_SHA = SHA_map.find(SHA_result);
    if(find_SHA == SHA_map.end()){
        // add new chunk to the map and return 0
        chunk->SHA_signature = SHA_result;
        chunk->number = num_unique_chunk;
        SHA_map[SHA_result] = chunk->number;
        
        num_unique_chunk++;
        return 0;
    }else{
        // return the number of chunk this chunk is duplicate of
        return SHA_map[SHA_result];
    }
}