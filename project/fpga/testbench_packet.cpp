#include <bits/stdc++.h>
#include "LZW_HW_packet.h"
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <strings.h>

#define OUT_SIZE_BITS 8
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define CODE_LENGTH 13

using namespace std;

vector<unsigned char> LZW_SW(unsigned char* s1, unsigned char* is_dups, uint8_t num_chunks,
                            unsigned int* chunk_lengths, unsigned int* chunk_numbers){

    
    vector<unsigned char> sw_out;
    unsigned int sw_out_idx = 0;
    int sw_length = 0;

    for( int chunks = 0 ; chunks < num_chunks ; chunks++){

        if (is_dups[chunks] == 1){
            uint32_t header_dup = 0;
            header_dup |= (chunk_numbers[chunks]<<1);
            header_dup |= (0x01);
            sw_out.push_back((char)(header_dup & 0xFF));
            sw_out_idx++;
            sw_out.push_back((char)((header_dup >> 8) & 0xFF));
            sw_out_idx++;
            sw_out.push_back((char)((header_dup >> 16) & 0xFF));
            sw_out_idx++;
            sw_out.push_back((char)((header_dup >> 24) & 0xFF));
            sw_out_idx++;
            printf("Duplicate header for string %d is written at %d \n", chunks, (sw_out_idx - 4));
        }
        else{
            cout << "Encoding\n";
            vector<unsigned int> output_code;
            unordered_map<string, int> table;          
            for (int i = 0; i <= 255; i++) {
                string ch = "";
                ch += char(i);
                table[ch] = i;
            }
        
            string p = "", c = "";
            p += s1[sw_length];
            int code = 256;
            //cout << "String\tOutput_Code\tAddition\n";
            for (int i = 0; i < chunk_lengths[chunks]; i++) {
                if (i != chunk_lengths[chunks] - 1)
                    c += s1[sw_length + i + 1];
                if (table.find(p + c) != table.end()) {
                    p = p + c;
                }
                else {
                    //cout << p << "\t" << table[p] << "\t\t"
                    //    << p + c << "\t" << code << endl;
                    output_code.push_back(table[p]);
                    table[p + c] = code;
                    code++;
                    p = c;
                }
                c = "";
            }
            //cout << p << "\t" << table[p] << endl;
            output_code.push_back(table[p]);

            //compress function bitpacking
            uint64_t Length = 0;
            unsigned char Byte = 0;
             
            uint8_t rem_inBytes = CODE_LENGTH;
            uint8_t rem_outBytes = OUT_SIZE_BITS;

            uint32_t output_size = output_code.size();

            std::cout <<"SW out length = " << output_code.size() << std::endl;



            uint32_t compressed_data_length= ceil(13*(output_size) / 8.0);
            printf("SW unique header for string %d which has length %d\n",chunks, compressed_data_length);

            uint32_t header = compressed_data_length << 1;

            sw_out.push_back((char)(header & 0xFF));
            sw_out_idx++;
            sw_out.push_back((char)((header>>8) & 0xFF));
            sw_out_idx++;
            sw_out.push_back((char)((header>>16) & 0xFF));
            sw_out_idx++;
            sw_out.push_back((char)((header>>24) & 0xFF));
            sw_out_idx++;
            printf("Unique header for string %d is written at %d \n", chunks, (sw_out_idx - 4));
            
            

            for(int i=0; i<output_code.size(); i++){
                int inData = output_code[i];
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
                        sw_out.push_back(Byte); 
                        sw_out_idx++;
                        Byte = 0;
                        rem_outBytes = OUT_SIZE_BITS;
                    }
                }
            }

            if(Length % 8 > 0){
                Byte = Byte << (8 - (Length%8));
                Length += (8 - (Length%8));
                // file[offset + Length/8 - 1] = Byte;
                sw_out.push_back(Byte);
                sw_out_idx++;
            }


            
        }

    sw_length += chunk_lengths[chunks];
    }

    
    return sw_out;
}

