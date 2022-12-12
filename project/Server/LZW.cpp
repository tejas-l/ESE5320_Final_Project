#include "LZW.h"
#include <unistd.h>

extern int offset;
extern unsigned char* file;

extern volatile int input_packets_done;
extern volatile int num_input_packets;

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
    OCL_CHECK(err, kernel = cl::Kernel(program, "LZW_encoding_HW:{LZW_encoding_HW_1,LZW_encoding_HW_2,LZW_encoding_HW_3}", &err));


    LOG(LOG_DEBUG, "creating vectors\n");

    write_events_vec.reserve(2*NUM_PACKETS);
    execute_events_vec.reserve(2*NUM_PACKETS);
    read_events_vec.reserve(2*NUM_PACKETS);

    size_t in_buf_size = (8192 + 2)*sizeof(unsigned char);
    size_t out_buf_size = 2*KERNEL_OUT_SIZE*sizeof(unsigned char);
    size_t out_len_size = 10*sizeof(unsigned int);
    size_t chunk_lengths_size = MAX_NUM_CHUNKS*sizeof(unsigned int);
    size_t chunk_numbers_size = MAX_NUM_CHUNKS*sizeof(unsigned int);
    size_t chunk_isdups_size = MAX_NUM_CHUNKS*sizeof(unsigned char);

    LOG(LOG_DEBUG, "initializing cl::buffers and enqueue map buffers\n");

    for(int i=0; i<NUM_PACKETS; i++){

        posix_memalign((void**)&to_fpga_buf[i], 4096, in_buf_size);
        posix_memalign((void**)&from_fpga_buf[i], 4096, out_buf_size);
        posix_memalign((void**)&LZW_HW_output_length_ptr[i], 4096, out_len_size);
        posix_memalign((void**)&chunk_lengths_buf_ptr[i], 4096, chunk_lengths_size);
        posix_memalign((void**)&chunk_numbers_buf_ptr[i], 4096, chunk_numbers_size);
        posix_memalign((void**)&chunk_isdups_buf_ptr[i], 4096, chunk_isdups_size);

        OCL_CHECK(err, in_buf[i] = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, in_buf_size, to_fpga_buf[i], &err ));
        OCL_CHECK(err, out_buf[i] = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, out_buf_size, from_fpga_buf[i], &err ));
        OCL_CHECK(err, out_len[i] = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_WRITE_ONLY, out_len_size, LZW_HW_output_length_ptr[i], &err ));
        OCL_CHECK(err,  chunk_lengths_buf[i] = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, chunk_lengths_size, chunk_lengths_buf_ptr[i], &err ));
        OCL_CHECK(err,  chunk_numbers_buf[i] = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, chunk_numbers_size, chunk_numbers_buf_ptr[i], &err ));
        OCL_CHECK(err,  chunk_isdups_buf[i] = cl::Buffer(context, CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY, chunk_isdups_size, chunk_isdups_buf_ptr[i], &err ));

    }
    LZW_HW_output_length_ptr[0][0] = 0xFF;
    //*LZW_HW_output_length_ptr[0] = 3842;
    std::cout << "LZW_HW_output_length_ptr[0] = " << *LZW_HW_output_length_ptr[0] << std::endl;

    LOG(LOG_DEBUG, "initialization done\n");

};


LZW_kernel_call::~LZW_kernel_call()
{
    std::cout << "LZW destructor, unmapping all objects" << std::endl;

    for(int i=0; i<NUM_PACKETS;i++){
        free(to_fpga_buf[i]);
        free(from_fpga_buf[i]);
        free(LZW_HW_output_length_ptr[i]);
        free(chunk_lengths_buf_ptr[i]);
        free(chunk_numbers_buf_ptr[i]);
        free(chunk_isdups_buf_ptr[i]);
    }
    std::cout << "LZW destructor finished" << std::endl;
}

