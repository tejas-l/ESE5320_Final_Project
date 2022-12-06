#include "LZW_HW_packet.h"


/* defines for compress function */

#define OUT_SIZE_BITS 8
#define CODE_LENGTH 13
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define ARR_SIZE (8*1024)
#define MIN_CHUNK_SIZE (16)
#define MAX_PACKET_SIZE 8*1024
#define MAX_NUM_CHUNKS (MAX_PACKET_SIZE/MIN_CHUNK_SIZE)
#define BUCKET_SIZE 3   


#define HASH_TABLE_SIZE 8192
#define HASH_MOD HASH_TABLE_SIZE

typedef struct hash_node{   
    int code;
    uint64_t hash_value;
}hash_node_t;



uint64_t MurmurHash2(const void* key, int len, uint64_t seed) {

  const uint64_t m = 0xc6a4a7935bd1e995;
  const int r = 47;

  uint64_t h = seed ^ (len * m);

  const uint64_t * data = (const uint64_t *)key;
  const uint64_t * end = data + (len/8);

    while(data != end)
    {
        uint64_t k = *data;

        k *= m; 
        k ^= k >> r; 
        k *= m; 
        
        h ^= k;
        h *= m; 
        data++;
    }

  const unsigned char * data2 = (const unsigned char*)data;

  switch(len & 7)
  {
  case 7: h ^= uint64_t(data2[6]) << 48;
  case 6: h ^= uint64_t(data2[5]) << 40;
  case 5: h ^= uint64_t(data2[4]) << 32;
  case 4: h ^= uint64_t(data2[3]) << 24;
  case 3: h ^= uint64_t(data2[2]) << 16;
  case 2: h ^= uint64_t(data2[1]) << 8;
  case 1: h ^= uint64_t(data2[0]);
          h *= m;
  };
 
  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;

}


int Murmur_find(uint64_t hash_result,int code, uint64_t* table){

    for (int i = 0 ; i< code; i++){
        if (hash_result == table[i]){
            return i;
        }

    }
    return -1;
}

int Insert (hash_node_t hash_node_ptr[][BUCKET_SIZE], int code, uint64_t hash_result){

    uint64_t hash = hash_result % HASH_MOD;
    for(int col = 0; col < BUCKET_SIZE; col++){
        if(hash_node_ptr[hash][col].hash_value == 0){
            hash_node_ptr[hash][col].hash_value = hash_result;
            hash_node_ptr[hash][col].code = code;
            return 1;
        }
    }
    // printf("This hash collided Hash value = %ld hash0fhash = %ld \n",hash_result, hash);
    std::cout << "This hash collided Hash value = "<< hash_result << " hashofhash = "<< hash << std::endl;
    return -1;
}

int Find (hash_node_t hash_node_ptr[][BUCKET_SIZE], uint64_t hash_result){

    uint64_t hash = hash_result % HASH_MOD;
    for(int col = 0; col < BUCKET_SIZE; col++){
        if(hash_node_ptr[hash][col].hash_value == hash_result){
            return hash_node_ptr[hash][col].code;
        }
    }
    return -1;
}


// void associative_store(uint64_t hash_value, int code)
// {
//     ap_uint<142> key_p1[512]; 
//     ap_uint<142> key_p2[512]; 
//     ap_uint<142> key_p3[512]; 
//     ap_uint<142> key_p4[512]; 
//     ap_uint<142> key_p5[512]; 
//     ap_uint<142> key_p6[512]; 
//     ap_uint<142> key_p7[512]; 
//     ap_uint<142> key_p8[512]; 
        
//     uint16_t hash_val = hash_value >> 
    


// }

