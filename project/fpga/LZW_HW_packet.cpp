#include "LZW_HW_packet.h"


/* defines for compress function */

#define OUT_SIZE_BITS 8
#define CODE_LENGTH 13
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define ARR_SIZE (8*1024)
#define MIN_CHUNK_SIZE (16)
#define MAX_PACKET_SIZE 8*1024
#define MAX_NUM_CHUNKS (MAX_PACKET_SIZE/MIN_CHUNK_SIZE)
#define BUCKET_SIZE 2


#define HASH_TABLE_SIZE 8192
#define HASH_MOD HASH_TABLE_SIZE

typedef struct hash_node{   
    int code;
    uint32_t hash_value;
}hash_node_t;

typedef ap_uint<72> key_size_t;



// uint64_t MurmurHash2(const void* key, int len, uint64_t seed) {

//   const uint64_t m = 0xc6a4a7935bd1e995;
//   const int r = 47;

//   uint64_t h = seed ^ (len * m);

//   const uint64_t * data = (const uint64_t *)key;
//   const uint64_t * end = data + (len/8);

//     while(data != end)
//     {
//         uint64_t k = *data;

//         k *= m; 
//         k ^= k >> r; 
//         k *= m; 
        
//         h ^= k;
//         h *= m; 
//         data++;
//     }

//   const unsigned char * data2 = (const unsigned char*)data;

//   switch(len & 7)
//   {
//   case 7: h ^= uint64_t(data2[6]) << 48;
//   case 6: h ^= uint64_t(data2[5]) << 40;
//   case 5: h ^= uint64_t(data2[4]) << 32;
//   case 4: h ^= uint64_t(data2[3]) << 24;
//   case 3: h ^= uint64_t(data2[2]) << 16;
//   case 2: h ^= uint64_t(data2[1]) << 8;
//   case 1: h ^= uint64_t(data2[0]);
//           h *= m;
//   };
 
//   h ^= h >> r;
//   h *= m;
//   h ^= h >> r;

//   return h;

// }
uint32_t FNV_32(const void * hash_result, unsigned int length) {
	const unsigned int fnv_prime = 0x811C9DC5;
	unsigned int hash = 0;
	const char * str = (const char *)hash_result;
	unsigned int i = 0;

FNV_LOOP:for (i = 0; i < length; str++, i++)
	{
		hash *= fnv_prime;
		hash ^= (*str);
	}

	return hash;
}

// int Murmur_find(uint64_t hash_result,int code, uint64_t* table){

//     for (int i = 0 ; i< code; i++){
//         if (hash_result == table[i]){
//             return i;
//         }

//     }
//     return -1;
// }

int Insert (hash_node_t hash_node_ptr[][BUCKET_SIZE], int code, uint32_t hash_result){
    uint32_t hash = hash_result % HASH_MOD;
INSERT_LOOP:for(int col = 0; col < BUCKET_SIZE; col++){
    #pragma HLS UNROLL factor=3
        if(hash_node_ptr[hash][col].hash_value == 0){
            hash_node_ptr[hash][col].hash_value = hash_result;
            hash_node_ptr[hash][col].code = code;
            return 1;
        }
    }
    // printf("Collision detected\n");
    return -1;
}

int Find (hash_node_t hash_node_ptr[][BUCKET_SIZE], uint32_t hash_result){
    uint32_t hash = hash_result % HASH_MOD;
FIND_LOOP:for(int col = 0; col < BUCKET_SIZE; col++){
    #pragma HLS UNROLL factor=3
        if(hash_node_ptr[hash][col].hash_value == hash_result){
            return hash_node_ptr[hash][col].code;
        }
    }
    return -1;
}


uint8_t associative_insert(key_size_t key[][512], int* value, uint8_t counter, uint32_t hash_value, int code)
{
    ap_uint<9> index[4] ;
    value[counter] = code;
    ap_uint<72> one_hot = 0 ; 
    one_hot =  1 << counter;

ASSOCIATIVE_INSERT_LOOP_1:for (int i = 0; i < 4 ; i++)
    {
        #pragma HLS UNROLL 
        index[i] = (hash_value >> (9*i)) & (0x1FF);    
    }

ASSOCIATIVE_INSERT_LOOP_2:for (int i = 0; i < 4 ; i++){
    #pragma HLS UNROLL 

        key[i][index[i]] |= one_hot; 

    }

    counter ++; 
    return counter; 

}

int reverse_one_hot(ap_uint<72> address){
#pragma HLS INLINE
REVERSE_ONE_HOT_LOOP:for(int i = 0; i<72; i++){
        if((address>>i) == 1){
            return i;
        }
    }
    return -1;
}

int associative_find(key_size_t key[][512], int* value, uint32_t hash_value)
{
    ap_uint<9> index[4]; 
ASSOCIATIVE_FIND_LOOP_1:for (int i = 0; i < 4 ; i++)
    {
        #pragma HLS UNROLL 
        index[i] = (hash_value >> (9*i)) & (0x1FF);    
    }

    ap_uint<72> address = key[0][index[0]] & key[1][index[1]]; 
ASSOCIATIVE_FIND_LOOP_2:for (int i = 2;  i < 4; i++ )
    {
        #pragma HLS UNROLL
        address &=  key[i][index[i]];
    }

    if(address == 0){
        return -1;
    }

    int address_value = reverse_one_hot(address); 
    int code = value[address_value]; 
    if(code == 0 )
    {
        code = -1; 
    }
    

    return code;

}

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

