#include "LZW.h"

extern int offset;
extern unsigned char* file;

std::vector<int> LZW_encoding(chunk_t* chunk)
{
    std::cout << "Encoding\n";
    std::unordered_map<std::string, int> table;
    for (int i = 0; i <= 255; i++) {
        std::string ch = "";
        ch += char(i);
        table[ch] = i;
    }
 
    std::string p = "", c = "";
    p += chunk->start[0];
    int code = 256;
    std::vector<int> output_code;
    //std::cout << "String\tOutput_Code\tAddition\n";
    for (unsigned int i = 0; i < chunk->length; i++) {
        if (i != chunk->length - 1)
            c += chunk->start[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        }
        else {
            //std::cout << p << "\t" << table[p] << "\t\t"
            //     << p + c << "\t" << code << std::endl;
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

uint64_t compress(std::vector<int> &compressed_data)
{
    uint64_t Length = 0;
    unsigned char Byte = 0;

    for(int i=0; i< compressed_data.size(); i++){
        int data = compressed_data[i];

        for(int j=0; j<CODE_LENGTH; j++){
            Byte = (Byte << 1) | ((data >> (CODE_LENGTH - 1 - j)) & 1);

            if(++Length % 8 == 0){
                file[offset + Length/8 - 1] = Byte; // write data to temp array
                Byte = 0;
            }
        }
    }

    if(Length % 8 > 0){
        Byte = Byte << (8 - (Length%8));
        Length += (8 - (Length%8));
        file[offset + Length/8 - 1] = Byte;
    }

    return Length/8; // return number of bytes to be written to output file
}