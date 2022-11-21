#include <bits/stdc++.h>
#include "LZW_HW.h"

#define OUT_SIZE_BITS 8
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define CODE_LENGTH 13

using namespace std;

vector<unsigned int> LZW_SW(string s1)
{
    cout << "Encoding\n";
    unordered_map<string, int> table;
    for (int i = 0; i <= 255; i++) {
        string ch = "";
        ch += char(i);
        table[ch] = i;
    }
 
    string p = "", c = "";
    p += s1[0];
    int code = 256;
    vector<unsigned int> output_code;
    cout << "String\tOutput_Code\tAddition\n";
    for (int i = 0; i < s1.length(); i++) {
        if (i != s1.length() - 1)
            c += s1[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        }
        else {
            cout << p << "\t" << table[p] << "\t\t"
                 << p + c << "\t" << code << endl;
            output_code.push_back(table[p]);
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
    }
    cout << p << "\t" << table[p] << endl;
    output_code.push_back(table[p]);
    return output_code;
}

std::vector<unsigned char> compress(std::vector<unsigned int> &compressed_data)
{
    uint64_t Length = 0;
    unsigned char Byte = 0;
    std::vector<unsigned char> output_vector; 

    uint8_t rem_inBytes = CODE_LENGTH;
    uint8_t rem_outBytes = OUT_SIZE_BITS;

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


bool compare_outputs(vector<unsigned int> sw_output_code, unsigned int *hw_output_code, uint32_t length)
{
    bool Equal = true;
    for (int i=0; i<length; i++)
    {
        if(sw_output_code[i] != hw_output_code[i])
        {
            cout<<"Element="<< i <<" SW out code: "<<sw_output_code[i]<<" HW out code: "<<hw_output_code[i]<<'\n';
            Equal = false;
        }
    }
    return Equal;
}

int main()
{
    // string test_string ="The Little Prince Chapter I Once when I was six years old I saw a magnificent picture in a book, called True Stories from Nature, about the primeval forest. It was a picture of a boa constrictor in the act of swallowing an animal. Here is a copy of the drawing. Boa In the book it said: Boa constric";
    string test_string = "gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGabc gdgserge yy66ey   &&**Ggg *GGGGGGGGGGGGGGG";
   
    unsigned char* string_s = (uint8_t*)calloc(500,sizeof(uint8_t));
    unsigned int* hw_output_code = (unsigned int*)calloc(8192, sizeof(unsigned int));

    for(int i = 0; i < test_string.size(); i++)
    {
        string_s[i] = test_string[i];
    }

    uint32_t* output_code_size = (uint32_t*)calloc(1,sizeof(uint32_t));

    LZW_encoding_HW(string_s,test_string.size(), hw_output_code, output_code_size);
    vector<unsigned int> sw_output_code = LZW_SW(test_string);

    //std::vector<unsigned char> sw_out = compress(sw_output_code);

    bool Equal = compare_outputs(sw_output_code, hw_output_code, *output_code_size );
    std::cout << "TEST " << (Equal ? "PASSED" : "FAILED") << std::endl;
}
