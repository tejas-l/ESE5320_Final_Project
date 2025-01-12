#ifndef _COMMON_H
#define _COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "../Server/server.h"
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <semaphore.h>
//#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "stopwatch.h"
#include <math.h>
#include <vector>
#include <bits/stdc++.h>
#include <arm_neon.h>
#include "logger.h"




#define NUM_PACKETS 64
#define pipe_depth 4
#define DONE_BIT_L (1 << 7)
#define DONE_BIT_H (1 << 15)
#define WIN_SIZE 16
#define MAX_CHUNK_SIZE 8*1024 // 8KB max chunk size
#define MIN_CHUNK_SIZE 16 // 16 bytes min chunk size
#define MAX_PACKET_SIZE 8*1024
#define MAX_NUM_CHUNKS MAX_PACKET_SIZE/MIN_CHUNK_SIZE


//#define TARGET 0x10
#define TARGET 0
#define PRIME 3
#define MODULUS 256
#define CODE_LENGTH (13) // = log2(MAX_CHUNK_SIZE)

/* data type for chunk */
typedef struct chunk{
    unsigned char *start;
    unsigned int length;
    std::string SHA_signature;
    uint32_t number; 
    //CHANGE
    uint32_t chunk_num_total;
    uint8_t is_duplicate;
} chunk_t;


/* data type for passing packet with chunk list */
typedef struct packet{
    chunk_t chunk_list[(MAX_PACKET_SIZE / MIN_CHUNK_SIZE) + 5];
    unsigned char *buffer;
    int length;
    uint64_t num_chunks;
} packet_t;

#endif