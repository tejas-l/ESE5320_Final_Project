#include "LZW_HW.h"

// extern int offset;
// extern unsigned char* file;

/* defines for compress function */

#define OUT_SIZE_BITS 8
#define CODE_LENGTH 13
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define ARR_SIZE (8*1024)
//#define INTER_ARRAY_SIZE (13*ARR_SIZE/8)

unsigned int MurmurHash2(const unsigned char* data, int len, unsigned int seed) {
	// const unsigned char* data = (const unsigned char *) &key;
	const unsigned int m = 0x5bd1e995;
	unsigned int h = seed ^ len;
   while (len >= 4)
   {
       unsigned char k = data[len-1];

       k *= m;
		k ^= k >> 24;
		k *= m;
		h *= m;
		h ^= k;
		len--;
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

static unsigned int table[ARR_SIZE];

int Murmur_find(unsigned int hash_result,int code){

    
    for (int i = 0 ; i< code; i++){
        #pragma HLS PIPELINE II=1

        if (hash_result == table[i]){

            return i; 
        }

    }
    return -1; 
}


void LZW_encoding_HW(unsigned char* data_in, unsigned int len, unsigned char* output, unsigned int* Output_length )//changed output array datatype
{
    unsigned char substring_array[ARR_SIZE]; 
    unsigned int  substring_arr_index = 0;
    unsigned int input_index = 0;
    unsigned int data_output_index = 0; 
    unsigned int data_out[ARR_SIZE];



    for (unsigned int i = 0; i <= 255; i++) {
        table[i] = MurmurHash2((unsigned char*)&i,1,3); // Initialize Table 
    }

    unsigned char c;
    substring_array[substring_arr_index++] = data_in[input_index]; // p += chunk->start[0]

    int code = 256;

    for (unsigned int i = 0; i < len - 1; i++) {
        #pragma HLS PIPELINE II=1
        if (i != len - 1){
           c = data_in[i + 1];
        }
        
        substring_array[substring_arr_index++] = c;


        unsigned int hash_result =  MurmurHash2(substring_array,substring_arr_index,3);
        //MurmurHash
        if (Murmur_find(hash_result,code) == -1 ) {
            int out_data = Murmur_find( MurmurHash2(substring_array,substring_arr_index-1,3),code) ; /*function that returns index*/
            data_out[data_output_index++] = out_data;
            table[code] = hash_result;  // Stores hash for p+c
            code++;
            substring_arr_index = 0;
            substring_array[substring_arr_index++] = c;
        }

    }


    int out_data = Murmur_find( MurmurHash2(substring_array,substring_arr_index,3),code) ; /*function that returns index*/
    data_out[data_output_index++] = out_data;

    //return output_code;
    unsigned int output_index = 0;
    uint32_t header = 0;
    uint32_t compressed_size = ceil(13*data_output_index / 8.0);
    //header |= ( compressed_size <<1); /* size of the new chunk */

    ((unsigned uint32_t *)output)[0] = ( compressed_size <<1); //Writing the header
    

    output_index += 4;

    unsigned int Length = 0;
    unsigned char Byte = 0;

    unsigned char rem_inBytes = CODE_LENGTH;
    unsigned char rem_outBytes = OUT_SIZE_BITS;


    for(int i = 0; i < data_output_index; i++){
        int inData = data_out[i];
        rem_inBytes = CODE_LENGTH;

        while(rem_inBytes){

            int read_bits = MIN(rem_inBytes,rem_outBytes);
            Byte = (Byte << read_bits) | ( inData >> (rem_inBytes - read_bits) );

            rem_inBytes -= read_bits;
            rem_outBytes -= read_bits;
            Length += read_bits;
            inData &= ((0x1 << rem_inBytes) - 1);
            if(rem_outBytes == 0){
                output[output_index++] = Byte;
                Byte = 0;
                rem_outBytes = OUT_SIZE_BITS;
            }
        }
    }
    
    if(Length % 8 > 0){
        Byte = Byte << (8 - (Length%8));
        output[output_index++] = Byte;
    }

    *Output_length = output_index;
    return;
}