std::vector<unsigned char> compress(std::vector<unsigned int> &compressed_data)
{
    uint64_t Length = 0;
    unsigned char Byte = 0;
    std::vector<unsigned char> output_vector; 

    uint8_t rem_inBytes = CODE_LENGTH;
    uint8_t rem_outBytes = OUT_SIZE_BITS;

    uint32_t output_size = compressed_data.size();

    std::cout <<"SW out length = " << compressed_data.size() << std::endl;



    uint32_t compressed_data_length= ceil(13*(output_size) / 8.0);

    uint32_t header = compressed_data_length << 1;
    
    output_vector.push_back((char)(header & 0xFF));
    output_vector.push_back((char)((header>>8) & 0xFF));
    output_vector.push_back((char)((header>>16) & 0xFF));
    output_vector.push_back((char)((header>>24) & 0xFF));

    for(int i=0; i<compressed_data.size(); i++){
        int inData = compressed_data[i];
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
                output_vector.push_back(Byte); 
                Byte = 0;
                rem_outBytes = OUT_SIZE_BITS;
            }
        }
    }

    if(Length % 8 > 0){
        Byte = Byte << (8 - (Length%8));
        Length += (8 - (Length%8));
        // file[offset + Length/8 - 1] = Byte;
        output_vector.push_back(Byte);
    }

    return output_vector;  // return number of bytes to be written to output file
}


bool compare_outputs(vector<unsigned char> sw_output_code, unsigned char *hw_output_code, uint32_t length)
{
    bool Equal = true;
    for (int i=0; i<length; i++)
    {
        if(sw_output_code[i] != hw_output_code[i])
        {
            //cout<<"Element="<< i <<" SW out code: "<<sw_output_code[i]<<" HW out code: "<<hw_output_code[i]<<std::endl;
            printf("Element = %d  SW out: %x  HW out: %x \n",i,sw_output_code[i],hw_output_code[i]);

            Equal = false;
        }
    }
    return Equal;
}

// int main()
// {
//     string test_string = "the Little Prince Chapter I Once when I was six years old I saw a magnificent picture in a book, called True Stories from Nature, about the primeval forest. It was a picture of a boa constrictor in the act of swallowing an animal. Here is a copy of the drawing. Boa In the book it said: Boa constric";
//     //string test_string = "gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGG";
//     // FILE *fp = fopen("Franklin.txt","r");
//     //filestream new_file;
//     // new_file.open("Franklin.txt",ios::in);
//     // if(!new_file){
//     //     printf("File not read!!\n");
//     // }else{
//     //     char ch ;
//     //     while(!new_file.eof())
//     // }

//     //unsigned char *test =  (unsigned char*)calloc(8192,sizeof(unsigned char));
//     // unsigned char test[8192] = {0};

//     // if(fread(test,sizeof(char), 8192, fp)==NULL){
//     //     printf("reading error");
//     // }

//     // fread(test,sizeof(char), 8192, fp);
//     // fclose(fp);
//     // for (int i=0; i<8192; i++){
//     //     printf("index %d -> char %c\n",i,test[i]);
//     // }

//     uint64_t num_chunks = 5;
//     unsigned int chunk_lengths[] = {50, 63, 40, 73, 73};
//     unsigned int chunk_numbers[] = {0, 1, 2, 3, 4};
//     unsigned char is_dups[] = {0, 0, 0, 0, 0};

    // uint64_t num_chunks_1 = 26;
    // unsigned int chunk_lengths_1[] = {266,218,997,107,48,738,158,27,263,909,495,710,50,250,183,82,140,864,436,70,241,61,240,88,528,23};
    // unsigned int chunk_numbers_1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25};
    // unsigned char is_dups_1[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0 ,0 ,0 ,0 , 0};
    
//     uint64_t num_chunks_2 = 27;
//     unsigned int chunk_lengths_2[] = {507, 123, 313, 33, 500, 538,433, 881, 296, 73, 289, 350, 337, 739, 42, 200, 99, 200, 68, 159, 47, 248, 28, 946, 409, 166, 168};
//     unsigned int chunk_numbers_2[] = {26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,48, 49, 50, 51, 52};
//     unsigned char is_dups_2[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0 ,0 ,0 ,0 , 0, 0};

