#include "encoder.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "server.h"
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "stopwatch.h"
#include <math.h>
#include <vector>
#include <bits/stdc++.h> 
#include <unordered_map>

// includes for wolfssl
#include "wolfssl/options.h"
#include <wolfssl/wolfcrypt/sha256.h>

#define NUM_PACKETS 8
#define pipe_depth 4
#define DONE_BIT_L (1 << 7)
#define DONE_BIT_H (1 << 15)
#define WIN_SIZE 16
#define MAX_CHUNK_SIZE 8*1024 // 8KB max chunk size
#define MIN_CHUNK_SIZE 16 // 16 bytes min chunk size

//#define TARGET 0x10
#define TARGET 0
#define PRIME 3
#define MODULUS 256
#define CODE_LENGTH (13) // = log2(MAX_CHUNK_SIZE)

int offset = 0;
unsigned char* file;

#include <arm_neon.h>

/* data type for chunk */
typedef struct chunk{
    unsigned char *start;
    unsigned int length;
    std::string SHA_signature; 
    uint32_t number; 
    //CHANGE
    uint32_t chunk_num_total;
} chunk_t;

void handle_input(int argc, char* argv[], int* blocksize) {
    int x;
    extern char *optarg;

    while ((x = getopt(argc, argv, ":b:")) != -1) {
        switch (x) {
        case 'b':
            *blocksize = atoi(optarg);
            printf("blocksize is set to %d optarg\n", *blocksize);
            break;
        case ':':
            printf("-%c without parameter\n", optopt);
            break;
        }
    }
}

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
void cdc_new(unsigned char *buff, chunk_t *chunk, int packet_length, int last_index)
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

std::string SHA_384(chunk_t *chunk)
{
        byte shaSum[SHA256_DIGEST_SIZE];
        byte buffer[1024];
        /*fill buffer with data to hash*/

        wc_Sha256 sha;
        wc_InitSha256(&sha);

        wc_Sha256Update(&sha, (const unsigned char*)chunk->start, chunk->length);  /*can be called again and again*/
        wc_Sha256Final(&sha, shaSum);

        printf("\n");
        for(int x=0; x < SHA256_DIGEST_SIZE; x++){
            printf("%02x",shaSum[x]);
        }
        printf("\n");

        return std::string(reinterpret_cast<char*>(shaSum),SHA256_DIGEST_SIZE);

}

static int number_of_chunk=0;
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

        
        // if(chunk->length == chunk_found.length){
        //     return chunk_found.number;
        // }else{
        //     chunk->SHA_signature = SHA_result;
        //     chunk->number = num_unique_chunk;
        //     SHA_map[SHA_result] = *chunk;
            
        //     num_unique_chunk++;
        //     return 0;
        // }
    }
}

