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
    cl::Buffer          in_buf;
    cl::Buffer          out_buf;
    cl::Buffer          out_len;
    cl::Context         context;

    public:
    LZW_kernel_call(cl::Context &context_1, 
                    cl::Program &program, 
                    cl::CommandQueue &queue);

    void LZW_kernel_run(unsigned int HW_LZW_IN_LEN,
                        size_t in_buf_size,
                        unsigned char* to_fpga,
                        size_t out_buf_size,
                        unsigned char* from_fpga,
                        size_t out_len_size,
                        unsigned int* LZW_HW_output_length_ptr);
};




//uint64_t compress(std::vector<int> &compressed_data);
uint64_t compress(unsigned int *compressed_data, unsigned int compressed_data_len);
//std::vector<int> LZW_encoding(chunk_t* chunk);
uint64_t LZW_encoding(chunk_t* chunk);

#endif
