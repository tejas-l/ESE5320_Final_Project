#include "LZW_HW_packetlevel.h"


/* defines for compress function */

#define OUT_SIZE_BITS 8
#define CODE_LENGTH 13
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define ARR_SIZE (8*1024)

volatile bool lzw_done = 0;
volatile bool compress_done = 0;

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
            unsigned int len,
            hls::stream<unsigned char> &in_stream){

    for (unsigned int i = 0; i < len; i++){
        #pragma HLS PIPELINE
        in_stream.write(data_in[i]);  
    }
}

void execute_lzw(   hls::stream<unsigned char> &in_stream,
                    hls::stream<int> &lzw_out_stream,
                    unsigned int len){
    
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

    for (unsigned int i = 0; i <= len; i++) {
        if( i < len){   
            if (i != len - 1){
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
        else{
            int value1 = Murmur_find( MurmurHash2(substring_array,substring_arr_index,1),code,table);  /*function that returns index*/
            lzw_out_stream.write(value1);
            lzw_done = 1;
        }


    }

}

void execute_compress ( hls::stream<int> &lzw_out_stream,
                        hls::stream<unsigned char> &out_stream){
    
    uint64_t Length = 0;
    unsigned char Byte = 0;
    uint64_t output_index = 0;
    int i = 0;
    uint8_t rem_inBytes = CODE_LENGTH;
    uint8_t rem_outBytes = OUT_SIZE_BITS;

    while((!lzw_done) || (!lzw_out_stream.empty())){
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

            if(rem_outBytes == 0){
                out_stream.write(Byte); 
                output_index++;
                Byte = 0;
                rem_outBytes = OUT_SIZE_BITS;
            }
        }
    }
    if(lzw_done){
        if(Length % 8 > 0){
            Byte = Byte << (8 - (Length%8));
            Length += (8 - (Length%8));
            out_stream.write(Byte); 
            output_index++;
        }
        compress_done = 1;
    }
}

void write (hls::stream<unsigned char> &out_stream,
            unsigned char* output,
            unsigned int *Output_length){
    int i = 4;
    while((!compress_done) || (!out_stream.empty())){
        output[i++] = out_stream.read();
    }
    if(compress_done){
        *Output_length = i;
        uint32_t header = 0;
        uint32_t length = i-4;
        header = ( length <<1); /* size of the new chunk */

        output[0] = header & 0xFF;
        output[1] = (header >> 8) & 0xFF;
        output[2] = (header >> 16) & 0xFF;
        output[3] = (header >> 24) & 0xFF;
    }
}


void LZW_encoding_HW(unsigned char* data_in, unsigned int len, unsigned char* output, unsigned int* Output_length )//changed output array datatype
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



