#include "LZW_HW_packet.h"


/* defines for compress function */

#define OUT_SIZE_BITS 8
#define CODE_LENGTH 13
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define ARR_SIZE (8*1024)

volatile bool lzw_done = 0;
volatile bool compress_done = 0;
volatile int lzw_flag = 0;
volatile int compress_flag = 0;

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

void read  (unsigned char* data_in,
            hls::stream<unsigned char> &in_stream,
            hls::stream<unsigned int> &dup_read,
            unsigned int* chunk_lengths, unsigned int* chunk_numbers,
            unsigned char* chunk_isdups, uint64_t num_chunks){
    uint32_t running_length = 0;
    for(uint64_t i =0; i < num_chunks; i++){
        if(chunk_isdups[i] == 1){
            dup_read.write(chuk_numbers[i]);
        }else{
            for (unsigned int j = 0; j < chunk_lengths[i]; j++){
#pragma HLS PIPELINE
            in_stream.write(data_in[running_length + j]);  
            }
        }
        running_length += chunk_lengths[i];
    }
}


void execute_lzw(   hls::stream<unsigned char> &in_stream,
                    hls::stream<int> &lzw_out_stream,
                    hls::stream<unsigned int> &dup_read,
                    hls::stream<unsigned int> &dup_executelzw,
                    unsigned int* chunk_lengths, unsigned int* chunk_numbers,
                    unsigned char* chunk_isdups, uint64_t num_chunks){

    for(uint64_t chunks =0; chunks < num_chunks; chunks++){
        if(chunk_isdups[chunks] == 1){
            unsigned int dup_chunk = dup_read.read();
            dup_executelzw.write(dup_chunk);
        }else{
            uint64_t table[ARR_SIZE] = {0};
            unsigned char substring_array[ARR_SIZE] = {0};
            unsigned int  substring_arr_index = 0;


            for (unsigned int i = 0; i <= 255; i++) {
                unsigned char ch = char(i);
                table[i] = MurmurHash2((void*)&ch,1,1); // Initialize Table 
            }

            unsigned char c;
            substring_array[substring_arr_index] = in_stream.read(); // p += chunk->start[0] // Read the first element
            substring_arr_index++;

            int code = 256;

            for (unsigned int i = 0; i <= chunk_lengths[chunks] + 1; i++) {
                if( i < chunk_lengths[chunks]){   
                    if (i != chunk_lengths[chunks] - 1){
                        c = in_stream.read();
                        substring_array[substring_arr_index] = c;
                        substring_arr_index++;
                    }


                    uint64_t hash_result =  MurmurHash2((void*)substring_array,substring_arr_index,1);
                    //MurmurHash
                    int res = Murmur_find(hash_result,code,table);
                    if ( res < 0 ) {
                        int value = Murmur_find( MurmurHash2((void*)substring_array,substring_arr_index-1,1),code,table) ;
                        lzw_out_stream.write(value); /*function that returns index*/

                        table[code] = hash_result;  // Stores hash for p+c
                        code++;
                        substring_arr_index = 0;
                        substring_array[substring_arr_index] = c;
                        substring_arr_index++;
                        }
                }            
                else if(i == chunk_lengths[chunks]){
                    int value1 = Murmur_find( MurmurHash2(substring_array,substring_arr_index,1),code,table);  /*function that returns index*/
                    lzw_out_stream.write(value1);
                    // lzw_done = 1;
                }
                if(i == chunk_lengths[chunks] + 1){
                    lzw_out_stream.write(-1);
                }

            }
        }

    }
    
}

