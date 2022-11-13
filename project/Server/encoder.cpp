#include "encoder.h"


int offset = 0;
unsigned char* file;

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

static unsigned int LZW_in_bytes = 0;

void compression_flow(unsigned char *buffer, int length, chunk_t *new_cdc_chunk)
{
    for(int i=0; i<length; i += new_cdc_chunk->length){

        cdc_new(&buffer[i],new_cdc_chunk,length,i);
    
        printf("Chunk Start = %p, length = %d\n",new_cdc_chunk->start, new_cdc_chunk->length);

        std::string SHA_result = SHA_384(new_cdc_chunk);
        
        uint32_t dup_chunk_num;
        if((dup_chunk_num = dedup(SHA_result,new_cdc_chunk))){
            uint32_t header = 0;
            header |= (dup_chunk_num<<1); // 31 bits specify the number of the chunk to be duplicated
            header |= (0x1); // LSB 1 indicates this is a duplicate chunk

            printf("DUPLICATE CHUNK : %p CHUNK NO : %d\n",new_cdc_chunk->start, dup_chunk_num);
            printf("Header written %d\n",header);

            memcpy(&file[offset], &header, sizeof(header)); // write header to the file
            offset += sizeof(header);

        }else{
            uint32_t header = 0;
            printf("NEW CHUNK : %p chunk no = %d\n",new_cdc_chunk->start, new_cdc_chunk->number);
           
            LZW_in_bytes += new_cdc_chunk->length;

            std::vector<int> compressed_data = LZW_encoding(new_cdc_chunk);
            
            uint64_t compressed_size = ceil(13*compressed_data.size() / 8.0);
            header |= ( compressed_size <<1); /* size of the new chunk */
            header &= ~(0x1); /* lsb equals 0 signifies new chunk */
            printf("Header written %x\n",header);
            memcpy(&file[offset], &header, sizeof(header)); /* write header to the output file */
            offset += sizeof(header);

            // compress the lzw encoded data
            uint64_t compressed_length = compress(compressed_data);

            if(compressed_length != compressed_size){
                std::cout << "Error: lengths not matching, calculated = " << compressed_size << ", return_val = " << std::endl;
                exit(1);
            }
            offset += compressed_length;
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
    chunk_t new_cdc_chunk;
    new_cdc_chunk.number = 0;

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
    stopwatch total_time;

    total_time.start();

    writer = pipe_depth;
    server.get_packet(input[writer]);
    count++;

    // get packet
    unsigned char* buffer = input[writer];

    // decode
    done = buffer[1] & DONE_BIT_L;
    length = buffer[0] | (buffer[1] << 8);
    length &= ~DONE_BIT_H;
    std::cout << " packet length " << length << std::endl;

    // printing takes time so be weary of transfer rate

    compression_flow(&buffer[HEADER], length, &new_cdc_chunk);

    //offset += length;
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

        printf("Start of Loop, packet size = %d\r\n",length);

        compression_flow(&buffer[HEADER], length, &new_cdc_chunk);

        writer++;
    }
    
    printf("Number of bytes going into LZW - %d \n",LZW_in_bytes);
    // write file to root and you can use diff tool on board
    FILE *outfd = fopen(filename, "wb");
    int bytes_written = fwrite(&file[0], 1, offset, outfd);

    total_time.stop();
    std::cout << "Total runtime = " << total_time.avg_latency() << "ns" << std::endl;
    std::cout << "Throughput = " << (bytes_written*8/1000000.0)/(total_time.avg_latency()/1000.0) << " Mb/s" << std::endl;

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