void read  (unsigned char* data_in,
            hls::stream<unsigned char> &in_stream,
            hls::stream<unsigned int> &chunk_numbers_read,
            hls::stream<unsigned int> &chunk_lengths_read,
            hls::stream<unsigned char> &chunk_isdups_read,
            unsigned int* chunk_numbers, 
            unsigned int* chunk_lengths,
            unsigned char* chunk_isdups,
            uint64_t num_chunks_local){

    uint32_t running_length = 0;
    unsigned int chunk_number_read = 0;
    unsigned int chunk_length_read = 0;
    unsigned char chunk_isdup_read = 0;

    for(uint64_t i =0; i < num_chunks_local; i++)
    {
        chunk_number_read = chunk_numbers[i];
        chunk_length_read = chunk_lengths[i];
        chunk_isdup_read = chunk_isdups[i];
        chunk_lengths_read.write(chunk_length_read);
        chunk_isdups_read.write(chunk_isdup_read);
        chunk_numbers_read.write(chunk_number_read);
        
        if(!chunk_isdup_read)
        {
            for (unsigned int j = 0; j < chunk_length_read; j++)
            {
                uint8_t temp_data = data_in[running_length + j + 2];
                in_stream.write(temp_data);  
            }
        }
        running_length += chunk_length_read;
    }
}


void execute_lzw(   hls::stream<unsigned char> &in_stream,
                    hls::stream<int> &lzw_out_stream,
                    hls::stream<unsigned char> &lzw_sync,
                    hls::stream<unsigned int> &chunk_numbers_read,
                    hls::stream<unsigned int> &chunk_lengths_read,
                    hls::stream<unsigned char> &chunk_isdups_read,
                    hls::stream<unsigned int> &chunk_numbers_lzw,
                    hls::stream<unsigned char> &chunk_isdups_lzw,
                    uint64_t num_chunks_local
                    ){
    
    unsigned int chunk_number_lzw = 0;
    unsigned int chunk_length_lzw = 0;
    unsigned char chunk_isdup_lzw = 0;

    for(uint64_t chunks =0; chunks < num_chunks_local; chunks++)
    {
        printf("For chunk number %d\n",chunks);
        chunk_length_lzw = chunk_lengths_read.read();
        chunk_isdup_lzw = chunk_isdups_read.read();
        chunk_number_lzw = chunk_numbers_read.read();

        chunk_isdups_lzw.write(chunk_isdup_lzw);
        chunk_numbers_lzw.write(chunk_number_lzw);
        
        if(!chunk_isdup_lzw)
        {
            hash_node_t hash_node_ptr[HASH_TABLE_SIZE][BUCKET_SIZE];

            for(int i = 0; i < ARR_SIZE; i++){
                for(int j = 0; j < BUCKET_SIZE; j++){
                    #pragma HLS UNROLL
                    hash_node_ptr[i][j].code = 0;
                    hash_node_ptr[i][j].hash_value = 0;         
                }
            }
            unsigned char substring_array[ARR_SIZE] = {0};
            unsigned int  substring_arr_index = 0;

            // for (unsigned int i = 0; i <= 255; i++)
            // {
            //     unsigned char ch = char(i);
            //     Insert(hash_node_ptr, i, MurmurHash2((void*)&ch,1,1));
            // }

            unsigned char c;
            substring_array[substring_arr_index] = in_stream.read(); // p += chunk->start[0] // Read the first element
            substring_arr_index++;

            int code = 256;
            int write_flag = 0;
            for (unsigned int i = 0; i < chunk_length_lzw; i++)
            {
                if (i != chunk_length_lzw - 1){
                    c = in_stream.read();
                    substring_array[substring_arr_index] = c;
                    substring_arr_index++;
                }

                uint64_t hash_result = MurmurHash2((void*)substring_array,substring_arr_index,1);

                //MurmurHash
                int res = Find(hash_node_ptr, hash_result);
                write_flag = 1;

                if ( res < 0 )
                {   
                    int value = 0;
                    if(substring_arr_index <= 2){
                        value = (int)substring_array[0];
                        write_flag = 0;
                    }else{
                        value = Find(hash_node_ptr, MurmurHash2((void*)substring_array,substring_arr_index-1,1));
                    }
                    lzw_sync.write(0);
                    lzw_out_stream.write(value); /*function that returns index*/
 
                    int retval = Insert(hash_node_ptr, code, hash_result); // Stores hash for p+c
                    if(retval == -1){
                        for(int s = 0; s< substring_arr_index; s++){
                            printf("%c", substring_array[s]);
                        }
                        printf("EOS\n");
                    }
                    code++;
                    substring_arr_index = 0;
                    substring_array[substring_arr_index] = c;
                    substring_arr_index++;
                }

            }

            // int value1 = Murmur_find( MurmurHash2(substring_array,substring_arr_index,1),code,table);  /*function that returns index*/
            if(write_flag){
                int value1 = Find(hash_node_ptr, MurmurHash2(substring_array,substring_arr_index,1));
                lzw_sync.write(0);
                lzw_out_stream.write(value1);
            }


            lzw_sync.write(1);
        }

    }
}

