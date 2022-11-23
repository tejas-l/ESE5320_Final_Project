#include "encoder.h"


void compression_flow(packet_t *new_packet, unsigned char *buffer, int length, chunk_t *new_chunk, LZW_kernel_call &lzw_kernel);

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

void compression_flow(packet_t *new_packet, unsigned char *buffer, int length, chunk_t *new_chunk, LZW_kernel_call &lzw_kernel)
{
    cdc_time.start();
    CDC_packet_level(new_packet);
    cdc_time.stop();
    // Input_bytes += new_chunk->length;
    
    LOG(LOG_CRIT,"NUM CHUNKS = %d \n",new_packet->num_chunks);

    // sha_time.start();
    // SHA256_NEON_packet_level(new_packet);
    // sha_time.stop();

    // printf("SHA result - \n");
    // for(int j = 0; j<32; j++){
    //     printf("%02x",new_packet->chunk_list[0].SHA_signature[j]);
    // }
    // printf("\n");

    // LOG(LOG_CRIT,"NUM CHUNKS = %d \n",new_packet->num_chunks);

    chunk_t *chunklist_ptr = new_packet->chunk_list;

    //for(int i=0; i<length; i += new_chunk->length){
    for(int i=0; i<new_packet->num_chunks; i++){

        new_chunk = &chunklist_ptr[i];

        sha_time.start();
        SHA256_NEON(new_chunk);
        sha_time.stop();

        //printf("Chunk Start = %p, length = %d\n",new_chunk->start, new_chunk->length);
        // LOG(LOG_INFO_2,"Chunk Start = %p, length = %d\n",new_chunk->start, new_chunk->length);


        //std::string SHA_result(new_chunk->SHA_signature);

        uint32_t dup_chunk_num;
        dedup_time.start();

        if((dup_chunk_num = dedup(new_chunk))){

            dedup_time.stop();
            uint32_t header = 0;
            header |= (dup_chunk_num<<1); // 31 bits specify the number of the chunk to be duplicated
            header |= (0x1); // LSB 1 indicates this is a duplicate chunk

            LOG(LOG_INFO_2,"DUPLICATE CHUNK : %p CHUNK NO : %d\n",new_chunk->start, dup_chunk_num);
            LOG(LOG_INFO_2,"Header written %d\n",header);

            memcpy(&file[offset], &header, sizeof(header)); // write header to the file
            offset += sizeof(header);

        }else{

            dedup_time.stop();

            LOG(LOG_INFO_2,"NEW CHUNK : %p chunk no = %d\n",new_chunk->start, new_chunk->number);
           
            LZW_in_bytes += new_chunk->length;

            lzw_time.start();

            unsigned int LZW_HW_output_length = 0;
            size_t in_buf_size = new_chunk->length*sizeof(unsigned char);
            size_t out_buf_size = KERNEL_OUT_SIZE*sizeof(unsigned char);
            size_t out_len_size = sizeof(unsigned int);

            unsigned int* LZW_HW_output_length_ptr = (unsigned int *)calloc(1,sizeof(unsigned int));
            unsigned int HW_LZW_IN_LEN = new_chunk->length;

            lzw_kernel.LZW_kernel_run(HW_LZW_IN_LEN, in_buf_size, new_chunk->start, out_buf_size, &file[offset], out_len_size, LZW_HW_output_length_ptr);

            lzw_time.stop();

            offset += *LZW_HW_output_length_ptr;
        }
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

    new_packet.buffer = input[writer];
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

    LZW_kernel_call LZW_kernel_call(context, program, q);

    total_time.start();

    compression_flow(&new_packet, &buffer[HEADER], length, &new_chunk, LZW_kernel_call);

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

        new_packet.buffer = input[writer];
        new_packet.length = length;

        LOG(LOG_INFO_2,"Start of Loop, packet size = %d\r\n",length);

        compression_flow(&new_packet, &buffer[HEADER], length, &new_chunk, LZW_kernel_call);

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

    std::cout << "Throughput = " << (data_received_bytes*8/1000000.0)/output_time << " Mb/s" << std::endl;

    LOG(LOG_CRIT,"write file with %d\n", bytes_written);
    fclose(outfd);

    for (int i = 0; i < NUM_PACKETS; i++) {
        free(input[i]);
    }
	
    free(file);
    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    float input_throughput = (Input_bytes * 8 / 1000000.0) / ethernet_latency; // Mb/s
    std::cout << "Input Throughput to Encoder: " << input_throughput << " Mb/s."
            << " (Latency: " << ethernet_latency << "s)." << std::endl;


    return 0;
}


