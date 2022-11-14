#include "cdc.h"


extern int offset;
extern unsigned char* file;

// CDC
// For calculating the first hash
// We pre-calculate the power values to save computation time
static const double pow_table[WIN_SIZE] = {3.0, 9.0, 27.0, 81.0, 243.0, 729.0, 2187.0, 6561.0,
                             19683.0, 59049.0, 177147.0, 531441.0, 1594323.0, 4782969.0, 14348907.0, 43046721.0};

uint64_t hash_func(unsigned char *input, unsigned int pos)
{
    // put your hash function implementation here
    uint64_t hash =0; 
    for ( int i = 0 ; i<WIN_SIZE ; i++)
    {
        hash += (input[pos + WIN_SIZE-1-i]) * (pow_table[i]);
    }
    return hash; 
}

//For rolling hash 
void CDC(unsigned char *buff, chunk_t *chunk, int packet_length, int last_index)
{
    static const double cdc_pow = pow(PRIME,WIN_SIZE+1);
    unsigned char *chunkStart = buff;

    uint64_t hash = hash_func(buff,WIN_SIZE); 

    for (int i = WIN_SIZE +1 ; i < MAX_CHUNK_SIZE - WIN_SIZE ; i++)
    {
        hash = (hash * PRIME - (buff[i-1])*cdc_pow + (buff[i-1+WIN_SIZE]*PRIME));

        if((last_index + i + WIN_SIZE -1) > packet_length){
            chunk->length = packet_length - last_index;
            chunk->start = chunkStart;
            chunkStart += i; // change the chunk start address to next start address;
            return;
        }

        if((hash % MODULUS) == TARGET)
        {
            chunk->length = i +1 ;
            chunk->start = chunkStart;
            chunkStart += i; // change the chunk start address to next start address;
            return;
        }

    }
    chunk->length = MAX_CHUNK_SIZE;
    chunk->start = chunkStart;
    chunkStart += MAX_CHUNK_SIZE; // change the chunk start address to next start address;
    return;

}