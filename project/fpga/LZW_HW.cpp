#include "LZW_HW.h"

// extern int offset;
// extern unsigned char* file;

/* defines for compress function */
#define OUT_SIZE_BITS 8
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define ARR_SIZE (8*1024)

unsigned int MurmurHash2(const unsigned char* data, int len, unsigned int seed) {
	// const unsigned char* data = (const unsigned char *) &key;
	const unsigned int m = 0x5bd1e995;
	unsigned int h = seed ^ len;
	switch (len) {
    case 4:
        h ^= data[3] << 32;
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

//std::vector<int> LZW_encoding(chunk_t* chunk)
void LZW_encoding_HW(unsigned char* data_in, unsigned int len, unsigned int* data_out)
{
    unsigned char substring_array[ARR_SIZE]; 
    unsigned int  substring_arr_index = 0;
    unsigned int input_index = 0;
    unsigned int output_index = 0; 
 
    //std::cout << "Encoding\n";
    // LOG(LOG_INFO_1,"Encoding\n");
    // std::unordered_map<std::string, int> table;


    for (unsigned int i = 0; i <= 255; i++) {
        // unsigned char ch = "";
        // ch += char(i);
        table[i] = MurmurHash2((unsigned char*)&i,1,3); // Initialize Table 
    }

    // unsigned char p = "", c = "";
    // p += chunk->start[0];
    unsigned char c;
    substring_array[substring_arr_index++] = data_in[input_index]; // p += chunk->start[0]

    int code = 256;
    //std::cout << "String\tOutput_Code\tAddition\n";
    for (unsigned int i = 0; i < len; i++) {
        #pragma HLS PIPELINE II=1
        if (i != len - 1){
           c = data_in[i + 1];
        }
        
        substring_array[substring_arr_index++] = c;

        unsigned int hash_result =  MurmurHash2(substring_array,substring_arr_index,3);
        // printf("\n\n%d \n", hash_result);
        // for(int i=0; i<substring_arr_index; i++){
        //     printf("%c",substring_array[i]);
        // }
        //printf("\n");
        //MurmurHash
        if (Murmur_find(hash_result,code) == -1 ) {

            //printf("Not Found \n");
            // for(int i=0; i<substring_arr_index-1; i++){
            //     printf("%c",substring_array[i]);
            // }
            //printf("\n");
            // p = p + c;
               //std::cout << p << "\t" << table[p] << "\t\t"
            //     << p + c << "\t" << code << std::endl;

            int out_data = Murmur_find( MurmurHash2(substring_array,substring_arr_index-1,3),code) ; /*function that returns index*/
            //printf("\nout data = %d\n",out_data);
            data_out[output_index++] = out_data;
            table[code] = hash_result;  // Stores hash for p+c
            code++;
            substring_arr_index = 0;
            substring_array[substring_arr_index++] = c;
        }

    }
    // output_code.push_back(table[p]);

    //printf("\n entry 256 = %d\n",table[256]);
    int out_data = Murmur_find( MurmurHash2(substring_array,substring_arr_index,3),code) ; /*function that returns index*/
    data_out[output_index++] = out_data;

    //return output_code;

    // int header = 0;
    // uint64_t compressed_size = ceil(13*output_code.size() / 8.0);
    // header |= ( compressed_size <<1); /* size of the new chunk */
    // header &= ~(0x1); /* lsb equals 0 signifies new chunk */
    // LOG(LOG_DEBUG,"Header written %x\n",header);
    // memcpy(&file[offset], &header, sizeof(header)); /* write header to the output file */
    // offset += sizeof(header);

    // return compress(output_code);
    return;
}


// uint64_t compress(std::vector<int> &compressed_data)
// {
//     uint64_t Length = 0;
//     unsigned char Byte = 0;

//     uint8_t rem_inBytes = CODE_LENGTH;
//     uint8_t rem_outBytes = OUT_SIZE_BITS;

//     for(int i=0; i<compressed_data.size(); i++){
//         int inData = compressed_data[i];
//         rem_inBytes = CODE_LENGTH;

//         while(rem_inBytes){

//             int read_bits = MIN(rem_inBytes,rem_outBytes);
//             Byte = (Byte << read_bits) | ( inData >> (rem_inBytes - read_bits) );

//             rem_inBytes -= read_bits;
//             rem_outBytes -= read_bits;
//             Length += read_bits;
//             inData &= ((0x1 << rem_inBytes) - 1);

//             if(rem_outBytes == 0){
//                 file[offset + Length/8 - 1] = Byte;
//                 Byte = 0;
//                 rem_outBytes = OUT_SIZE_BITS;
//             }
//         }
//     }

//     if(Length % 8 > 0){
//         Byte = Byte << (8 - (Length%8));
//         Length += (8 - (Length%8));
//         file[offset + Length/8 - 1] = Byte;
//     }

//     return Length/8; // return number of bytes to be written to output file
// }