void execute_compress ( hls::stream<int> &lzw_out_stream,
                        hls::stream<unsigned char> &out_stream,
                        hls::stream<unsigned char> &lzw_sync,
                        hls::stream<unsigned char> &compress_sync,
                        hls::stream<unsigned int> &chunk_numbers_lzw,
                        hls::stream<unsigned char> &chunk_isdups_lzw,
                        hls::stream<unsigned int> &chunk_numbers_comp,
                        hls::stream<unsigned char> &chunk_isdups_comp,
                        uint64_t num_chunks_local){
    unsigned int chunk_number_comp = 0;
    unsigned char chunk_isdup_comp = 0;

    for(uint64_t chunks =0; chunks < num_chunks_local; chunks++)
    {
        chunk_isdup_comp = chunk_isdups_lzw.read();
        chunk_number_comp = chunk_numbers_lzw.read();
        chunk_isdups_comp.write(chunk_isdup_comp);
        chunk_numbers_comp.write(chunk_number_comp); 
        
        if(!chunk_isdup_comp)
        {
            uint64_t Length = 0;
            unsigned char Byte = 0;
            uint64_t output_index = 0;
            int i = 0;
            uint8_t rem_inBytes = CODE_LENGTH;
            uint8_t rem_outBytes = OUT_SIZE_BITS;
            
            unsigned char lzw_sync_local = lzw_sync.read();
            
            while(!lzw_sync_local)
            {
                i++;
                int inData = lzw_out_stream.read();
                rem_inBytes = CODE_LENGTH;

                while(rem_inBytes){

                    int read_bits = MIN(rem_inBytes,rem_outBytes);
                    Byte = (Byte << read_bits) | ( inData >> (rem_inBytes - read_bits) );

                    rem_inBytes -= read_bits;
                    rem_outBytes -= read_bits;
                    Length += read_bits;
                    inData &= ((0x1 << rem_inBytes) - 1);

                    if(rem_outBytes == 0)
                    {
                        compress_sync.write(0);
                        out_stream.write(Byte);
                        output_index++;
                        Byte = 0;
                        rem_outBytes = OUT_SIZE_BITS;
                    }
                }
                
                lzw_sync_local = lzw_sync.read();
            }
            
            if(Length % 8 > 0)
            {
                Byte = Byte << (8 - (Length%8));
                Length += (8 - (Length%8));
                compress_sync.write(0);
                out_stream.write(Byte); 
                output_index++;
            }
            compress_sync.write(1);
        }
    }
}


