#include "LZW.h"


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
    OCL_CHECK(err, kernel = cl::Kernel(program, "LZW_encoding_HW", &err));

    // initialize var to 0
    num_events = 0;

    LOG(LOG_DEBUG, "creating vectors\n");

    write_events_vec.reserve(2*NUM_PACKETS);
    execute_events_vec.reserve(2*NUM_PACKETS);
    read_events_vec.reserve(2*NUM_PACKETS);

    size_t in_buf_size = (8192 + 2)*sizeof(unsigned char);
    size_t out_buf_size = 2*KERNEL_OUT_SIZE*sizeof(unsigned char);
    size_t out_len_size = sizeof(unsigned int);
    size_t chunk_lengths_size = MAX_NUM_CHUNKS*sizeof(unsigned int);
    size_t chunk_numbers_size = MAX_NUM_CHUNKS*sizeof(unsigned int);
    size_t chunk_isdups_size = MAX_NUM_CHUNKS*sizeof(unsigned char);

    LOG(LOG_DEBUG, "initializing cl::buffers and enqueue map buffers\n");

    for(int i=0; i<NUM_PACKETS; i++){
        OCL_CHECK(err, in_buf[i] = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, in_buf_size, NULL, &err ));
        OCL_CHECK(err, out_buf[i] = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_WRITE_ONLY, out_buf_size, NULL, &err ));
        OCL_CHECK(err, out_len[i] = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_WRITE_ONLY, out_len_size, NULL, &err ));
        OCL_CHECK(err,  chunk_lengths_buf[i] = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, chunk_lengths_size, NULL, &err ));
        OCL_CHECK(err,  chunk_numbers_buf[i] = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, chunk_numbers_size, NULL, &err ));
        OCL_CHECK(err,  chunk_isdups_buf[i] = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, chunk_isdups_size, NULL, &err ));

        to_fpga_buf[i] = (unsigned char *)q.enqueueMapBuffer(in_buf[i], CL_TRUE, CL_MAP_WRITE, 0, in_buf_size);
        from_fpga_buf[i] = (unsigned char *)q.enqueueMapBuffer(out_buf[i], CL_TRUE, CL_MAP_READ, 0, out_buf_size);
        LZW_HW_output_length_ptr[i] = (unsigned int *)q.enqueueMapBuffer(out_len[i], CL_TRUE, CL_MAP_READ, 0, out_len_size);
        chunk_lengths_buf_ptr[i] = (unsigned int *)q.enqueueMapBuffer(chunk_lengths_buf[i], CL_TRUE, CL_MAP_WRITE, 0, chunk_lengths_size);
        chunk_numbers_buf_ptr[i] = (unsigned int *)q.enqueueMapBuffer(chunk_numbers_buf[i], CL_TRUE, CL_MAP_WRITE, 0, chunk_numbers_size);
        chunk_isdups_buf_ptr[i] = (unsigned char *)q.enqueueMapBuffer(chunk_isdups_buf[i], CL_TRUE, CL_MAP_WRITE, 0, chunk_isdups_size);
    }
    LOG(LOG_DEBUG, "initialization done\n");

    // OCL_CHECK(err, in_buf = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, in_buf_size, NULL, &err ));
    // OCL_CHECK(err, out_buf = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_WRITE_ONLY, out_buf_size, NULL, &err ));
    // OCL_CHECK(err, out_len = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_WRITE_ONLY, out_len_size, NULL, &err ));
    // OCL_CHECK(err,  chunk_lengths_buf = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, chunk_lengths_size, NULL, &err ));
    // OCL_CHECK(err,  chunk_numbers_buf = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, chunk_numbers_size, NULL, &err ));
    // OCL_CHECK(err,  chunk_isdups_buf = cl::Buffer(context, CL_MEM_ALLOC_HOST_PTR | CL_MEM_READ_ONLY, chunk_isdups_size, NULL, &err ));

    // to_fpga_buf = (unsigned char *)q.enqueueMapBuffer(in_buf, CL_TRUE, CL_MAP_WRITE, 0, in_buf_size);
    // from_fpga_buf = (unsigned char *)q.enqueueMapBuffer(out_buf, CL_TRUE, CL_MAP_READ, 0, out_buf_size);
    // LZW_HW_output_length_ptr = (unsigned int *)q.enqueueMapBuffer(out_len, CL_TRUE, CL_MAP_READ, 0, out_len_size);
    // chunk_lengths_buf_ptr = (unsigned int *)q.enqueueMapBuffer(chunk_lengths_buf, CL_TRUE, CL_MAP_WRITE, 0, chunk_lengths_size);
    // chunk_numbers_buf_ptr = (unsigned int *)q.enqueueMapBuffer(chunk_numbers_buf, CL_TRUE, CL_MAP_WRITE, 0, chunk_numbers_size);
    // chunk_isdups_buf_ptr = (unsigned char *)q.enqueueMapBuffer(chunk_isdups_buf, CL_TRUE, CL_MAP_WRITE, 0, chunk_isdups_size);

};

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
                        unsigned int* LZW_HW_output_length_ptr,

                        uint64_t num_chunks,
                        int packet_num){
    
    cl_int err;
    // std::vector<cl::Event> write_events_vec;
    // std::vector<cl::Event> execute_events_vec, read_events_vec;
    cl::Event write_event, execute_event, read_event;

    OCL_CHECK(err, err = kernel.setArg(0, in_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(1, chunk_lengths_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(2, chunk_numbers_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(3, chunk_isdups_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(4, num_chunks));
    OCL_CHECK(err, err = kernel.setArg(5, out_buf[packet_num]));
    OCL_CHECK(err, err = kernel.setArg(6, out_len[packet_num]));

    LOG(LOG_DEBUG, "KERNEL ARGS SET\n");

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({in_buf[packet_num], chunk_lengths_buf[packet_num], chunk_numbers_buf[packet_num], chunk_isdups_buf[packet_num]}, 0 /* 0 means from host*/, NULL, &write_event));
    std::vector<cl::Event> write_vec_temp;
    write_vec_temp.push_back(write_event);
    write_events_vec.push_back(write_vec_temp);

    OCL_CHECK(err, err = q.enqueueTask(kernel, &write_events_vec[num_events], &execute_event));
    std::vector<cl::Event> exec_vec_temp;
    exec_vec_temp.push_back(execute_event);
    execute_events_vec.push_back(exec_vec_temp);

    OCL_CHECK(err, err = q.enqueueMigrateMemObjects({out_buf[packet_num], out_len[packet_num]}, CL_MIGRATE_MEM_OBJECT_HOST /* 0 means from host*/, &execute_events_vec[num_events], &read_event));
    // read_event.wait();
    std::vector<cl::Event> read_vec_temp;
    read_vec_temp.push_back(read_event);
    read_events_vec.push_back(read_vec_temp);

    num_events++;

    // q.finish();

}

int LZW_kernel_call::LZW_wait_on_read_event(void)
{
    static int packet_num_wait = 0;
    if(!read_events_vec.empty()){
        LOG(LOG_DEBUG,"CL EVENTS: waiting for packet %d in host\n",packet_num_wait);
        read_events_vec[packet_num_wait][0].wait();
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


uint64_t LZW_encoding_packet_level(packet_t **packet_ring_buf, LZW_kernel_call *lzw_kernel, sem_t *sem_dedup_lzw, sem_t *sem_lzw, volatile int *sem_done)
{
    static int packet_num = 0;
    static int packets_pending = 0;
    static int packets_at_output = 0;
    static int total_packets_enqueued = 0;

    unsigned int *chunk_lengths;
    posix_memalign((void**)&chunk_lengths, 4*4096,MAX_NUM_CHUNKS*sizeof(unsigned int));
    unsigned int *chunk_numbers;
    posix_memalign((void**)&chunk_numbers, 4*4096,MAX_NUM_CHUNKS*sizeof(unsigned int));
    unsigned char *chunk_isdups;
    posix_memalign((void**)&chunk_isdups, 4*4096,MAX_NUM_CHUNKS*sizeof(unsigned char));

    // unsigned int* LZW_HW_output_length_ptr = (unsigned int*)calloc(1,sizeof(unsigned int));

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

        lzw_kernel->LZW_kernel_run(  in_buf_size, lzw_kernel->to_fpga_buf[packet_num], 
                                    out_buf_size, lzw_kernel->from_fpga_buf[packet_num], 
                                    chunk_lengths_size, lzw_kernel->chunk_lengths_buf_ptr[packet_num],
                                    chunk_numbers_size, lzw_kernel->chunk_numbers_buf_ptr[packet_num],
                                    chunk_isdups_size, lzw_kernel->chunk_isdups_buf_ptr[packet_num],
                                    out_len_size, lzw_kernel->LZW_HW_output_length_ptr[packet_num],
                                    num_chunks,packet_num);

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

    while(packets_pending--){
        LOG(LOG_DEBUG,"LZW_KERNEL Waiting for output packet = %d\n",packets_pending);
        lzw_kernel->LZW_wait_on_read_event();

        memcpy(&file[offset],lzw_kernel->from_fpga_buf[packets_at_output],*(lzw_kernel->LZW_HW_output_length_ptr[packets_at_output]));
        offset += *(lzw_kernel->LZW_HW_output_length_ptr[packets_at_output]);

        packets_at_output++;

        if(packets_at_output == NUM_PACKETS){
            packets_at_output = 0;
        }

        sem_post(sem_lzw);
    }

    LOG(LOG_DEBUG,"LZW KERNEL all pending packets removed waiting for q.finish()\n");

    lzw_kernel->LZW_q_finish_wait();
    // sem_wait(sem_dedup_lzw);
    // sem_post(sem_lzw);

    LOG(LOG_DEBUG,"kernel q emptied");
    LOG(LOG_DEBUG,"kernel SEMS received = %d\n",num_sems_received);

    sem_wait(sem_dedup_lzw);
    LOG(LOG_DEBUG,"kernel SEMS received = %d, sem_done = %d\n",num_sems_received,*sem_done);
    while ((*sem_done) == 0);
    sem_post(sem_lzw);

    LOG(LOG_DEBUG,"kernel SEMS received = %d\n",num_sems_received);

    //free(LZW_HW_output_length_ptr);
    free(chunk_lengths);
    free(chunk_numbers);
    free(chunk_isdups);

    std::cout << "Average time for LZW Encoding = " << lzw_time.avg_latency() << "ms" << " Total time = " << lzw_time.latency() << "ms" << std::endl;
    std::cout << "lzw wait time = " << lzw_wait_time.avg_latency() << "ms" << " Total wait time = " << lzw_wait_time.latency() << "ms" << std::endl;

    return offset;

}