#include "LZW_HW.h"

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


void LZW_encoding_HW(unsigned char* data_in, unsigned int len, unsigned int* data_out, unsigned int* Output_length )//changed output array datatype
{


#pragma HLS INTERFACE m_axi depth=8192 port=data_in bundle=p0
#pragma HLS INTERFACE m_axi depth=8192 port=data_out bundle=p1


    uint64_t table[ARR_SIZE] = {0};
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
        table[i] = MurmurHash2((void*)&ch,1,1); // Initialize Table 
    }

    unsigned char c;
    substring_array[substring_arr_index] = data_in[input_index]; // p += chunk->start[0]
    substring_arr_index++;

    int code = 256;

    for (unsigned int i = 0; i < len; i++) {
        if (i != len - 1){
            c = data_in[i + 1];
            substring_array[substring_arr_index] = c;
            substring_arr_index++;
        }


        uint64_t hash_result =  MurmurHash2((void*)substring_array,substring_arr_index,1);
        //MurmurHash
        int res = Murmur_find(hash_result,code,table);
        if ( res < 0 ) {

            data_out[data_output_index] = Murmur_find( MurmurHash2((void*)substring_array,substring_arr_index-1,1),code,table) ; /*function that returns index*/
            data_output_index++;

            table[code] = hash_result;  // Stores hash for p+c
            code++;
            substring_arr_index = 0;
            substring_array[substring_arr_index] = c;
            substring_arr_index++;
        }

    }


    data_out[data_output_index] = Murmur_find( MurmurHash2(substring_array,substring_arr_index,1),code,table);  /*function that returns index*/

    data_output_index++;


    *Output_length = data_output_index;

    return;
}