//     //unsigned char* string_s = (uint8_t*)calloc(500,sizeof(uint8_t));
//     unsigned char* hw_output_code_1 = (unsigned char*)calloc(2000, sizeof(unsigned char));
//     //unsigned char* hw_output_code_2 = (unsigned char*)calloc(20000, sizeof(unsigned char));
//     // unsigned char hw_output_code_1[16000] = {0};
//     //unsigned char hw_output_code_2[16000] = {0};
//     //printf("Input length %d\n",test_string.size());
//     string string_s_1;
//     // string string_s_2;
//     for(int i = 0; i < 8192; i++)
//     {
//         string_s_1 +=  [i];
//         // string_s_2 += test[8192 + i];
//     }


//     uint32_t* output_code_size_1 = (uint32_t*)calloc(1,sizeof(uint32_t));
//     //uint32_t* output_code_size_2 = (uint32_t*)calloc(1,sizeof(uint32_t));
    
//     LZW_encoding_HW(test, chunk_lengths_1, chunk_numbers_1, is_dups_1, num_chunks_1, hw_output_code_1, output_code_size_1);
    

    

//     // uint32_t output_code_size_1 = 0;
//     // uint32_t output_code_size_2 = 0;
//     std::vector<unsigned char> sw_out_1;
//     //std::vector<unsigned char> sw_out_2;
//     //sw_out_1 = LZW_SW(string_s_1, is_dups_1, num_chunks_1, chunk_lengths_1, chunk_numbers_1);
//     //sw_out_2 = LZW_SW(string_s_2, is_dups_2, num_chunks_2, chunk_lengths_2, chunk_numbers_2);

//     printf("software lzw done\n");

//     printf("Second run of HW\n");
//     //LZW_encoding_HW((unsigned char*)&test[8192], chunk_lengths_2, chunk_numbers_2, is_dups_2, num_chunks_2, hw_output_code_2, output_code_size_2);

    
//     // LZW_encoding_HW(test, chunk_lengths_1, chunk_numbers_1, is_dups_1, num_chunks_1, hw_output_code_1, output_code_size_1);
    

//     printf("Running check for 1st chunk\n");
//     printf("Running check for 2nd chunk\n");
//     //bool Equal = compare_outputs(sw_out_1, hw_output_code_1, *output_code_size_1 );
//     // if(Equal){
//     //     printf("packet 1 passed\n");
//     // }else{
//     //     printf("packet 1 passed\n");
//     // }
//     // printf("Running check for 2nd chunk\n");
//     //Equal = compare_outputs(sw_out_2, hw_output_code_2, *output_code_size_2 );
//     // free(test);
//     free(hw_output_code_1);
//     free(output_code_size_1);
//     // free(hw_output_code_2);
//     // free(output_code_size_2);
    
//     //std::cout << "TEST " << (Equal ? "PASSED" : "FAILED") << std::endl;
// }


int main(){

    unsigned char test[] = "abc tors swallow their prey whole, without chewing it. After that they are not able to move, and they sleep through the six months that they need for digestion. I pondered deeply, then, over the adventures of the jungle. And after some work with a colored pencil I succeeded in making my first drawing. My Drawing Number One. The Little Prince Chapter I\nOnce when I was six years old I saw a magnificent picture in a book, called True Stories from Nature, about the primeval forest. It was a picture of a boa constrictor in the act of swallowing an animal. Here is a copy of the drawing. Boa In the book it said: Boa constric";
    
    uint64_t num_chunks_1 = 4;
    unsigned int chunk_lengths_1[] = {51, 150, 271, 154};
    unsigned int chunk_numbers_1[] = {20, 5, 18, 40};
    unsigned char is_dups_1[] = {0, 0, 1, 0};

    std::vector<unsigned char> sw_out_1;
    
    unsigned char* hw_output_code_1 = (unsigned char*)calloc(1000, sizeof(unsigned char));

    uint32_t* output_code_size_1 = (uint32_t*)calloc(1,sizeof(uint32_t));

    LZW_encoding_HW(test, chunk_lengths_1, chunk_numbers_1, is_dups_1, num_chunks_1, hw_output_code_1, output_code_size_1);


    sw_out_1 = LZW_SW(test, is_dups_1, num_chunks_1, chunk_lengths_1, chunk_numbers_1);


    bool Equal = compare_outputs(sw_out_1, hw_output_code_1, *output_code_size_1 );
    std::cout << "TEST " << (Equal ? "PASSED" : "FAILED") << std::endl;

    free(hw_output_code_1);
    free(output_code_size_1);

}