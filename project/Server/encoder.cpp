#include "encoder.h"


// from https://eli.thegreenplace.net/2016/c11-threads-affinity-and-hyperthreading/
void pin_thread_to_cpu(std::thread &t, int cpu_num)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(__APPLE__)
    return;
#else
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_num, &cpuset);
    int rc =
        pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0)
    {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
#endif
}

void pin_main_thread_to_cpu0()
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__) || defined(__APPLE__)
    return;
#else
    pthread_t thread;
    thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    int rc =
        pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (rc != 0)
    {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
#endif
}


int offset = 0;
unsigned char* file;

void handle_input(int argc, char* argv[], int* blocksize) {
    int x;
    extern char *optarg;

    while ((x = getopt(argc, argv, ":b:")) != -1) {
        switch(x) {
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

static unsigned int LZW_in_bytes = 0;
static unsigned int Input_bytes = 0;
stopwatch cdc_time;
stopwatch sha_time;
stopwatch dedup_time;
stopwatch lzw_time;

volatile int input_packets_done = 0;
volatile int num_input_packets = 0;
volatile int sem_done;


int main(int argc, char* argv[]) {
    stopwatch ethernet_timer;
    unsigned char* input[NUM_PACKETS];
    int writer = 0;
    int done = 0;
    int length = 0;
    int count = 0;
    ESE532_Server server;
    chunk_t new_chunk;
    packet_t new_packet;

    packet_t *packet_ring_buffer[NUM_PACKETS];
    int packet_num = 0;
    int num_packets_in_pipeline = 0;

    int num_packets_received = 0;
    int num_packets_processed = 0;

    /* packets structs for different stages in pipeline */

    new_chunk.number = 0;

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

    // file = (unsigned char*) malloc(sizeof(unsigned char) * 70000000);
    posix_memalign((void**)&file, 4*4096, sizeof(unsigned char) * 70000000);
    if (file == NULL) {
        printf("help\n");
    }

    for (int i = 0; i < NUM_PACKETS; i++) {
        // input[i] = (unsigned char*) malloc(
        //         sizeof(unsigned char) * (NUM_ELEMENTS + HEADER));
        posix_memalign((void**)&input[i], 4*4096, sizeof(unsigned char) * (NUM_ELEMENTS + HEADER));
        if (input[i] == NULL) {
            std::cout << "aborting " << std::endl;
            return 1;
        }

        /* mem alloc for num packets */
        packet_ring_buffer[i] = (packet_t *) malloc(1*sizeof(packet_t));
        if (input[i] == NULL) {
            std::cout << "aborting " << std::endl;
            return 1;
        }
    }

    server.setup_server(blocksize);

    int file_size = 14247; // 14247 bytes per second
    uint64_t data_received_bytes = 0;
    stopwatch total_time;


    writer = pipe_depth;
    server.get_packet(input[writer]);
    count++;

    // get packet
    unsigned char* buffer = input[writer];

    // decode
    done = buffer[1] & DONE_BIT_L;
    length = buffer[0] | (buffer[1] << 8);
    length &= ~DONE_BIT_H;
    data_received_bytes += length; // add the length of data received to the byte counter
    std::cout << " packet length " << length << std::endl;

    packet_ring_buffer[packet_num]->buffer = &buffer[HEADER];
    packet_ring_buffer[packet_num]->length = length;
    packet_num++;
    num_packets_in_pipeline++;
    writer++;
    num_packets_received++;

    cl_int err;
    std::string binaryFile = "LZW_encoding_HW.xclbin";
    unsigned fileBufSize;
    std::vector<cl::Device> devices = get_xilinx_devices();
    devices.resize(1);
    cl::Device device = devices[0];
    cl::Context context(device, NULL, NULL, NULL, &err);
    char *fileBuf = read_binary_file(binaryFile, fileBufSize);
    cl::Program::Binaries bins{{fileBuf, fileBufSize}};
    cl::Program program(context, devices, bins, NULL, &err);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);

    LZW_kernel_call lzw_kernel(context, program, q);

    LOG(LOG_DEBUG, "LZW constructor initialized\n");


    /*
     * Create threads for the pipelined implementation 
     * of compression flow.
     */
    std::vector<std::thread> ths;


    // semaphores for thread synchronization
    sem_t sem_cdc;
    sem_t sem_cdc_sha;
    sem_t sem_sha_dedup;
    sem_t sem_dedup_lzw;
    sem_t sem_lzw;


    sem_init(&sem_cdc,0,0); // initialize with sem value 0 meaning not ready
    sem_init(&sem_cdc_sha,0,0); // initialize with sem value 0 meaning not ready
    sem_init(&sem_sha_dedup,0,0); // initialize with sem value 0 meaning not ready
    sem_init(&sem_dedup_lzw,0,0); // initialize with sem value 0 meaning not ready
    sem_init(&sem_lzw,0,0); // initialize with sem value 0 meaning not ready

    pin_main_thread_to_cpu0();

    // create threads for functions in the pipeline
    // cdc thread
    ths.push_back(std::thread(&CDC_packet_level, &packet_ring_buffer[0], &sem_cdc, &sem_cdc_sha, &sem_done));
    pin_thread_to_cpu(ths[0],0);

    // sha neon thread
    ths.push_back(std::thread(&SHA256_NEON_packet_level, &packet_ring_buffer[0], &sem_cdc_sha, &sem_sha_dedup, &sem_done));
    pin_thread_to_cpu(ths[1],1);

    // dedup thread
    ths.push_back(std::thread(&dedup_packet_level, &packet_ring_buffer[0], &sem_sha_dedup, &sem_dedup_lzw, &sem_done));
    pin_thread_to_cpu(ths[2],2);

    // lzw thread
    ths.push_back(std::thread(&LZW_encoding_packet_level, &packet_ring_buffer[0], &lzw_kernel, &sem_dedup_lzw, &sem_lzw, &sem_done));
    pin_thread_to_cpu(ths[3],3);

    total_time.start();
    int num_sems_released = 0;

    sem_post(&sem_cdc);
    num_sems_released++;
    num_input_packets++;

    LOG(LOG_DEBUG,"num_packets_in_pipeline = %d\n",num_packets_in_pipeline);

    //last message
    while (!done) {
        // reset ring buffer
        if (writer == NUM_PACKETS) {
            writer = 0;
        }
        if (packet_num == NUM_PACKETS){
            packet_num = 0;
        }

        LOG(LOG_DEBUG,"num_packets_in_pipeline = %d\n",num_packets_in_pipeline);


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
        data_received_bytes += length; // add the length of data received to the byte counter

        packet_ring_buffer[packet_num]->buffer = &buffer[HEADER];
        packet_ring_buffer[packet_num]->length = length;
        packet_num++;
        LOG(LOG_DEBUG,"packet_number = %d\n",packet_num);
        num_packets_in_pipeline++;
        writer++;
        num_packets_received++;

        Input_bytes += length;

        LOG(LOG_DEBUG, "posted cdc sem, num packets in pipeline = %d\n",num_packets_in_pipeline);

        num_input_packets++;
        sem_post(&sem_cdc);
        num_sems_released++;

        if(num_packets_in_pipeline == NUM_PACKETS){//(NUM_PACKETS-1)){
            sem_wait(&sem_lzw);
            num_packets_in_pipeline--;
            num_packets_processed++;
            LOG(LOG_DEBUG, "received lzw semaphore\n");
        }

        LOG(LOG_DEBUG,"Start of Loop, packet size = %d\r\n",length);
    }

    // signal threads that all input packets are inserted.
    input_packets_done = 1;
    LOG(LOG_DEBUG,"EXITED WHILE LOOP");
    LOG(LOG_DEBUG,"releasing sem done, num sems released = %d\n",num_sems_released);

    while(num_packets_in_pipeline--){
        LOG(LOG_DEBUG,"MAIN: PACKETS IN PIPELINE = %d\n",num_packets_in_pipeline);
        sem_wait(&sem_lzw);
        num_packets_processed++;
    }

    total_time.stop();

    // make the threads exit
    sem_done = 1;
    sem_post(&sem_cdc);
    num_sems_released++;
    LOG(LOG_DEBUG,"MAIN: num sems released = %d\n",num_sems_released);
    sem_wait(&sem_lzw);

    for(auto &th : ths){
        th.join();
    }

    for(int i=0; i<4; i++){
        ths.pop_back();
    }

    LOG(LOG_INFO_1,"Number of bytes going into LZW - %d \n",LZW_in_bytes);
    LOG(LOG_INFO_1,"Number of packets received = %d\n",num_packets_received);
    LOG(LOG_INFO_1,"Number of packets processed = %d\n",num_packets_processed);

    // write file to root and you can use diff tool on board
    FILE *outfd = fopen(filename, "wb");
    int bytes_written = fwrite(&file[0], 1, offset, outfd);
    std::cout << "Average time for CDC = " << cdc_time.avg_latency() << "ms" << " Total time = " << cdc_time.latency() << "ms" << std::endl;
    std::cout << "Average time for SHA = " << sha_time.avg_latency() << "ms" << " Total time = " << sha_time.latency() << "ms" << std::endl;
    std::cout << "Average time for Dedup = " << dedup_time.avg_latency() << "ms" << " Total time = " << dedup_time.latency() << "ms" << std::endl;
    std::cout << "Average time for LZW Encoding = " << lzw_time.avg_latency() << "ms" << " Total time = " << lzw_time.latency() << "ms" << std::endl;

    std::cout << "Total runtime = " << total_time.latency() << "ms" << std::endl;
    
    float output_time = total_time.latency()-lzw_time.latency();
    
    std::cout << "Total runtime without LZW = " << output_time << "ms" << std::endl;

    output_time = output_time/1000.0;

    std::cout << "Databytes received = " << data_received_bytes << std::endl;

    std::cout << "Throughput wihout LZW = " << (data_received_bytes*8/1000000.0)/output_time << " Mb/s" << std::endl;

    std::cout << "Throughput = " << (data_received_bytes*8/1000000.0)/(total_time.latency()/1000.0) << " Mb/s" << std::endl;

    LOG(LOG_CRIT,"write file with %d\n", bytes_written);
    fclose(outfd);

    for (int i = 0; i < NUM_PACKETS; i++) {
        free(input[i]);
        free(packet_ring_buffer[i]);
    }
	
    free(file);
    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    float input_throughput = (Input_bytes * 8 / 1000000.0) / ethernet_latency; // Mb/s

    std::cout<< "Input bytes" << Input_bytes << std::endl;
    std::cout << "Input Throughput to Encoder: " << input_throughput << " Mb/s."
            << " (Latency: " << ethernet_latency << "s)." << std::endl;


    return 0;
}