void execute_compress ( hls::stream<int> &lzw_out_stream,
                        hls::stream<unsigned char> &out_stream,
                        hls::stream<unsigned int> &dup_executelzw,
                        hls::stream<unsigned int> &dup_executecomp,
                        hls::stream<unsigned char> &compress_sync,
                        unsigned int* chunk_lengths, unsigned int* chunk_numbers,
                        unsigned char* chunk_isdups, uint64_t num_chunks){
    
    for(uint64_t chunks =0; chunks < num_chunks; chunks++){
        if(chunk_isdups[chunks] == 1){
            unsigned int dup_chunk = dup_executelzw.read();
            dup_executecomp.write(dup_chunk);
        }else{
            uint64_t Length = 0;
            unsigned char Byte = 0;
            uint64_t output_index = 0;
            int i = 0;
            uint8_t rem_inBytes = CODE_LENGTH;
            uint8_t rem_outBytes = OUT_SIZE_BITS;
            
            int inData = lzw_out_stream.read();
            while(inData > 0){
                i++;
                //int inData = lzw_out_stream.read();
                rem_inBytes = CODE_LENGTH;

                while(rem_inBytes){

                    int read_bits = MIN(rem_inBytes,rem_outBytes);
                    Byte = (Byte << read_bits) | ( inData >> (rem_inBytes - read_bits) );

                    rem_inBytes -= read_bits;
                    rem_outBytes -= read_bits;
                    Length += read_bits;
                    inData &= ((0x1 << rem_inBytes) - 1);

                    if(rem_outBytes == 0){
                        compress_sync.write(0);
                        out_stream.write(Byte);
                        output_index++;
                        Byte = 0;
                        rem_outBytes = OUT_SIZE_BITS;
                    }
                }
                inData = lzw_out_stream.read();
            }
            if(inData == -1){
                if(Length % 8 > 0){
                    Byte = Byte << (8 - (Length%8));
                    Length += (8 - (Length%8));
                    compress_sync.write(0);
                    out_stream.write(Byte); 
                    output_index++;
                }
                compress_sync.write(1);;
            }
        }
    }
    
}


void write (hls::stream<unsigned char> &out_stream,
            hls::stream<unsigned int> &dup_executecomp,
            hls::stream<unsigned char> &compress_sync,
            unsigned int* chunk_lengths, unsigned int* chunk_numbers,
            unsigned char* chunk_isdups, uint64_t num_chunks,
            unsigned char* output, 
            unsigned int *Output_length){
    int unique_index = 0;
    int out_index = 0;
    for(uint64_t chunks =0; chunks < num_chunks; chunks++){
        if(chunk_isdups[chunks] == 1){
            unsigned int dup_chunk = dup_executecomp.read();
            uint32_t header_dup = 0;
            header_dup |= (dup_chunk<<1);
            header_dup |= (0x1);
            output[out_index++] = header_dup & 0xFF;
            output[out_index++] = (header_dup >> 8) & 0xFF;
            output[out_index++] = (header_dup >> 16) & 0xFF;
            output[out_index++] = (header_dup >> 24) & 0xFF;
        }else{
            unique_index = out_index;
            out_index += 4;
            while(compress_sync.read() == 0){
                output[out_index++] = out_stream.read();
                unique_length++;
                }
            if(compress_sync.read() == 1){
                uint32_t header = 0;
                header = ( unique_length <<1); /* size of the new chunk */
                unique_length = 0;
                output[unique_index++] = header & 0xFF;
                output[unique_index++] = (header >> 8) & 0xFF;
                output[unique_index++] = (header >> 16) & 0xFF;
                output[unique_index++] = (header >> 24) & 0xFF;
             }

        }
}




void LZW_encoding_HW(unsigned char* data_in, unsigned int* chunk_lengths, unsigned int* chunk_numbers,
                     unsigned char* chunk_isdups, unsigned char* output, unsigned int* Output_length )//changed output array datatype
{

#pragma HLS INTERFACE m_axi depth=8192 port=data_in bundle=p0
#pragma HLS INTERFACE m_axi depth=8192 port=output bundle=p1

    #pragma HLS DATAFLOW

    hls::stream<unsigned char, 8*1024> out_stream;
    hls::stream<int, 8*1024> lzw_out_stream;
    hls::stream<unsigned char, 8*1024> in_stream;

    read(data_in, len, in_stream);

    execute_lzw(in_stream, lzw_out_stream, len);  

    execute_compress(lzw_out_stream, out_stream);

    write(out_stream, output, Output_length); 

    return;
}