READ_LOOP_1:for(uint64_t i =0; i < num_chunks_local; i++)
    {
        chunk_number_read = chunk_numbers[i];
        chunk_length_read = chunk_lengths[i];
        chunk_isdup_read = chunk_isdups[i];
        chunk_lengths_read.write(chunk_length_read);
        chunk_isdups_read.write(chunk_isdup_read);
        chunk_numbers_read.write(chunk_number_read);
        
        if(!chunk_isdup_read)
        {
            READ_LOOP_2:for (unsigned int j = 0; j < chunk_length_read; j++)
            {
                #pragma HLS PIPELINE
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



LZW_LOOP_1:for(uint64_t chunks =0; chunks < num_chunks_local; chunks++)
    {
        chunk_length_lzw = chunk_lengths_read.read();
        chunk_isdup_lzw = chunk_isdups_read.read();
        chunk_number_lzw = chunk_numbers_read.read();

        chunk_isdups_lzw.write(chunk_isdup_lzw);
        chunk_numbers_lzw.write(chunk_number_lzw);
        
        if(!chunk_isdup_lzw)
        {
            hash_node_t hash_node_ptr[HASH_TABLE_SIZE][BUCKET_SIZE];
            #pragma HLS array_partition variable=hash_node_ptr block factor=2 dim=2
            key_size_t key[4][512]; 
            #pragma HLS array_partition variable=key block factor=4 dim=1
            int value[72];
            uint8_t counter = 0;
            LZW_KEY_CLEAR_LOOP_1:for(int i = 0; i<512; i++){
                #pragma HLS loop_flatten
                // #pragma HLS loop_merge force
                LZW_KEY_CLEAR_LOOP_2:for(int j = 0; j < 4; j++){
                #pragma HLS UNROLL factor=4
                    key[j][i] = 0;
                }
                    }

            LZW_HASH_CLEAR_LOOP_1:for(int i = 0; i < ARR_SIZE; i++){
                #pragma HLS loop_flatten
                LZW_HASH_CLEAR_LOOP_2:for(int j = 0; j < BUCKET_SIZE; j++){
                #pragma HLS UNROLL factor=2
                    hash_node_ptr[i][j].hash_value = 0;         
                }
            }
            unsigned char substring_array[ARR_SIZE];

            unsigned int  substring_arr_index = 0;

            unsigned char c;
            substring_array[substring_arr_index] = in_stream.read(); // p += chunk->start[0] // Read the first element
            substring_arr_index++;

            int code = 256;
            int write_flag = 0;
            LZW_LOOP_2:for (unsigned int i = 0; i < chunk_length_lzw; i++)
            {
                if (i != chunk_length_lzw - 1){
                    c = in_stream.read();
                    substring_array[substring_arr_index] = c;
                    substring_arr_index++;
                }

                // uint32_t hash_result = MurmurHash2((void*)substring_array,substring_arr_index,1);
                uint32_t hash_result = FNV_32((void*)substring_array,substring_arr_index);


                //MurmurHash
                int res = Find(hash_node_ptr, hash_result);
                if(res < 0 ){
                    res = associative_find(key, value, hash_result);
                }
                
                write_flag = 1;

                if ( res < 0 )
                {   
                    uint32_t hash_result1 = 0;
                    int lookup_value = 0;
                    if(substring_arr_index <= 2){
                        lookup_value = (int)substring_array[0];
                        write_flag = 0;
                    }else{
                        // hash_result1 = MurmurHash2((void*)substring_array,substring_arr_index-1,1);
                        hash_result1 = FNV_32((void*)substring_array,substring_arr_index-1);
                        lookup_value = Find(hash_node_ptr, hash_result1);
                        if(lookup_value < 0){
                            lookup_value = associative_find(key, value, hash_result1);
                        }
                    }
                    lzw_sync.write(0);
                    lzw_out_stream.write(lookup_value); /*function that returns index*/
 
                    int insert_result = Insert(hash_node_ptr, code, hash_result); // Stores hash for p+c
                    if ((insert_result < 0) && counter < 72){
                        counter = associative_insert(key, value, counter, hash_result, code);
                    }

                    code++;
                    substring_arr_index = 0;
                    substring_array[substring_arr_index] = c;
                    substring_arr_index++;
                }

            }

            if(write_flag){
                // uint32_t hash_result2 = MurmurHash2((void*)substring_array,substring_arr_index,1);
                uint32_t hash_result2 = FNV_32((void*)substring_array,substring_arr_index);
                int lookup_value1 = Find(hash_node_ptr, hash_result2);
                if(lookup_value1 < 0){ 
                    lookup_value1 = associative_find(key, value, hash_result2);
                }
                lzw_sync.write(0);
                lzw_out_stream.write(lookup_value1);
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

    COMP_LOOP_1:for(uint64_t chunks =0; chunks < num_chunks_local; chunks++)
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
            
            COMP_LOOP_2:while(!lzw_sync_local)
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

    WRITE_LOOP_1:for(uint64_t chunks = 0; chunks < num_chunks_local; chunks++)
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

            READ_LOOP_2:while(!comp_sync)
            {
                #pragma HLS PIPELINE
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
// #pragma HLS INTERFACE ap_rst port=return

uint64_t num_chunks_local = num_chunks; 

    #pragma HLS DATAFLOW

    hls::stream<unsigned char, 4*1024> out_stream("out_stream");
    hls::stream<int, 4*1024> lzw_out_stream("lzw_out_stream");
    hls::stream<unsigned char, 4*1024> in_stream("in_stream");
    hls::stream<unsigned char, 512> compress_sync("compress_sync");
    hls::stream<unsigned char, 512> lzw_sync("lzw_sync");
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
