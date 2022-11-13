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
// #include "../SHA_256_neon/SHA_256_neon.h"

// #include <arm_neon.h>
// #include <arm_acle.h>

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

static const uint32_t K[] =
{
    0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
    0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
    0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
    0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
    0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
    0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
    0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
    0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
    0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
    0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
    0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
    0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
    0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
    0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
    0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
    0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};


/* Process multiple blocks. The caller is responsible for setting the initial */
/*  state, and the caller is responsible for padding the final block.        */
// int sha256_process_arm(uint32_t state[8], const uint8_t data[], uint32_t length)
// {
//     uint32x4_t STATE0, STATE1, ABEF_SAVE, CDGH_SAVE;
//     uint32x4_t MSG0, MSG1, MSG2, MSG3;
//     uint32x4_t TMP0, TMP1, TMP2;

//     /* Load state */
//     STATE0 = vld1q_u32(&state[0]);
//     STATE1 = vld1q_u32(&state[4]);

//     while (length >= 64)
//     {
//         /* Save state */
//         ABEF_SAVE = STATE0;
//         CDGH_SAVE = STATE1;

//         /* Load message */
//         MSG0 = vld1q_u32((const uint32_t *)(data +  0));
//         MSG1 = vld1q_u32((const uint32_t *)(data + 16));
//         MSG2 = vld1q_u32((const uint32_t *)(data + 32));
//         MSG3 = vld1q_u32((const uint32_t *)(data + 48));

//         /* Reverse for little endian */
//         MSG0 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSG0)));
//         MSG1 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSG1)));
//         MSG2 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSG2)));
//         MSG3 = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(MSG3)));

//         TMP0 = vaddq_u32(MSG0, vld1q_u32(&K[0x00]));

//         /* Rounds 0-3 */
//         MSG0 = vsha256su0q_u32(MSG0, MSG1);
//         TMP2 = STATE0;
//         TMP1 = vaddq_u32(MSG1, vld1q_u32(&K[0x04]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
//         MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);

//         /* Rounds 4-7 */
//         MSG1 = vsha256su0q_u32(MSG1, MSG2);
//         TMP2 = STATE0;
//         TMP0 = vaddq_u32(MSG2, vld1q_u32(&K[0x08]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
//         MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);

//         /* Rounds 8-11 */
//         MSG2 = vsha256su0q_u32(MSG2, MSG3);
//         TMP2 = STATE0;
//         TMP1 = vaddq_u32(MSG3, vld1q_u32(&K[0x0c]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
//         MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);

//         /* Rounds 12-15 */
//         MSG3 = vsha256su0q_u32(MSG3, MSG0);
//         TMP2 = STATE0;
//         TMP0 = vaddq_u32(MSG0, vld1q_u32(&K[0x10]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
//         MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);

//         /* Rounds 16-19 */
//         MSG0 = vsha256su0q_u32(MSG0, MSG1);
//         TMP2 = STATE0;
//         TMP1 = vaddq_u32(MSG1, vld1q_u32(&K[0x14]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
//         MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);

//         /* Rounds 20-23 */
//         MSG1 = vsha256su0q_u32(MSG1, MSG2);
//         TMP2 = STATE0;
//         TMP0 = vaddq_u32(MSG2, vld1q_u32(&K[0x18]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
//         MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);

//         /* Rounds 24-27 */
//         MSG2 = vsha256su0q_u32(MSG2, MSG3);
//         TMP2 = STATE0;
//         TMP1 = vaddq_u32(MSG3, vld1q_u32(&K[0x1c]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
//         MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);

//         /* Rounds 28-31 */
//         MSG3 = vsha256su0q_u32(MSG3, MSG0);
//         TMP2 = STATE0;
//         TMP0 = vaddq_u32(MSG0, vld1q_u32(&K[0x20]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
//         MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);

//         /* Rounds 32-35 */
//         MSG0 = vsha256su0q_u32(MSG0, MSG1);
//         TMP2 = STATE0;
//         TMP1 = vaddq_u32(MSG1, vld1q_u32(&K[0x24]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
//         MSG0 = vsha256su1q_u32(MSG0, MSG2, MSG3);

