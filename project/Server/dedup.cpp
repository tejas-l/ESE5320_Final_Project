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
    static std::unordered_map<std::string, int> SHA_map;
    static uint32_t num_unique_chunk = 0;

    //chunk_t* chunkList = new_packet->chunk_list;
    printf("Hello from dedup packet level\n");
    printf("Number of chunks received to dedup = %ld\n",new_packet->num_chunks);
    uint64_t num_chunks_dedup = new_packet->num_chunks;
    for(uint64_t i = 0; i < num_chunks_dedup; i++){
        if(i == num_chunks_dedup){
        break;
        }
        auto find_SHA = SHA_map.find(new_packet->chunk_list[i].SHA_signature);
        if(find_SHA == SHA_map.end()){
            // add new chunk to the map and return 0
            //chunk->SHA_signature = SHA_result;
            new_packet->chunk_list[i].number = num_unique_chunk;
            SHA_map[new_packet->chunk_list[i].SHA_signature] = new_packet->chunk_list[i].number;
            
            num_unique_chunk++;
            new_packet->chunk_list[i].is_duplicate = 0;

            printf("Chunk number with index %ld is unique\n",i);
        }else{
            // return the number of chunk this chunk is duplicate of
            printf("Chunk number with index %ld is duplicate\n",i);
            new_packet->chunk_list[i].is_duplicate = 1;
            new_packet->chunk_list[i].number = SHA_map[new_packet->chunk_list[i].SHA_signature];
        }




    }
    return;
}