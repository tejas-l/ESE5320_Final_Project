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

#define NUM_PACKETS 8
#define pipe_depth 4
#define DONE_BIT_L (1 << 7)
#define DONE_BIT_H (1 << 15)
#define WIN_SIZE 16
#define MAX_CHUNK_SIZE 8*1024 // 8KB max chunk size
#define MIN_CHUNK_SIZE 16 // 16 bytes min chunk size
#define TARGET 0
#define PRIME 3
#define MODULUS 256

int offset = 0;
unsigned char* file;

/* data type for chunk */
typedef struct chunk{
	unsigned char *start;
	unsigned int length;
	uint64_t SHA_signature; 
	uint32_t number; 
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
uint64_t hash_func(unsigned char *input, unsigned int pos)
{
	// put your hash function implementation here
	uint64_t hash =0; 
	for ( int i = 0 ; i<WIN_SIZE ; i++)
	{
		hash += (input[pos + WIN_SIZE-1-i]) * (pow(PRIME,i+1)); 
	}
	return hash; 
}

	//For rolling hash 
void cdc_new(unsigned char *buff, chunk_t *chunk)
{
	static unsigned char *chunkStart = buff;

	uint64_t hash = hash_func(buff,WIN_SIZE); 

	if((hash % MODULUS) == TARGET)
		{
			chunk->length = MIN_CHUNK_SIZE; // minimum chunk size
		}

	//for (int i = WIN_SIZE +1 ; i < buff_size - WIN_SIZE ; i++)
	for (int i = MIN_CHUNK_SIZE +1 ; i < MAX_CHUNK_SIZE - MIN_CHUNK_SIZE ; i++)
	{
		hash = (hash * PRIME - (buff[i-1])*pow(PRIME,WIN_SIZE+1) + (buff[i-1+WIN_SIZE]*PRIME)); 

		if((hash % MODULUS) == TARGET)
		{
			chunk->length = i;
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

uint64_t SHA_dummy(chunk_t *chunk)
{
	// put your hash function implementation here
	uint64_t hash = 0x2890032189F32 ; 
	
	for ( unsigned int i = 0 ; i<WIN_SIZE ; i++)
	{
		hash += (chunk->start[WIN_SIZE + WIN_SIZE-1-i]) * (pow(PRIME,i+1)); 
	}

	for(unsigned int i=WIN_SIZE; i<(chunk->length-WIN_SIZE); i++){
		hash = (hash * PRIME - (chunk->start[i-1])*pow(PRIME,WIN_SIZE+1) + (chunk->start[i-1+WIN_SIZE]*PRIME));
	}
	return hash;
}

int dedup(uint64_t SHA_result,chunk_t chunk)
{
	static std::vector<chunk_t> SHA_table;

	for(unsigned int i=0; i<SHA_table.size();i++){
		if(SHA_table[i].SHA_signature == SHA_result){
			return SHA_table[i].number;
		}
	}
	chunk.SHA_signature = SHA_result;
	SHA_table.push_back(chunk);

	return 0;
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
    std::cout << "String\tOutput_Code\tAddition\n";
    for (unsigned int i = 0; i < chunk->length; i++) {
        if (i != chunk->length - 1)
            c += chunk->start[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        }
        else {
            std::cout << p << "\t" << table[p] << "\t\t"
                 << p + c << "\t" << code << std::endl;
            output_code.push_back(table[p]);
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
    }
    std::cout << p << "\t" << table[p] << std::endl;
    output_code.push_back(table[p]);
    return output_code;
}

int main(int argc, char* argv[]) {
	stopwatch ethernet_timer;
	unsigned char* input[NUM_PACKETS];
	int writer = 0;
	int done = 0;
	int length = 0;
	int count = 0;
	ESE532_Server server;
	chunk_t cdc_chunk; 
	int num_chunks = 0; 


	// default is 2k
	int blocksize = BLOCKSIZE;

	// set blocksize if decalred through command line
	handle_input(argc, argv, &blocksize);

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

	writer = pipe_depth;
	server.get_packet(input[writer]);
	count++;

	// get packet
	unsigned char* buffer = input[writer];

	// decode
	done = buffer[1] & DONE_BIT_L;
	length = buffer[0] | (buffer[1] << 8);
	length &= ~DONE_BIT_H;
	// printing takes time so be weary of transfer rate
	//printf("length: %d offset %d\n",length,offset);

	// we are just memcpy'ing here, but you should call your
	// top function here.
	memcpy(&file[offset], &buffer[HEADER], length);

	offset += length;
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

		for(int i=0; i<length; i += cdc_chunk.length){

			cdc_new(&buffer[HEADER + i],&cdc_chunk);
			num_chunks++;
			cdc_chunk.number = num_chunks;

			printf("Chunk Start = %p, length = %d\n",cdc_chunk.start, cdc_chunk.length);

			uint64_t SHA_result = SHA_dummy(&cdc_chunk);

			printf("SHA result = %ld, Chunk Number =%d\n", SHA_result, cdc_chunk.number);
			
			int chunk_num;
			if((chunk_num = dedup(SHA_result,cdc_chunk))){
				printf("DUPLICATE CHUNK : %p CHUNK NO : %d\n",cdc_chunk.start, chunk_num);

			}else{
				printf("NEW CHUNK : %p\n",cdc_chunk.start);
				// LZW_encoding(&cdc_chunk);
			}
		}

		offset += length;
		writer++;
	}

	// write file to root and you can use diff tool on board
	FILE *outfd = fopen("output_cpu.bin", "wb");
	int bytes_written = fwrite(&file[0], 1, offset, outfd);
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

