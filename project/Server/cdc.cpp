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

//For rolling hash 
void CDC_packet_level(packet_t *new_packet)
{
    static const double cdc_pow = pow(PRIME,WIN_SIZE+1);

    unsigned char *buff = new_packet->buffer;
    chunk_t *chunklist_ptr = new_packet->chunk_list;
    uint32_t list_index = 0;
    uint32_t packet_length = new_packet->length;
    uint32_t prev_index = 0;

    uint64_t hash = hash_func(buff,MIN_CHUNK_SIZE);
    chunklist_ptr[list_index].start = buff;


    for(uint32_t i = MIN_CHUNK_SIZE; i < packet_length;){

        if((i + MIN_CHUNK_SIZE -1) > packet_length){ // Try moving this outside the for loop
            chunklist_ptr[list_index].length = packet_length - prev_index;//i + 1 - prev_index;
            printf("i1: %d, chunk number: %d, chunk length: %d\n",i,list_index,chunklist_ptr[list_index].length);
            list_index++;
            new_packet->num_chunks = list_index;
            return;
        }

        if((hash % MODULUS) == TARGET)
        {

            chunklist_ptr[list_index].length = i + 1 - prev_index; //Enter the length for nth element in the list //running length
            prev_index = i + 1;
            printf("i2: %d, chunk number: %d, chunk length: %d\n",i,list_index,chunklist_ptr[list_index].length);
            list_index++;
            chunklist_ptr[list_index].start = &buff[i+1]; // Enter the chunk start for n+1 th element
            i += MIN_CHUNK_SIZE;
            hash = hash_func(buff,i);
            continue;            
        }
        i++;
        hash = (hash * PRIME - (buff[i-1])*cdc_pow + (buff[i-1+WIN_SIZE]*PRIME));
        
    }
    list_index++;
    new_packet->num_chunks = list_index;

    return;

}