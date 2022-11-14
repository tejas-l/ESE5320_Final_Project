#include "encoder.h"


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
stopwatch compress_time;

void compression_flow(unsigned char *buffer, int length, chunk_t *new_cdc_chunk)
{

    for(int i=0; i<length; i += new_cdc_chunk->length){

        cdc_time.start();
        CDC(&buffer[i],new_cdc_chunk,length,i);
        cdc_time.stop();
        Input_bytes += new_cdc_chunk->length;

        //printf("Chunk Start = %p, length = %d\n",new_cdc_chunk->start, new_cdc_chunk->length);
        LOG(LOG_INFO_2,"Chunk Start = %p, length = %d\n",new_cdc_chunk->start, new_cdc_chunk->length);

        sha_time.start();
        std::string SHA_result = SHA_384(new_cdc_chunk);
        sha_time.stop();

        uint32_t dup_chunk_num;
        dedup_time.start();

        if((dup_chunk_num = dedup(SHA_result,new_cdc_chunk))){

            dedup_time.stop();
            uint32_t header = 0;
            header |= (dup_chunk_num<<1); // 31 bits specify the number of the chunk to be duplicated
            header |= (0x1); // LSB 1 indicates this is a duplicate chunk

            LOG(LOG_INFO_2,"DUPLICATE CHUNK : %p CHUNK NO : %d\n",new_cdc_chunk->start, dup_chunk_num);
            LOG(LOG_INFO_2,"Header written %d\n",header);

            memcpy(&file[offset], &header, sizeof(header)); // write header to the file
            offset += sizeof(header);

        }else{
            uint32_t header = 0;
            LOG(LOG_INFO_2,"NEW CHUNK : %p chunk no = %d\n",new_cdc_chunk->start, new_cdc_chunk->number);
           
            LZW_in_bytes += new_cdc_chunk->length;

            lzw_time.start();
            //std::vector<int> compressed_data = LZW_encoding(new_cdc_chunk);
            uint64_t compressed_length = LZW_encoding(new_cdc_chunk);

            lzw_time.stop();

            compress_time.start();
            
            // uint64_t compressed_size = ceil(13*compressed_data.size() / 8.0);
            // header |= ( compressed_size <<1); /* size of the new chunk */
            // header &= ~(0x1); /* lsb equals 0 signifies new chunk */
            // LOG(LOG_DEBUG,"Header written %x\n",header);
            // memcpy(&file[offset], &header, sizeof(header)); /* write header to the output file */
            // offset += sizeof(header);

            // compress the lzw encoded data
            //uint64_t compressed_length = compress(compressed_data);

            compress_time.stop();
            

            if(compressed_length != compressed_size){
                //std::cout << "Error: lengths not matching, calculated = " << compressed_size << ", return_val = " << compressed_length<< std::endl;
                LOG(LOG_ERR,"Error on line %d: lengths not matching, calculated = %d, return_val = %d\n",__LINE__,compressed_size, compressed_length);
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

    total_time.start();

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

        //printf("Start of Loop, packet size = %d\r\n",length);
        LOG(LOG_INFO_2,"Start of Loop, packet size = %d\r\n",length);

        compression_flow(&buffer[HEADER], length, &new_cdc_chunk);

        writer++;
    }

    total_time.stop();
    
    //printf("Number of bytes going into LZW - %d \n",LZW_in_bytes);
    LOG(LOG_INFO_1,"Number of bytes going into LZW - %d \n",LZW_in_bytes);
    // write file to root and you can use diff tool on board
    FILE *outfd = fopen(filename, "wb");
    int bytes_written = fwrite(&file[0], 1, offset, outfd);
    std::cout << "Total time for CDC = " << cdc_time.avg_latency() << "ns" << std::endl;
    std::cout << "Total time for SHA = " << sha_time.avg_latency() << "ns" << std::endl;
    std::cout << "Total time for Dedup = " << dedup_time.avg_latency() << "ns" << std::endl;
    std::cout << "Total time for LZW Encoding = " << lzw_time.avg_latency() << "ns" << std::endl;
    std::cout << "Total time for Compress = " << compress_time.avg_latency() << "ns" << std::endl;

    std::cout << "Total runtime = " << total_time.latency() << "ns" << std::endl;
    float output_time = (total_time.latency()/1000.0);
    std::cout << "Throughput = " << (file_size*8/1000000.0)/output_time << " Mb/s" << std::endl;

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


