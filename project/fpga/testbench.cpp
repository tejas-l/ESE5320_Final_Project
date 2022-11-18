#include <bits/stdc++.h>
#include "LZW_HW.h"


using namespace std;

/* defines for compress function */
#define OUT_SIZE_BITS 8
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );
#define CODE_LENGTH 13



std::vector<unsigned char> compress_test(std::vector<int> &compressed_data)
{
    std::vector<unsigned char> output_vector;
    uint64_t Length = 0;
    unsigned char Byte = 0;

    uint8_t rem_inBytes = CODE_LENGTH;
    uint8_t rem_outBytes = OUT_SIZE_BITS;


    // for(int i=0; i<10;i++){
    //     printf("Index = %d SW lzw res %d\n",i,compressed_data[i]);
    // }

    // for(int i=10; i>0;i--){
    //     printf("Index = %d SW lzw res %d\n",compressed_data.size()-i,compressed_data[compressed_data.size()-i]);
    // }

    printf("compressed_data.size() = %d\n",compressed_data.size());
    for(int i=0; i<compressed_data.size(); i++){
        int inData = compressed_data[i];
        rem_inBytes = CODE_LENGTH;

        while(rem_inBytes){

            int read_bits = MIN(rem_inBytes,rem_outBytes);
            Byte = (Byte << read_bits) | ( inData >> (rem_inBytes - read_bits) );

            rem_inBytes -= read_bits;
            rem_outBytes -= read_bits;
            // if(i>compressed_data.size()-10){
            //     printf("\nSW Length loop = %d\n",Length);
            // }
            Length += read_bits;
            inData &= ((0x1 << rem_inBytes) - 1);

            if(rem_outBytes == 0){
                //file[offset + Length/8 - 1] = Byte;
                output_vector.push_back(Byte);
                Byte = 0;
                rem_outBytes = OUT_SIZE_BITS;
            }
        }
    }

    printf("SW length = %d\n",Length);
    if(Length % 8 > 0){
        Byte = Byte << (8 - (Length%8));
        Length += (8 - (Length%8));
        //file[offset + Length/8 - 1] = Byte;
        output_vector.push_back(Byte);
    }

    printf("SW length = %d\n",Length);

    return output_vector; // return number of bytes to be written to output file
}


vector<int> encoding(string s1)
{
    unordered_map<string, int> table;
    for (int i = 0; i <= 255; i++) {
        string ch = "";
        ch += char(i);
        table[ch] = i;
    }
 
    string p = "", c = "";
    p += s1[0];
    int code = 256;
    vector<int> output_code;
    //cout << "String\tOutput_Code\tAddition\n";
    for (int i = 0; i < s1.length(); i++) {
        if (i != s1.length() - 1)
            c += s1[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        }
        else {
            //cout << p << "\t" << table[p] << "\t\t"
            //     << p + c << "\t" << code << endl;
            output_code.push_back(table[p]);
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
    }
    output_code.push_back(table[p]);
    return output_code;
}


int main()
{    
    
    //unsigned char input[300] = "The Little Prince Chapter I Once when I was six years old I saw a magnificent picture in a book, called True Stories from Nature, about the primeval forest. It was a picture of a boa constrictor in the act of swallowing an animal. Here is a copy of the drawing. Boa In the book it said: Boa constric";

    unsigned int *test_output = (unsigned int *)malloc(8192*sizeof(unsigned int));
    //string s = "The Little Prince Chapter I Once when I was six years old I saw a magnificent picture in a book, called True Stories from Nature, about the primeval forest. It was a picture of a boa constrictor in the act of swallowing an animal. Here is a copy of the drawing. Boa In the book it said: Boa constric";
    unsigned char *input = (unsigned char *)malloc(300*sizeof(unsigned char));//"WYS*WYGWYS*WYSWYSG";  
    string s = "WYS*WYGWYS*WYSWYSG";
    //memcpy(input,"WYS*WYGWYS*WYSWYSG",s.size());
    for(int i=0; i<s.size();i++){
        input[i] = s[i];
    }

    

    //unsigned char HW_input[19] =  "WYS*WYGWYS*WYSWYSG";

    //std::vector<int> encoding_out= encoding(s);
    //std::vector<unsigned char> output_code = compress_test(encoding_out);
    std::vector<int> output_code = encoding(s);

    printf("testbench sending input to hw\n");
    for(int i=0; i<s.size(); i++){
        printf("%c",input[i]);
    }
    unsigned int *HW_output_length = (unsigned int *)malloc(1*sizeof(unsigned int));
    LZW_encoding_HW(input,s.size(),test_output, HW_output_length);
    printf("\nChecking Starts\n");
    int Equal = 1; 

    if((*HW_output_length)!= output_code.size()){
        Equal = 0;
        printf("\nLengths of SW and HW not matching");
        printf("\nSW length = %d\n",output_code.size());
        printf("\nHW length = %d\n",*HW_output_length);
    }
    
    for(int i = 0; i<output_code.size();i++){
        if(output_code[i] != test_output[i]){
            Equal = 0;
            printf("mismatch at %d\n",i);
            printf("SW value = %d\tHW value = %d\n",output_code[i],test_output[i]);
            //break;
        }
    }

    //printf("last_value SW = %d\tlast_value HW=%d\n",output_code[output_code.size()-1],test_output[HW_output_length-1]);
    


    std::cout << "TEST " << (Equal ? "PASSED" : "FAILED") << std::endl;


    // free(test_output);
    // free(input);
    // free(HW_output_length);

    return !Equal;

    // cout << "Output Codes are: ";
    // for (int i = 0; i < output_code.size(); i++) {
    //     cout << output_code[i] << " ";
    // }
    // cout << endl;
    // decoding(output_code);
}
