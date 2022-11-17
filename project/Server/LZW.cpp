#include "LZW.h"
#include "../fpga/LZW_HW.h"


extern int offset;
extern unsigned char* file;


/* defines for compress function */
#define OUT_SIZE_BITS 8
#define MIN(A,B) ( (A)<(B) ? (A) : (B) );



uint64_t compress(std::vector<int> &compressed_data)
{
    uint64_t Length = 0;
    unsigned char Byte = 0;

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
                file[offset + Length/8 - 1] = Byte;
                Byte = 0;
                rem_outBytes = OUT_SIZE_BITS;
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



//std::vector<int> LZW_encoding(chunk_t* chunk)
uint64_t LZW_encoding(chunk_t* chunk)
{
    //std::cout << "Encoding\n";
    LOG(LOG_INFO_1,"Encoding\n");
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
    //return output_code;

    int header = 0;
    uint64_t compressed_size = ceil(13*output_code.size() / 8.0);
    header |= ( compressed_size <<1); /* size of the new chunk */
    header &= ~(0x1); /* lsb equals 0 signifies new chunk */
    LOG(LOG_DEBUG,"Header written %x\n",header);
    memcpy(&file[offset], &header, sizeof(header)); /* write header to the output file */
    offset += sizeof(header);

    return compress(output_code);
}

