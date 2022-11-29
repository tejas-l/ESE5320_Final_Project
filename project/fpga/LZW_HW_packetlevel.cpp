#include "LZW_HW_packetlevel.h"

// extern int offset;
// extern unsigned char* file;

/* defines for compress function */

#define OUT_SIZE_BITS 8
#define CODE_LENGTH 13
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define ARR_SIZE (8*1024)


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

    //printf("read print1");   
    for (unsigned int i = 0; i < len; i++){
        #pragma HLS PIPELINE
        in_stream.write(data_in[i]);  
    }
    //printf("read print2");

    return;

}

void execute_lzw(   hls::stream<unsigned char> &in_stream,
                    hls::stream<int> &lzw_out_stream,
                    unsigned int len,
                    int *data_output_index){
    
    //printf("executelzw print1");
    
    uint64_t table[ARR_SIZE] = {0};
    unsigned char substring_array[ARR_SIZE] = {0};
    unsigned int  substring_arr_index = 0;
    unsigned int index = 0;

    // for(unsigned int i = 0; i < ARR_SIZE; i++){
    //     #pragma HLS PIPELINE II=1
    //     #pragma HLS UNROLL FACTOR=2
	// 	table[i] = 0;
	// 	data_out[i] = 0;
	// 	substring_array[i] = 0;
    // }

    //printf("executelzw print2");
    
    for (unsigned int i = 0; i <= 255; i++) {
        unsigned char ch = char(i);
        table[i] = MurmurHash2((void*)&ch,1,1); // Initialize Table 
    }

    unsigned char c;
    substring_array[substring_arr_index] = in_stream.read(); // p += chunk->start[0] // Read the first element
    substring_arr_index++;

    int code = 256;
    //printf("executelzw print3");

    for (unsigned int i = 0; i < len; i++) {
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
            index++;

            table[code] = hash_result;  // Stores hash for p+c
            code++;
            substring_arr_index = 0;
            substring_array[substring_arr_index] = c;
            substring_arr_index++;
        }

    }
    int value1 = Murmur_find( MurmurHash2(substring_array,substring_arr_index,1),code,table);  /*function that returns index*/
    lzw_out_stream.write(value1);
    index++;
    *data_output_index = index;
    //printf("executelzw print4");
    
    return;
}

void execute_compress ( hls::stream<int> &lzw_out_stream,
                        hls::stream<unsigned char> &out_stream,
                        int data_output_index,
                        int *Output_length){
    
    uint64_t Length = 0;
    unsigned char Byte = 0;
    uint64_t output_index = 0;
    //printf("executecomp print1");

    uint32_t header = 0;
    uint64_t compressed_size = ceil(13*(data_output_index) / 8.0);
    header = ( compressed_size <<1); /* size of the new chunk */

    out_stream.write(header & 0xFF);
    out_stream.write((header >> 8) & 0xFF);
    out_stream.write((header >> 16) & 0xFF);
    out_stream.write((header >> 24) & 0xFF);

    output_index += 4;

    uint8_t rem_inBytes = CODE_LENGTH;
    uint8_t rem_outBytes = OUT_SIZE_BITS;

    //printf("executecomp print2");
    for(int i=0; i<data_output_index; i++){
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
                // file[offset + Length/8 - 1] = Byte;
                //output_vector.push_back(Byte);
                out_stream.write(Byte); 
                output_index++;
                Byte = 0;
                rem_outBytes = OUT_SIZE_BITS;
            }
        }
    }
    //printf("executecomp print3");
    if(Length % 8 > 0){
        Byte = Byte << (8 - (Length%8));
        Length += (8 - (Length%8));
        // file[offset + Length/8 - 1] = Byte;
        //output_vector.push_back(Byte);
        out_stream.write(Byte); 
        output_index++;
    }

    //return output_vector;
    //printf("executecomp print4");

    *Output_length = output_index;

    return;
    
}

void write (hls::stream<unsigned char> &out_stream,
            int out_len,
            unsigned char* output){
    //printf("write print1");
    for(int i = 0; i < out_len; i++){
        #pragma HLS PIPELINE
        output[i] = out_stream.read();
    }
    //printf("write print2");

    return;

}


void LZW_encoding_HW(unsigned char* data_in, unsigned int len, unsigned char* output, unsigned int* Output_length )//changed output array datatype
{

#pragma HLS INTERFACE m_axi depth=8192 port=data_in bundle=p0
#pragma HLS INTERFACE m_axi depth=8192 port=output bundle=p1

    int data_output_index = 0;
    // int *Output_length =0; 
    int compression_length = 0; 

    //printf("main print");
    #pragma HLS DATAFLOW

    hls::stream<unsigned char, 8*1024> out_stream;
    hls::stream<int, 8*1024> lzw_out_stream;
    hls::stream<unsigned char, 8*1024> in_stream;

    read(data_in, len, in_stream);

    execute_lzw(in_stream, lzw_out_stream, len, &data_output_index);  

    execute_compress(lzw_out_stream, out_stream, data_output_index, &compression_length);

    write(out_stream, compression_length, output); 

    return;
}