std::vector<int> LZW_encoding(chunk_t* chunk)
{
    std::cout << "Encoding\n";
    std::unordered_map<std::string, int> table;
    for (int i = 0; i <= 255; i++) {
        std::string ch = "";
        ch += char(i);
        table[ch] = i;
    }
 
    std::string p = "", c = "";
    p += chunk->start[0];
    int code = 256;
    std::vector<int> output_code;
    //std::cout << "String\tOutput_Code\tAddition\n";
    for (unsigned int i = 0; i < chunk->length; i++) {
        if (i != chunk->length - 1)
            c += chunk->start[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        }
        else {
            //std::cout << p << "\t" << table[p] << "\t\t"
            //     << p + c << "\t" << code << std::endl;
            output_code.push_back(table[p]);
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
    }
    output_code.push_back(table[p]);
    return output_code;
}

uint64_t compress(std::vector<int> &compressed_data)
{
    uint64_t Length = 0;
    unsigned char Byte = 0;

    for(int i=0; i< compressed_data.size(); i++){
        int data = compressed_data[i];

        for(int j=0; j<CODE_LENGTH; j++){
            Byte = (Byte << 1) | ((data >> (CODE_LENGTH - 1 - j)) & 1);

            if(++Length % 8 == 0){
                file[offset + Length/8 - 1] = Byte; // write data to temp array
                Byte = 0;
            }
        }
    }

    if(Length % 8 > 0){
        Byte = Byte << (8 - (Length%8));
        Length += (8 - (Length%8));
        file[offset + Length/8 - 1] = Byte;
    }

    return Length/8; // return number of bytes to be written to output file
}
static unsigned int LZW_in_bytes = 0;

void compression_flow(unsigned char *buffer, int length, chunk_t *new_cdc_chunk)
{
    for(int i=0; i<length; i += new_cdc_chunk->length){

        cdc_new(&buffer[i],new_cdc_chunk,length,i);
    
        printf("Chunk Start = %p, length = %d\n",new_cdc_chunk->start, new_cdc_chunk->length);

        std::string SHA_result = SHA_384(new_cdc_chunk);
        
        uint32_t dup_chunk_num;
        if((dup_chunk_num = dedup(SHA_result,new_cdc_chunk))){
            uint32_t header = 0;
            header |= (dup_chunk_num<<1); // 31 bits specify the number of the chunk to be duplicated
            header |= (0x1); // LSB 1 indicates this is a duplicate chunk

            printf("DUPLICATE CHUNK : %p CHUNK NO : %d\n",new_cdc_chunk->start, dup_chunk_num);
            printf("Header written %d\n",header);

            memcpy(&file[offset], &header, sizeof(header)); // write header to the file
            offset += sizeof(header);

        }else{
            uint32_t header = 0;
            printf("NEW CHUNK : %p chunk no = %d\n",new_cdc_chunk->start, new_cdc_chunk->number);
           
            LZW_in_bytes += new_cdc_chunk->length;

            std::vector<int> compressed_data = LZW_encoding(new_cdc_chunk);
            
            uint64_t compressed_size = ceil(13*compressed_data.size() / 8.0);
            header |= ( compressed_size <<1); /* size of the new chunk */
            header &= ~(0x1); /* lsb equals 0 signifies new chunk */
            printf("Header written %x\n",header);
            memcpy(&file[offset], &header, sizeof(header)); /* write header to the output file */
            offset += sizeof(header);

            // compress the lzw encoded data
            uint64_t compressed_length = compress(compressed_data);

            if(compressed_length != compressed_size){
                std::cout << "Error: lengths not matching, calculated = " << compressed_size << ", return_val = " << std::endl;
                exit(1);
            }
            offset += compressed_length;
        }
    }
}

int main(int argc, char* argv[]) {
    stopwatch ethernet_timer;
    unsigned char* input[NUM_PACKETS];
    int writer = 0;
    int done = 0;
    int length = 0;
    int count = 0;
    ESE532_Server server;
    chunk_t new_cdc_chunk;
    new_cdc_chunk.number = 0;

    int input_bytes=0;

    // default is 2k
    int blocksize = BLOCKSIZE;

    // set blocksize if decalred through command line
    handle_input(argc, argv, &blocksize);

    // check filename through argument
    const char *filename = "output_cpu.bin";
    if(argc > 1){
        if(*argv[1] == '-'){
            if(argc == 4){
                filename = argv[3]; // ./encoder -b 1024 filename
            }
        }else{
            filename = argv[1]; // ./encoder filename
        }
    }

    file = (unsigned char*) malloc(sizeof(unsigned char) * 70000000);
    if (file == NULL) {
        printf("help\n");
    }

    for (int i = 0; i < NUM_PACKETS; i++) {
        input[i] = (unsigned char*) malloc(
                sizeof(unsigned char) * (NUM_ELEMENTS + HEADER));
        if (input[i] == NULL) {
            std::cout << "aborting " << std::endl;
            return 1;
        }
    }

    server.setup_server(blocksize);

    int file_size = 14247; // 14247 bytes per second
    stopwatch total_time;

    total_time.start();

    writer = pipe_depth;
    server.get_packet(input[writer]);
    count++;

    // get packet
    unsigned char* buffer = input[writer];

    // decode
    done = buffer[1] & DONE_BIT_L;
    length = buffer[0] | (buffer[1] << 8);
    length &= ~DONE_BIT_H;
    std::cout << " packet length " << length << std::endl;

    // printing takes time so be weary of transfer rate

    compression_flow(&buffer[HEADER], length, &new_cdc_chunk);

    //offset += length;
    writer++;

    //last message
    while (!done) {
        // reset ring buffer
        if (writer == NUM_PACKETS) {
            writer = 0;
        }

        ethernet_timer.start();
        server.get_packet(input[writer]);
        ethernet_timer.stop();

        count++;

        // get packet
        unsigned char* buffer = input[writer];

        // decode
        done = buffer[1] & DONE_BIT_L;
        length = buffer[0] | (buffer[1] << 8);
        length &= ~DONE_BIT_H;

        printf("Start of Loop, packet size = %d\r\n",length);

        compression_flow(&buffer[HEADER], length, &new_cdc_chunk);

        writer++;
    }
    
    printf("Number of bytes going into LZW - %d \n",LZW_in_bytes);
    // write file to root and you can use diff tool on board
    FILE *outfd = fopen(filename, "wb");
    int bytes_written = fwrite(&file[0], 1, offset, outfd);

    total_time.stop();
    std::cout << "Total runtime = " << total_time.avg_latency() << "ns" << std::endl;
    std::cout << "Throughput = " << (bytes_written*8/1000000.0)/(total_time.avg_latency()/1000.0) << " Mb/s" << std::endl;

    printf("write file with %d\n", bytes_written);
    fclose(outfd);

    for (int i = 0; i < NUM_PACKETS; i++) {
        free(input[i]);
    }
	
    free(file);
    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    float input_throughput = (bytes_written * 8 / 1000000.0) / ethernet_latency; // Mb/s
    std::cout << "Input Throughput to Encoder: " << input_throughput << " Mb/s."
            << " (Latency: " << ethernet_latency << "s)." << std::endl;

    return 0;
}