void write (hls::stream<unsigned char> &out_stream,
            hls::stream<unsigned char> &compress_sync,
            hls::stream<unsigned int> &chunk_numbers_comp,
            hls::stream<unsigned char> &chunk_isdups_comp,
            uint64_t num_chunks_local,
            unsigned char* output, 
            unsigned int *Output_length){
    
    int unique_index = 0;
    int out_index = 0;
    
    unsigned int chunk_number_write = 0;
    unsigned char chunk_isdup_write = 0;

    for(uint64_t chunks = 0; chunks < num_chunks_local; chunks++)
    {
        chunk_isdup_write = chunk_isdups_comp.read();
        chunk_number_write = chunk_numbers_comp.read();
        unsigned int dup_chunk = chunk_number_write;

        if(!chunk_isdup_write)
        {
            uint32_t header = 0;
            int unique_length = 0;

            unique_index = out_index;
            out_index += 4;
            unsigned char comp_sync = compress_sync.read();

            while(!comp_sync)
            {
                output[out_index] = out_stream.read();
                out_index++;
                unique_length++;
                comp_sync = compress_sync.read();
            }

            header = (unique_length << 1); /* size of the new chunk */

            output[unique_index] = header;// & 0xFF;
            unique_index++;
            output[unique_index] = (header >> 8);// & 0xFF;
            unique_index++;
            output[unique_index] = (header >> 16);// & 0xFF;
            unique_index++;
            output[unique_index] = (header >> 24);// & 0xFF;
            unique_index++;
        }
        else
        {
            uint32_t header_dup = 0;
            
            header_dup = (dup_chunk << 1);
            header_dup |= (0x01);

            output[out_index] = header_dup;// & 0xFF;
            out_index++;
            output[out_index] = (header_dup >> 8);// & 0xFF;
            out_index++;
            output[out_index] = (header_dup >> 16);// & 0xFF;
            out_index++;
            output[out_index] = (header_dup >> 24);// & 0xFF;
            out_index++;
        }
    }
    *Output_length = out_index;
}


void LZW_encoding_HW(unsigned char* data_in, unsigned int* chunk_lengths, unsigned int* chunk_numbers,
                     unsigned char* chunk_isdups, uint64_t num_chunks, unsigned char* output, unsigned int* Output_length)//changed output array datatype
{

#pragma HLS INTERFACE m_axi depth=8192 port=data_in bundle=p2
#pragma HLS INTERFACE m_axi depth=8192 port=output bundle=p1
#pragma HLS INTERFACE m_axi depth=512 port=chunk_lengths bundle=p0
#pragma HLS INTERFACE m_axi depth=512 port=chunk_numbers bundle=p0
#pragma HLS INTERFACE m_axi depth=512 port=chunk_isdups bundle=p0

uint64_t num_chunks_local = num_chunks; 

    #pragma HLS DATAFLOW

    hls::stream<unsigned char, 10*1024> out_stream("out_stream");
    hls::stream<int, 10*1024> lzw_out_stream("lzw_out_stream");
    hls::stream<unsigned char, 10*1024> in_stream("in_stream");
    hls::stream<unsigned char, 10*1024> compress_sync("compress_sync");
    hls::stream<unsigned char, 10*1024> lzw_sync("lzw_sync");
    hls::stream<unsigned int, 512> chunk_numbers_read("chunk_numbers_read");
    hls::stream<unsigned int, 512> chunk_numbers_lzw("chunk_numbers_lzw");
    hls::stream<unsigned int, 512> chunk_numbers_comp("chunk_numbers_comp");
    hls::stream<unsigned int, 512> chunk_lengths_read("chunk_lengths_read");
    hls::stream<unsigned char, 512> chunk_isdups_read("chunk_isdups_read");
    hls::stream<unsigned char, 512> chunk_isdups_lzw("chunk_isdups_lzw");
    hls::stream<unsigned char, 512> chunk_isdups_comp("chunk_isdups_comp");


    read(data_in, in_stream, chunk_numbers_read, chunk_lengths_read, chunk_isdups_read, chunk_numbers, chunk_lengths, chunk_isdups, num_chunks_local);

    execute_lzw(in_stream, lzw_out_stream, lzw_sync, chunk_numbers_read, chunk_lengths_read, chunk_isdups_read, chunk_numbers_lzw, chunk_isdups_lzw, num_chunks_local);  

    execute_compress(lzw_out_stream, out_stream, lzw_sync, compress_sync, chunk_numbers_lzw, chunk_isdups_lzw, chunk_numbers_comp, chunk_isdups_comp, num_chunks_local);

    write(out_stream, compress_sync, chunk_numbers_comp, chunk_isdups_comp, num_chunks_local, output, Output_length); 

}
