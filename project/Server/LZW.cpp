#include "LZW.h"


extern int offset;
extern unsigned char* file;


/* defines for compress function */
#define OUT_SIZE_BITS 8
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define KERNEL_IN_SIZE 8*1024
#define KERNEL_OUT_SIZE 8*1024

LZW_kernel_call::LZW_kernel_call(cl::Context &context_1, 
                cl::Program &program, 
                cl::CommandQueue &queue)
{
    cl_int err;

    q = queue;
    context = context_1;
    OCL_CHECK(err, kernel = cl::Kernel(program, "LZW_encoding_HW", &err));

};

void LZW_kernel_call::LZW_kernel_run(unsigned int HW_LZW_IN_LEN,
                    size_t in_buf_size,
                    unsigned char* to_fpga,
                    size_t out_buf_size,
                    unsigned char* from_fpga,
                    size_t out_len_size,
                    unsigned int* LZW_HW_output_length_ptr){
    cl_int err;
    OCL_CHECK(err, in_buf = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, in_buf_size, to_fpga, &err ));
    OCL_CHECK(err, out_buf = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, out_buf_size, from_fpga, &err ));
    OCL_CHECK(err, out_len = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, out_len_size, LZW_HW_output_length_ptr, &err ));
    
    std::vector<cl::Event> write_events_vec;
    std::vector<cl::Event> execute_events_vec, read_events_vec;
    cl::Event write_event, execute_event, read_event;

    OCL_CHECK(err, err = kernel.setArg(0, in_buf));
    OCL_CHECK(err, err = kernel.setArg(1, HW_LZW_IN_LEN));
    OCL_CHECK(err, err = kernel.setArg(2, out_buf));
    OCL_CHECK(err, err = kernel.setArg(3, out_len));


    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({in_buf}, 0 /* 0 means from host*/, &read_events_vec, &write_event));
    
    write_events_vec.push_back(write_event);

    OCL_CHECK(err, err = q.enqueueTask(kernel, &write_events_vec, &execute_event));
    
    execute_events_vec.push_back(execute_event);

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({out_buf, out_len}, CL_MIGRATE_MEM_OBJECT_HOST /* 0 means from host*/, &execute_events_vec, &read_event));

    read_event.wait();
    read_events_vec.push_back(read_event);

    q.finish();

}


