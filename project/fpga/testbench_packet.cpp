#include <bits/stdc++.h>
#include "LZW_HW_packet.h"

#define OUT_SIZE_BITS 8
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define CODE_LENGTH 13

using namespace std;

vector<unsigned char> LZW_SW(string s1, unsigned char* is_dups, uint8_t num_chunks,
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

int main()
{
    string test_string = "the Little Prince Chapter I Once when I was six years old I saw a magnificent picture in a book, called True Stories from Nature, about the primeval forest. It was a picture of a boa constrictor in the act of swallowing an animal. Here is a copy of the drawing. Boa In the book it said: Boa constric";
    //string test_string = "gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGG";
    std::vector<std::string> strings(5);

    // strings[0] = "the Little Prince Chapter I Once when I was six ye";
    // strings[1] = "ars old I saw a magnificent picture in a book, called True Stor";
    // strings[2] = "ies from Nature, about the primeval fore";
    // strings[3] = "st. It was a picture of a boa constrictor in the act of swallowing an ani";
    // strings[5] = "mal. Here is a copy of the drawing. Boa In the book it said: Boa constric";

    uint64_t num_chunks = 5;
    unsigned int chunk_lengths[] = {50, 63, 40, 73, 73};
    unsigned int chunk_numbers[] = {0, 1, 0, 2, 1};
    unsigned char is_dups[] = {0, 0, 1, 0, 1};


    unsigned char* string_s = (uint8_t*)calloc(500,sizeof(uint8_t));
    unsigned char* hw_output_code = (unsigned char*)calloc(8192, sizeof(unsigned char));
    printf("Input length %d\n",test_string.size());

    for(int i = 0; i < test_string.size(); i++)
    {
        string_s[i] = test_string[i];
    }

    uint32_t* output_code_size = (uint32_t*)calloc(1,sizeof(uint32_t));
    LZW_encoding_HW(string_s, chunk_lengths, chunk_numbers, is_dups, num_chunks, hw_output_code, output_code_size);
    
    std::vector<unsigned char> sw_out;
    std::vector<unsigned char> sw_int;
    std::vector<unsigned int> sw_output_code;

    // for(int i =0; i< num_chunks; i++){
      
    //     printf("SW Processing start for string %d\n",i);
    //     sw_output_code = LZW_SW(string_s[i]);
    //     sw_int = compress(sw_output_code);
    //     sw_out.insert(sw_out.end(), sw_int.begin(), sw_int.end());
    //     printf("String %d done processing for SW \n",i);
    // }
    sw_out = LZW_SW(test_string, is_dups, num_chunks, chunk_lengths, chunk_numbers);
    printf("HW out length %d\n",*output_code_size);
    printf("SW out length %d\n", sw_out.size());

    bool Equal = compare_outputs(sw_out, hw_output_code, *output_code_size );
    free(string_s);
    free(hw_output_code);
    free(output_code_size);
    std::cout << "TEST " << (Equal ? "PASSED" : "FAILED") << std::endl;
}
