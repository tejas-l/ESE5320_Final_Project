#ifndef _LZW_H
#define _LZW_H

#include "../common/common.h"
#include "Utilities.h"

#define KERNEL_IN_SIZE 8*1024
#define KERNEL_OUT_SIZE 8*1024


class LZW_kernel_call
{
    cl::Kernel          kernel;
    cl::CommandQueue    q;
    cl::Context         context;
    cl::Buffer          in_buf[NUM_PACKETS];
    cl::Buffer          out_buf[NUM_PACKETS];
    cl::Buffer          out_len[NUM_PACKETS];
    cl::Buffer          chunk_lengths_buf[NUM_PACKETS];
    cl::Buffer          chunk_numbers_buf[NUM_PACKETS];
    cl::Buffer          chunk_isdups_buf[NUM_PACKETS];


    std::vector<cl::Event> write_events_vec;
    std::vector<cl::Event> execute_events_vec;
    std::vector<cl::Event> read_events_vec;
    std::vector<cl::Event> read_len_events_vec;

    public:
    unsigned char * to_fpga_buf[NUM_PACKETS];
    unsigned char * from_fpga_buf[NUM_PACKETS];
    volatile unsigned int * LZW_HW_output_length_ptr[NUM_PACKETS];
    unsigned int * chunk_lengths_buf_ptr[NUM_PACKETS];
    unsigned int* chunk_numbers_buf_ptr[NUM_PACKETS];
    unsigned char* chunk_isdups_buf_ptr[NUM_PACKETS];

    LZW_kernel_call(cl::Context &context_1, 
                    cl::Program &program, 
                    cl::CommandQueue &queue);

    ~LZW_kernel_call();

    void LZW_kernel_run(size_t in_buf_size,
                        unsigned char* to_fpga,

                        size_t out_buf_size,
                        unsigned char* from_fpga,

                        size_t chunk_lengths_size,
                        unsigned int* chunk_lengths,

                        size_t chunk_numbers_size,
                        unsigned int* chunk_numbers,
                        
                        size_t chunk_isdups_size,
                        unsigned char* chunk_isdups,

                        size_t out_len_size,
                        
                        uint64_t num_chunks,
                        int packet_num);
    
    int LZW_wait_on_read_event(void);

    void LZW_q_finish_wait(void);
};


uint64_t compress(unsigned int *compressed_data, unsigned int compressed_data_len);
uint64_t LZW_encoding(chunk_t* chunk);
uint64_t LZW_encoding_packet_level(packet_t **packet_ring_buf, LZW_kernel_call *lzw_kernel, sem_t *sem_dedup_lzw, sem_t *sem_lzw, volatile int *sem_done);
#endif