//         /* Rounds 36-39 */
//         MSG1 = vsha256su0q_u32(MSG1, MSG2);
//         TMP2 = STATE0;
//         TMP0 = vaddq_u32(MSG2, vld1q_u32(&K[0x28]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
//         MSG1 = vsha256su1q_u32(MSG1, MSG3, MSG0);

//         /* Rounds 40-43 */
//         MSG2 = vsha256su0q_u32(MSG2, MSG3);
//         TMP2 = STATE0;
//         TMP1 = vaddq_u32(MSG3, vld1q_u32(&K[0x2c]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);
//         MSG2 = vsha256su1q_u32(MSG2, MSG0, MSG1);

//         /* Rounds 44-47 */
//         MSG3 = vsha256su0q_u32(MSG3, MSG0);
//         TMP2 = STATE0;
//         TMP0 = vaddq_u32(MSG0, vld1q_u32(&K[0x30]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);
//         MSG3 = vsha256su1q_u32(MSG3, MSG1, MSG2);

//         /* Rounds 48-51 */
//         TMP2 = STATE0;
//         TMP1 = vaddq_u32(MSG1, vld1q_u32(&K[0x34]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);

//         /* Rounds 52-55 */
//         TMP2 = STATE0;
//         TMP0 = vaddq_u32(MSG2, vld1q_u32(&K[0x38]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);

//         /* Rounds 56-59 */
//         TMP2 = STATE0;
//         TMP1 = vaddq_u32(MSG3, vld1q_u32(&K[0x3c]));
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP0);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP0);

//         /* Rounds 60-63 */
//         TMP2 = STATE0;
//         STATE0 = vsha256hq_u32(STATE0, STATE1, TMP1);
//         STATE1 = vsha256h2q_u32(STATE1, TMP2, TMP1);

//         /* Combine state */
//         STATE0 = vaddq_u32(STATE0, ABEF_SAVE);
//         STATE1 = vaddq_u32(STATE1, CDGH_SAVE);

//         data += 64;
//         length -= 64;
//     }

//     /* Save state */
//     vst1q_u32(&state[0], STATE0);
//     vst1q_u32(&state[4], STATE1);
//     return 1;
// }


/* data type for chunk */
typedef struct chunk{
    unsigned char *start;
    unsigned int length;
    std::string SHA_signature; 
    uint32_t number; 
    //CHANGE
    uint32_t chunk_num_total;
} chunk_t;

typedef struct chunk_number_data{
    uint32_t chunk_number_data;
    bool unique;
    uint32_t dup_num;
} chunk_number_data_t;

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
        //hash += (input[pos + WIN_SIZE-1-i]) * (pow(PRIME,i+1));
    }
    return hash; 
}

    //For rolling hash 