void LZW_kernel_call::LZW_kernel_run(size_t in_buf_size,
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
                        int packet_num){

    cl_int err;
    cl::Event write_event, execute_event, read_event, read_len_event;

    OCL_CHECK(err, err = kernel.setArg(0, in_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(1, chunk_lengths_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(2, chunk_numbers_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(3, chunk_isdups_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(4, num_chunks));
    OCL_CHECK(err, err = kernel.setArg(5, out_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(6, out_len[packet_num]));
    // OCL_CHECK(err, err = kernel.setArg(0, in_buf[packet_num]));
    // OCL_CHECK(err, err = kernel.setArg(1, chunk_lengths_buf[packet_num]));
    // OCL_CHECK(err, err = kernel.setArg(2, num_chunks));
    // OCL_CHECK(err, err = kernel.setArg(3, chunk_isdups_buf[packet_num]));
    // OCL_CHECK(err, err = kernel.setArg(4, out_buf[packet_num]));
    // OCL_CHECK(err, err = kernel.setArg(5, out_len[packet_num]));
    // OCL_CHECK(err, err = kernel.setArg(6, chunk_numbers_buf[packet_num]));

    LOG(LOG_DEBUG, "KERNEL ARGS SET\n");

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({in_buf[packet_num], chunk_lengths_buf[packet_num], chunk_numbers_buf[packet_num], chunk_isdups_buf[packet_num]}, 0 /* 0 means from host*/, NULL, &write_event));
    write_events_vec.push_back(write_event);

    OCL_CHECK(err, err = q.enqueueTask(kernel, &write_events_vec, &execute_event));
    execute_events_vec.push_back(execute_event);

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({out_buf[packet_num],out_len[packet_num]}, CL_MIGRATE_MEM_OBJECT_HOST /* 0 means from host*/, &execute_events_vec, &read_event));
    read_events_vec.push_back(read_event);

}

int packet_num_wait = 0;

int LZW_kernel_call::LZW_wait_on_read_event(void)
{
    std::cout << "LZW_HW_output_length_ptr[0] = " << LZW_HW_output_length_ptr[0][0] << std::endl;
    if(packet_num_wait < read_events_vec.size()){
        LOG(LOG_DEBUG,"CL EVENTS: waiting for packet %d in host\n",packet_num_wait);
        read_events_vec[packet_num_wait].wait();
        packet_num_wait++;
        LOG(LOG_DEBUG,"CL EVENTS: packet %d received in host\n",packet_num_wait);
        return 0;
    }else{
        return 1;
    }
}

void LZW_kernel_call::LZW_q_finish_wait(void)
{
    q.finish();
    LOG(LOG_DEBUG,"Q.FINISH SKIPPED\n");
}

int flag = 0;

uint64_t LZW_encoding_packet_level(packet_t **packet_ring_buf, LZW_kernel_call *lzw_kernel, sem_t *sem_dedup_lzw, sem_t *sem_lzw, volatile int *sem_done)
{
    static int packet_num = 0;
    static int packets_pending = 0;
    static int packets_at_output = 0;
    static int total_packets_enqueued = 0;


    stopwatch lzw_time;
    stopwatch lzw_wait_time;
    int num_sems_received = 0;

    while(1){
        // wait for semaphore released by dedup
        if((input_packets_done == 1) && (total_packets_enqueued == num_input_packets)){
            LOG(LOG_DEBUG,"exiting while 1 loop, packets recved = %d, packets enqueued = %d\n",num_input_packets,total_packets_enqueued);
            break;
        }

        lzw_wait_time.start();
        sem_wait(sem_dedup_lzw);
        lzw_wait_time.stop();
        num_sems_received++;

        lzw_time.start();

        packet_t *new_packet = packet_ring_buf[packet_num];
        packets_pending++;

        LOG(LOG_INFO_1, "semaphore received, starting lzw, packet number: %d\n",packet_num);

        chunk_t *chunklist_ptr = new_packet->chunk_list;
        uint64_t num_chunks = new_packet->num_chunks;
        LOG(LOG_DEBUG,"PACKET_NUM = %d, LZW NUM CHUNK = %d\n",packet_num,num_chunks);
        std::cout << "PACKET NUM = " << packet_num << ", NUM CHUNKS = " << num_chunks << std::endl;

        size_t in_buf_size = ((new_packet->length) + 2)*sizeof(unsigned char);
        size_t out_buf_size = 2*KERNEL_OUT_SIZE*sizeof(unsigned char);
        size_t out_len_size = sizeof(unsigned int);
        size_t chunk_lengths_size = num_chunks*sizeof(unsigned int);
        size_t chunk_numbers_size = num_chunks*sizeof(unsigned int);
        size_t chunk_isdups_size = num_chunks*sizeof(unsigned char);

        memcpy(lzw_kernel->to_fpga_buf[packet_num],(new_packet->buffer - 2),in_buf_size);
        for(int i = 0; i< num_chunks; i++){
            lzw_kernel->chunk_lengths_buf_ptr[packet_num][i] = chunklist_ptr[i].length;
            lzw_kernel->chunk_numbers_buf_ptr[packet_num][i] = chunklist_ptr[i].number;
            lzw_kernel->chunk_isdups_buf_ptr[packet_num][i] = chunklist_ptr[i].is_duplicate;
        }

        if(flag == 0){
            // warmup packet sent for hardware
            lzw_kernel->LZW_kernel_run(  in_buf_size, lzw_kernel->to_fpga_buf[packet_num], 
                                    out_buf_size, lzw_kernel->from_fpga_buf[packet_num], 
                                    chunk_lengths_size, lzw_kernel->chunk_lengths_buf_ptr[packet_num],
                                    chunk_numbers_size, lzw_kernel->chunk_numbers_buf_ptr[packet_num],
                                    chunk_isdups_size, lzw_kernel->chunk_isdups_buf_ptr[packet_num],
                                    out_len_size, num_chunks, packet_num);
            
            flag = 1;
        }

        lzw_kernel->LZW_kernel_run(  in_buf_size, lzw_kernel->to_fpga_buf[packet_num], 
                                    out_buf_size, lzw_kernel->from_fpga_buf[packet_num], 
                                    chunk_lengths_size, lzw_kernel->chunk_lengths_buf_ptr[packet_num],
                                    chunk_numbers_size, lzw_kernel->chunk_numbers_buf_ptr[packet_num],
                                    chunk_isdups_size, lzw_kernel->chunk_isdups_buf_ptr[packet_num],
                                    out_len_size, num_chunks, packet_num);

        std::cout << "while loop LZW_HW_output_length_ptr[0] = " << (lzw_kernel->LZW_HW_output_length_ptr[0][0]) << std::endl;

        total_packets_enqueued++;

        LOG(LOG_DEBUG,"LZW_KERNEL kernel enqueued, total_packets_enqueued = %d\n",total_packets_enqueued);
        LOG(LOG_DEBUG,"LZW_KERNEL total_packets_pending = %d, total_packets_entered_in_pipeline = %d\n",packets_pending,num_input_packets);

        if(packets_pending == NUM_PACKETS){
            LOG(LOG_DEBUG,"LZW_KERNEL Waiting for output packet = %d\n",packets_pending);
            lzw_kernel->LZW_wait_on_read_event();

            memcpy(&file[offset],lzw_kernel->from_fpga_buf[packets_at_output],*(lzw_kernel->LZW_HW_output_length_ptr[packets_at_output]));
            offset += *(lzw_kernel->LZW_HW_output_length_ptr[packets_at_output]);
            packets_at_output++;

            if(packets_at_output == NUM_PACKETS){
                packets_at_output = 0;
            }

            packets_pending--;
            sem_post(sem_lzw);
        }

        packet_num++;
        if(packet_num == NUM_PACKETS){
            packet_num = 0; // ring buffer calculations
        }

        lzw_time.stop();
        LOG(LOG_INFO_1, "releasing semaphore for cdc from lzw\n");
    }

    std::cout << "Removing pending packets from the pipeline" << std::endl;
    std::cout << "packets pending in the pipeline = " << packets_pending << std::endl;

    // stopwatch sync_time;

    // sync_time.start();
    // if(num_input_packets < 10){
    //     std::cout << "LZW WAITING FOR first packet" << std::endl;
    //     /* insert software delay to make sure */
    //     usleep(10000);
    // }
    // sync_time.stop();
    // std::cout << "Sync var = " << sync_time.avg_latency() << std::endl;


    // read out warmup packet
    lzw_kernel->LZW_wait_on_read_event();

    while(packets_pending--){
        LOG(LOG_DEBUG,"LZW_KERNEL Waiting for output packet = %d\n",packets_pending);
        lzw_kernel->LZW_wait_on_read_event();

        for(int m = 0; m< 15; m++){
            printf("%02x",lzw_kernel->from_fpga_buf[packets_at_output][m]);
        }
        printf("\n");

        memcpy(&file[offset],lzw_kernel->from_fpga_buf[packets_at_output],(lzw_kernel->LZW_HW_output_length_ptr[packets_at_output][0]));
        offset += (lzw_kernel->LZW_HW_output_length_ptr[packets_at_output][0]);
        std::cout << "output length = " << (lzw_kernel->LZW_HW_output_length_ptr[packets_at_output][0]) << std::endl;

        packets_at_output++;

        if(packets_at_output == NUM_PACKETS){
            packets_at_output = 0;
        }

        sem_post(sem_lzw);
    }

    LOG(LOG_DEBUG,"LZW KERNEL all pending packets removed waiting for q.finish()\n");

    lzw_kernel->LZW_q_finish_wait();

    LOG(LOG_DEBUG,"kernel q emptied");
    LOG(LOG_DEBUG,"kernel SEMS received = %d\n",num_sems_received);

    sem_wait(sem_dedup_lzw);
    LOG(LOG_DEBUG,"kernel SEMS received = %d, sem_done = %d\n",num_sems_received,*sem_done);
    while ((*sem_done) == 0);
    sem_post(sem_lzw);

    LOG(LOG_DEBUG,"kernel SEMS received = %d\n",num_sems_received);

    //free(LZW_HW_output_length_ptr);
    // free(chunk_lengths);
    // free(chunk_numbers);
    // free(chunk_isdups);

    std::cout << "Average time for LZW Encoding = " << lzw_time.avg_latency() << "ms" << " Total time = " << lzw_time.latency() << "ms" << std::endl;
    std::cout << "lzw wait time = " << lzw_wait_time.avg_latency() << "ms" << " Total wait time = " << lzw_wait_time.latency() << "ms" << std::endl;

    return offset;

}