#include "LZW_HW.h"

// extern int offset;
// extern unsigned char* file;

/* defines for compress function */

#define OUT_SIZE_BITS 8
#define CODE_LENGTH 13
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define ARR_SIZE (8*1024)
//#define INTER_ARRAY_SIZE (13*ARR_SIZE/8)

uint32_t MurmurHash2(const unsigned char* key, int len, unsigned int seed) {
	
    const unsigned char* data = (const unsigned char *) key;
    const unsigned int m = 0x5bd1e995;
	uint32_t h = seed ^ len;
    
    while (len >= 4)
    {
        size_t k = data[len-1];

        k *= m;
		k ^= k >> 24;
		k *= m;
		h *= m;
		h ^= k;
        data += 4;
		len -= 4;
    }

    switch (len) {
    case 3:
        h ^= data[2] << 16;
    case 2:
        h ^= data[1] << 8;
    case 1:
        h ^= data[0];
        h *= m;
    };
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
}

//static uint32_t table[ARR_SIZE];

int Murmur_find(unsigned int hash_result,int code, uint32_t* table){

    for (int i = 0 ; i< code; i++){
        if (hash_result == table[i]){
            return i;
        }

    }
    return -1;
}


void LZW_encoding_HW(unsigned char* data_in, unsigned int len, unsigned int* data_out, unsigned int* Output_length )//changed output array datatype
{

#pragma HLS INTERFACE m_axi depth=8192 port=data_in bundle=p0
#pragma HLS INTERFACE m_axi depth=8192 port=data_out bundle=p1


    uint32_t table[ARR_SIZE] = {0};
    unsigned char substring_array[ARR_SIZE] = {0};
    unsigned int  substring_arr_index = 0;
    unsigned int input_index = 0;
    unsigned int data_output_index = 0; 
    //unsigned int data_out[ARR_SIZE];

    for(unsigned int i = 0; i < ARR_SIZE; i++){
        #pragma HLS PIPELINE II=1
        #pragma HLS UNROLL FACTOR=2
		table[i] = 0;
		//data_out[i] = -1;
		substring_array[i] = 0;
    }

//    printf("\n");
//    for(int i=0; i<len; i++){
//    	printf("%c",data_in[i]);
//    }
//    printf("\n");

    for (unsigned int i = 0; i <= 255; i++) {
        unsigned char ch = char(i);
        table[i] = MurmurHash2(&ch,1,1); // Initialize Table 
    }

    unsigned char c;
    substring_array[substring_arr_index++] = data_in[input_index]; // p += chunk->start[0]

    int code = 256;

    for (unsigned int i = 0; i < len-1; i++) {
        if (i != len - 1){
            c = data_in[i + 1];
        }

        substring_array[substring_arr_index++] = c;


        unsigned int hash_result =  MurmurHash2(substring_array,substring_arr_index,1);
        //MurmurHash
        if (Murmur_find(hash_result,code,table) == -1 ) {
            int out_data = Murmur_find( MurmurHash2(substring_array,substring_arr_index-1,1),code,table) ; /*function that returns index*/
            data_out[data_output_index++] = out_data;
            table[code] = hash_result;  // Stores hash for p+c
            code++;
            substring_arr_index = 0;
            substring_array[substring_arr_index++] = c;
        }

    }


    int out_data = Murmur_find( MurmurHash2(substring_array,substring_arr_index,1),code,table) ; /*function that returns index*/
    data_out[data_output_index++] = out_data;

    // //return output_code;
    // unsigned int output_index = 0;
    // uint32_t header = 0;
    // uint32_t compressed_size = ceil(13*data_output_index / 8.0);
    // //header |= ( compressed_size <<1); /* size of the new chunk */

    // ((uint32_t *)output)[0] = ( compressed_size <<1); //Writing the header
    

    // output_index += 4;

    // unsigned int Length = 0;
    // unsigned char Byte = 0;

    // unsigned char rem_inBytes = CODE_LENGTH;
    // unsigned char rem_outBytes = OUT_SIZE_BITS;


    // for(int i = 0; i < data_output_index; i++){
    //     int inData = data_out[i];
    //     rem_inBytes = CODE_LENGTH;

    //     while(rem_inBytes){

    //         int read_bits = MIN(rem_inBytes,rem_outBytes);
    //         Byte = (Byte << read_bits) | ( inData >> (rem_inBytes - read_bits) );

    //         rem_inBytes -= read_bits;
    //         rem_outBytes -= read_bits;
    //         Length += read_bits;
    //         inData &= ((0x1 << rem_inBytes) - 1);
    //         if(rem_outBytes == 0){
    //             output[output_index++] = Byte;
    //             Byte = 0;
    //             rem_outBytes = OUT_SIZE_BITS;
    //         }
    //     }
    // }
    
    // if(Length % 8 > 0){
    //     Byte = Byte << (8 - (Length%8));
    //     output[output_index++] = Byte;
    // }

    Output_length[0] = data_output_index;

    return;
}


