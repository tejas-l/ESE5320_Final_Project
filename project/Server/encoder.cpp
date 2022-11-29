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

// void compression_flow(packet_t *new_packet, unsigned char *buffer, int length, chunk_t *new_chunk, LZW_kernel_call *lzw_kernel);

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

// void compression_flow(packet_t *new_packet, unsigned char *buffer, int length, chunk_t *new_chunk, LZW_kernel_call *lzw_kernel)
// {
//     static int num_packet = 0;
//     num_packet++;
    
//     cdc_time.start();
//     CDC_packet_level(new_packet);
//     cdc_time.stop();

//     sha_time.start();
//     SHA256_NEON_packet_level(new_packet);
//     sha_time.stop();

//     dedup_time.start();
//     dedup_packet_level(new_packet);
//     dedup_time.stop();

//     LOG(LOG_INFO_1,"LZW packet level called\n");

//     lzw_time.start();
//     LZW_encoding_packet_level(new_packet,lzw_kernel);
//     lzw_time.stop();

// }

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

    new_packet.buffer = &buffer[HEADER];//input[writer];
    new_packet.length = length;

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

    LZW_kernel_call lzw_kernel(context, program, q);


    total_time.start();

    /*
     * Create threads for the pipelined implementation 
     * of compression flow.
     */
    std::vector<std::thread> ths;


    sem_t sem_cdc;
    sem_t sem_cdc_sha;
    sem_t sem_sha_dedup;
    sem_t sem_dedup_lzw;
    sem_t sem_lzw;

    sem_init(&sem_cdc,0,1); // initialize with sem value 1 meaning ready
    sem_init(&sem_cdc_sha,0,0); // initialize with sem value 0 meaning not ready
    sem_init(&sem_sha_dedup,0,0); // initialize with sem value 0 meaning not ready
    sem_init(&sem_dedup_lzw,0,0); // initialize with sem value 0 meaning not ready
    sem_init(&sem_lzw,0,0); // initialize with sem value 0 meaning not ready

    // create threads for functions in the pipeline
    // ths.push_back(std::thread(&compression_flow, &new_packet, &buffer[HEADER], length, &new_chunk, &lzw_kernel));
    // cdc thread
    ths.push_back(std::thread(&CDC_packet_level, &new_packet, &sem_cdc, &sem_cdc_sha));
    pin_thread_to_cpu(ths[0],0);

    // for(auto &th : ths){
    //     th.join();
    // }

    //ths.pop_back();

    // sha neon thread
    ths.push_back(std::thread(&SHA256_NEON_packet_level, &new_packet, &sem_cdc_sha, &sem_sha_dedup));
    pin_thread_to_cpu(ths[1],0);

    // for(auto &th : ths){
    //     th.join();
    // }

    //ths.pop_back();

    // dedup thread
    ths.push_back(std::thread(&dedup_packet_level, &new_packet, &sem_sha_dedup, &sem_dedup_lzw));
    pin_thread_to_cpu(ths[2],0);

    // for(auto &th : ths){
    //     th.join();
    // }

    //ths.pop_back();

    // lzw thread
    ths.push_back(std::thread(&LZW_encoding_packet_level, &new_packet, &lzw_kernel, &sem_dedup_lzw, &sem_lzw));
    pin_thread_to_cpu(ths[3],0);

    for(auto &th : ths){
        th.join();
    }

    for(int i=0; i<ths.size(); i++){
        ths.pop_back();
    }

    sem_wait(&sem_lzw);
    sem_post(&sem_cdc);

    // compression_flow(&new_packet, &buffer[HEADER], length, &new_chunk, &lzw_kernel);

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
        data_received_bytes += length; // add the length of data received to the byte counter

        new_packet.buffer = &buffer[HEADER];//input[writer];
        new_packet.length = length;
        Input_bytes = length;

        LOG(LOG_INFO_2,"Start of Loop, packet size = %d\r\n",length);

        // compression_flow(&new_packet, &buffer[HEADER], length, &new_chunk, &lzw_kernel);

        // ths.push_back(std::thread(&compression_flow, &new_packet, &buffer[HEADER], length, &new_chunk, &lzw_kernel));

        // pin_thread_to_cpu(ths[0],0);

        // for(auto &th : ths){
        //     th.join();
        // }

        // ths.pop_back();

        ths.push_back(std::thread(&CDC_packet_level, &new_packet, &sem_cdc, &sem_cdc_sha));
        pin_thread_to_cpu(ths[0],0);

        // for(auto &th : ths){
        //     th.join();
        // }

        //ths.pop_back();

        // sha neon thread
        ths.push_back(std::thread(&SHA256_NEON_packet_level, &new_packet, &sem_cdc_sha, &sem_sha_dedup));
        pin_thread_to_cpu(ths[1],0);

        // for(auto &th : ths){
        //     th.join();
        // }

        //ths.pop_back();

        // dedup thread
        ths.push_back(std::thread(&dedup_packet_level, &new_packet, &sem_sha_dedup, &sem_dedup_lzw));
        pin_thread_to_cpu(ths[2],0);

        // for(auto &th : ths){
        //     th.join();
        // }

        //ths.pop_back();

        // lzw thread
        ths.push_back(std::thread(&LZW_encoding_packet_level, &new_packet, &lzw_kernel, &sem_dedup_lzw, &sem_lzw));
        pin_thread_to_cpu(ths[3],0);

        for(auto &th : ths){
            th.join();
        }

        for(int i=0; i<ths.size(); i++){
            ths.pop_back();
        }

        sem_wait(&sem_lzw);
        sem_post(&sem_cdc);

        writer++;
    }

    total_time.stop();
    
    LOG(LOG_INFO_1,"Number of bytes going into LZW - %d \n",LZW_in_bytes);
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