void cdc_new(unsigned char *buff, chunk_t *chunk, int packet_length, int last_index)
{
    static const double cdc_pow = pow(PRIME,WIN_SIZE+1);
    unsigned char *chunkStart = buff;

    uint64_t hash = hash_func(buff,WIN_SIZE); 

    // if((hash % MODULUS) == TARGET)
    //  {
    //      chunk->length = MIN_CHUNK_SIZE; // minimum chunk size
    //      chunk->start = chunkStart;
    //      chunkStart += MIN_CHUNK_SIZE;
    //      return;
    //  }

    //for (int i = WIN_SIZE +1 ; i < buff_size - WIN_SIZE ; i++)
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
    // use the wolfssl sha library
    // wc_Sha3 sha3_384;
    // unsigned char shaSum[SHA3_384_DIGEST_SIZE];

    // wc_InitSha3_384(&sha3_384, NULL, INVALID_DEVID);
    // wc_Sha3_384_Update(&sha3_384, (const unsigned char*)chunk->start, chunk->length);
    // wc_Sha3_384_Final(&sha3_384, (unsigned char *)shaSum);


    // printf("\n");
    // for(int x=0; x < SHA3_384_DIGEST_SIZE; x++){
    //     printf("%x",shaSum[x]);
    // }
    // printf("\n");

    // return std::string(reinterpret_cast<char*>(shaSum),SHA3_384_DIGEST_SIZE);

        byte shaSum[SHA256_DIGEST_SIZE];
        byte buffer[1024];
        /*fill buffer with data to hash*/

        wc_Sha256 sha;
        wc_InitSha256(&sha);

        wc_Sha256Update(&sha, (const unsigned char*)chunk->start, chunk->length);  /*can be called again
                                                and again*/
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
    //static std::vector<chunk_t> SHA_table;
    static std::unordered_map<std::string, chunk_t> SHA_map;

    static uint32_t num_unique_chunk = 0;

    // for(unsigned int i=0; i<SHA_table.size();i++){
    //  if(SHA_table[i].SHA_signature == SHA_result){
    //      return SHA_table[i].number;
    //  }
    // }
    //CHANGE
    number_of_chunk++;
    chunk->chunk_num_total= number_of_chunk;
    auto find_SHA = SHA_map.find(SHA_result);
    if(find_SHA == SHA_map.end()){
        // add new chunk to the map and return 0
        chunk->SHA_signature = SHA_result;
        chunk->number = num_unique_chunk;
        //SHA_map.insert({SHA_result,num_unique_chunk});
        //SHA_map[SHA_result] = num_unique_chunk;
        SHA_map[SHA_result] = *chunk;
        
        num_unique_chunk++;
        return 0;
    }else{
        // return the number of chunk this chunk is duplicate of
        //CHNAGE
        //chunk->number = SHA_map[SHA_result];
        chunk_t chunk_found = SHA_map[SHA_result];
        if(chunk->length == chunk_found.length){
            return chunk_found.number;
        }else{
            chunk->SHA_signature = SHA_result;
            chunk->number = num_unique_chunk;
            //SHA_map.insert({SHA_result,num_unique_chunk});
            //SHA_map[SHA_result] = num_unique_chunk;
            SHA_map[SHA_result] = *chunk;
            
            num_unique_chunk++;
            return 0;
        }
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
    //std::cout << p << "\t" << table[p] << std::endl;
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
//static int chunk_number1 = 0;
std::vector<chunk_number_data_t> chunk_num_data_vector;
static unsigned int LZW_in_bytes = 0;

void compression_flow(unsigned char *buffer, int length, chunk_t *new_cdc_chunk)
{
    for(int i=0; i<length; i += new_cdc_chunk->length){

        //chunk_number_data_t chunk_data;

        cdc_new(&buffer[i],new_cdc_chunk,length,i);
        //new_cdc_chunk->number++; // same cdc chunk variable is used to store new chunk info temporarily so we can increment the variable directly

        printf("Chunk Start = %p, length = %d\n",new_cdc_chunk->start, new_cdc_chunk->length);
        //std::cout << "Chunk Start = " << static_cast<void *>(new_cdc_chunk->start) << ", length = " << new_cdc_chunk->length << std::endl;
        for(int i = 0; i < new_cdc_chunk->length ; i++){
            printf("%c",new_cdc_chunk->start[i]);
        }
        printf("\n");

        std::string SHA_result = SHA_384(new_cdc_chunk);

        
        // std::cout << "SHA result = " << SHA_result << std::endl;
        //printf("SHA Result = %s", SHA_result);
        //chunk_number1 ++;
        
        uint32_t dup_chunk_num;
        if((dup_chunk_num = dedup(SHA_result,new_cdc_chunk))){
            uint32_t header = 0;
            header |= (dup_chunk_num<<1); // 31 bits specify the number of the chunk to be duplicated
            header |= (0x1); // LSB 1 indicates this is a duplicate chunk

            printf("DUPLICATE CHUNK : %p CHUNK NO : %d\n",new_cdc_chunk->start, dup_chunk_num);
            printf("Header written %d\n",header);
            
            //chunk_data.chunk_number_data = new_cdc_chunk->chunk_num_total;
            //chunk_data.dup_num = dup_chunk_num;


            memcpy(&file[offset], &header, sizeof(header)); // write header to the file
            offset += sizeof(header);

        }else{
            uint32_t header = 0;
            printf("NEW CHUNK : %p chunk no = %d\n",new_cdc_chunk->start, new_cdc_chunk->number);
            // for(int i = 0; i < new_cdc_chunk->length ; i++){
            //     printf("%c",new_cdc_chunk->start[i]);
            // }
            // printf("\n");
            LZW_in_bytes += new_cdc_chunk->length;

            std::vector<int> compressed_data = LZW_encoding(new_cdc_chunk);
            
            uint64_t compressed_size = ceil(13*compressed_data.size() / 8.0);
            header |= ( compressed_size <<1); /* size of the new chunk */
            header &= ~(0x1); /* lsb equals 0 signifies new chunk */
            printf("Header written %x\n",header);
            memcpy(&file[offset], &header, sizeof(header)); /* write header to the output file */
            offset += sizeof(header);
            //chunk_data.chunk_number_data = new_cdc_chunk->chunk_num_total;
            //chunk_data.unique = true;

            // compress the lzw encoded data
            uint64_t compressed_length = compress(compressed_data);

            if(compressed_length != compressed_size){
                std::cout << "Error: lengths not matching, calculated = " << compressed_size << ", return_val = " << std::endl;
                exit(1);
            }

            //memcpy(&file[*offset], &compressed_data, compressed_data.size());  /* write compressed data to output file */
            offset += compressed_length;
        }
        //chunk_num_data_vector.push_back(chunk_data);
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
    //printf("length: %d offset %d\n",length,offset);

    // we are just memcpy'ing here, but you should call your
    // top function here.
    //memcpy(&file[offset], &buffer[HEADER], length);

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
        //printf("length: %d offset %d\n",length,offset);
        //memcpy(&file[offset], &buffer[HEADER], length);
        
        printf("Start of Loop, packet size = %d\r\n",length);

        compression_flow(&buffer[HEADER], length, &new_cdc_chunk);

        writer++;
    }
    //printf("Total chunks - %d\n", chunk_number1);

    // printf("chunk_data - \n");

    // for(int i =0; i< chunk_num_data_vector.size(); i++){
    //  printf("Chunk number - %d \n",chunk_num_data_vector[i].chunk_number_data);
    //  if(chunk_num_data_vector[i].unique != true){
    //      printf("Duplicate number - %d \n", chunk_num_data_vector[i].dup_num);
    //  }
    // }
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
	//SHA_256 neon checking
	// uint8_t message[64];
    // memset(message, 0x00, sizeof(message));
    // message[0] = 0x80;

    // /* initial state */
    // uint32_t state[8] = {
    //     0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    //     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    // };

    // int sha_success = sha256_process_arm(state, message, sizeof(message));

    // const uint8_t b1 = (uint8_t)(state[0] >> 24);
    // const uint8_t b2 = (uint8_t)(state[0] >> 16);
    // const uint8_t b3 = (uint8_t)(state[0] >>  8);
    // const uint8_t b4 = (uint8_t)(state[0] >>  0);
    // const uint8_t b5 = (uint8_t)(state[1] >> 24);
    // const uint8_t b6 = (uint8_t)(state[1] >> 16);
    // const uint8_t b7 = (uint8_t)(state[1] >>  8);
    // const uint8_t b8 = (uint8_t)(state[1] >>  0);

    // /* e3b0c44298fc1c14... */
    // printf("SHA256 hash of empty message: ");
    // printf("%02X%02X%02X%02X%02X%02X%02X%02X...\n",
    //     b1, b2, b3, b4, b5, b6, b7, b8);

    // int success = ((b1 == 0xE3) && (b2 == 0xB0) && (b3 == 0xC4) && (b4 == 0x42) &&
    //                 (b5 == 0x98) && (b6 == 0xFC) && (b7 == 0x1C) && (b8 == 0x14));


	// if (success)
    //     printf("Success!\n");
    // else
    //     printf("Failure!\n");
	//SHA_256 neon checking ends

    free(file);
    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    float input_throughput = (bytes_written * 8 / 1000000.0) / ethernet_latency; // Mb/s
    std::cout << "Input Throughput to Encoder: " << input_throughput << " Mb/s."
            << " (Latency: " << ethernet_latency << "s)." << std::endl;

    

    return 0;
}