uint64_t compress(unsigned int* compressed_data, unsigned int length)
{
    uint64_t Length = 0;
    unsigned char Byte = 0;

    uint8_t rem_inBytes = CODE_LENGTH;
    uint8_t rem_outBytes = OUT_SIZE_BITS;

    for(int i=0; i<length; i++){
        int inData = compressed_data[i];
        rem_inBytes = CODE_LENGTH;

        while(rem_inBytes){

            int read_bits = MIN(rem_inBytes,rem_outBytes);
            Byte = (Byte << read_bits) | ( inData >> (rem_inBytes - read_bits) );

            rem_inBytes -= read_bits;
            rem_outBytes -= read_bits;
            Length += read_bits;
            inData &= ((0x1 << rem_inBytes) - 1);

            if(rem_outBytes == 0){
                file[offset + Length/8 - 1] = Byte;
                Byte = 0;
                rem_outBytes = OUT_SIZE_BITS;
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


uint64_t LZW_encoding(chunk_t* chunk)
{
    cl_int err;
    //std::string binaryFile = argv[1];
    std::string binaryFile = "LZW_encoding_HW.xclbin";
    unsigned fileBufSize;
    std::vector<cl::Device> devices = get_xilinx_devices();
    devices.resize(1);
    cl::Device device = devices[0];
    cl::Context context(device, NULL, NULL, NULL, &err);
    char *fileBuf = read_binary_file(binaryFile, fileBufSize);
    cl::Program::Binaries bins{{fileBuf, fileBufSize}};
    cl::Program program(context, devices, bins, NULL, &err);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
    cl::Kernel krnl_LZW_HW(program, "LZW_encoding_HW", &err);

    //KERNEL CALLS

    unsigned int LZW_HW_output_length = 0;
    size_t in_buf_size = chunk->length*sizeof(unsigned char);
    size_t out_buf_size = KERNEL_OUT_SIZE*sizeof(unsigned int);
    size_t out_len_size = sizeof(unsigned int);

    unsigned char* to_fpga = (unsigned char*)calloc(chunk->length,sizeof(unsigned char));
    unsigned int* from_fpga = (unsigned int*)calloc(KERNEL_OUT_SIZE,sizeof(unsigned int));
    unsigned int* LZW_HW_output_length_ptr = (unsigned int *)calloc(1,sizeof(unsigned int));//&LZW_HW_output_length;

    memcpy(to_fpga,chunk->start,chunk->length);


    cl::Buffer in_buf(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, in_buf_size, to_fpga, &err);
    cl::Buffer out_buf(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, out_buf_size, from_fpga, &err);
    cl::Buffer out_len(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, out_len_size, LZW_HW_output_length_ptr, &err);

    
    std::vector<cl::Event> write_events_vec;
    std::vector<cl::Event> execute_events_vec, read_events_vec;
    cl::Event write_event, execute_event, read_event;

    unsigned int HW_LZW_IN_LEN = chunk->length;


    krnl_LZW_HW.setArg(0, in_buf);
    krnl_LZW_HW.setArg(1, HW_LZW_IN_LEN);
    krnl_LZW_HW.setArg(2, out_buf);
    krnl_LZW_HW.setArg(3, out_len);
    q.enqueueMigrateMemObjects({in_buf}, 0 /* 0 means from host*/, &read_events_vec, &write_event);

    write_events_vec.push_back(write_event);

    q.enqueueTask(krnl_LZW_HW, &write_events_vec, &execute_event);
    execute_events_vec.push_back(execute_event);

    q.enqueueMigrateMemObjects({out_buf, out_len}, CL_MIGRATE_MEM_OBJECT_HOST /* 0 means from host*/, &execute_events_vec, &read_event);
    read_event.wait();
    read_events_vec.push_back(read_event);

    q.finish();


    int header = 0;
    uint64_t compressed_size = ceil(13*(*LZW_HW_output_length_ptr) / 8.0);
    header |= ( compressed_size <<1); /* size of the new chunk */
    header &= ~(0x1); /* lsb equals 0 signifies new chunk */
    LOG(LOG_DEBUG,"Header written %x\n",header);
    memcpy(&file[offset], &header, sizeof(header)); /* write header to the output file */
    offset += sizeof(header);

    return compress(from_fpga,*LZW_HW_output_length_ptr);
}

uint64_t LZW_encoding_packet_level(packet_t *new_packet, LZW_kernel_call *lzw_kernel, sem_t *sem_dedup_lzw, sem_t *sem_lzw)
{
    // wait for semaphore released by dedup
    sem_wait(sem_dedup_lzw);

    chunk_t *chunklist_ptr = new_packet->chunk_list;
    uint64_t num_chunks = new_packet->num_chunks;

    for(uint64_t i = 0; i < num_chunks; i++){
        if(chunklist_ptr[i].is_duplicate){
            uint32_t header = 0;
            header |= (chunklist_ptr[i].number<<1); // 31 bits specify the number of the chunk to be duplicated
            header |= (0x1); // LSB 1 indicates this is a duplicate chunk

            //LOG(LOG_INFO_2,"DUPLICATE CHUNK : %p CHUNK NO : %d\n",new_chunk->start, dup_chunk_num);
            //LOG(LOG_INFO_2,"Header written %d\n",header);

            memcpy(&file[offset], &header, sizeof(header)); // write header to the file

            // printf("Header written to file - \n");
            LOG(LOG_INFO_1,"Header written %d\n",header);

            offset += sizeof(header);

        }else{
            unsigned int LZW_HW_output_length = 0;
            size_t in_buf_size = chunklist_ptr[i].length*sizeof(unsigned char);
            size_t out_buf_size = KERNEL_OUT_SIZE*sizeof(unsigned char);
            size_t out_len_size = sizeof(unsigned int);

            unsigned int* LZW_HW_output_length_ptr = (unsigned int *)calloc(1,sizeof(unsigned int));
            unsigned int HW_LZW_IN_LEN = chunklist_ptr[i].length;

            lzw_kernel->LZW_kernel_run(HW_LZW_IN_LEN, in_buf_size, chunklist_ptr[i].start, out_buf_size, &file[offset], out_len_size, LZW_HW_output_length_ptr);

            offset += *LZW_HW_output_length_ptr;
            free(LZW_HW_output_length_ptr);

        }
    }

    // release semaphore for lzw kernel completion
    sem_post(sem_lzw);

    return offset;

}