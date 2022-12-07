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

    //unsigned char test[] = "kkabc tors swallow their prey whole, without chewing it. After that they are not able to move, and they sleep through the six months that they need for digestion. I pondered deeply, then, over the adventures of the jungle. And after some work with a colored pencil I succeeded in making my first drawing. My Drawing Number One. The Little Prince Chapter I\nOnce when I was six years old I saw a magnificent picture in a book, called True Stories from Nature, about the primeval forest. It was a picture of a boa constrictor in the act of swallowing an animal. Here is a copy of the drawing. Boa In the book it said: Boa constric";
    
    unsigned char test[] = "The Autobiography of Benjamin Franklin edited by Charles Eliot presented\nby Project Gutenberg\n\nThis eBook is for the use of anyone anywhere at no cost and with almost\nno restrictions whatsoever. You may copy it, give it away or re-use it\nunder the terms of the Project Gutenberg License included with this\neBook or online at www.gutenberg.net\n\nTitle: The Autobiography of Benjamin Franklin\nEditor: Charles Eliot\nRelease Date: May 22, 2008 [EBook #148]\nLast Updated: July 2016\nLanguage: English\n\nProduced by Project Gutenberg. HTML version by Robert Homa.\n\n*** START OF THIS PROJECT GUTENBERG EBOOK THE AUTOBIOGRAPHY OF BENJAMIN\nFRANKLIN ***\n\nTitle: The Autobiography of Benjamin Franklin\n\nAuthor: Benjamin Franklin\n\nFirst Released: August 4, 1995 [Ebook: #148]\n[Last updated: August 2, 2016]\n\nLanguage: English\n\n*** START OF THIS PROJECT GUTENBERG EBOOK AUTOBIOGRAPHY OF BENJAMIN\nFRANKLIN ***\n\nTHE AUTOBIOGRAPHY OF BENJAMIN FRANKLIN\n\nThe Harvard Classics\n\nWITH INTRODUCTION AND NOTES\n\nEDITED BY\n\nCHARLES W ELLIOT LLD\n\nP F COLLIER & SON COMPANY\nNEW YORK\n1909\n\n\n\n\nNavigation\n\n    Letter from Mr. Abel James.\n    Publishes the first number of \"Poor Richard's Almanac.\n    Proposes a Plan of Union for the colonies\n    Chief events in Franklin's life.\n\nINTRODUCTORY NOTE\n\nBenjamin Franklin was born in Milk Street, Boston, on January 6, 1706.\nHis father, Josiah Franklin, was a tallow chandler who married twice,\nand of his seventeen children Benjamin was the youngest son. His\nschooling ended at ten, and at twelve he was bound apprentice to his\nbrother James, a printer, who published the \"New England Courant.\" To\nthis journal he became a contributor, and later was for a time its\nnominal editor. But the brothers quarreled, and Benjamin ran away, going\nfirst to New York, and thence to Philadelphia, where he arrived in\nOctober, 1723. He soon obtained work as a printer, but after a few\nmonths he was induced by Governor Keith to go to London, where, finding\nKeith's promises empty, he again worked as a compositor till he was\nbrought back to Philadelphia by a merchant named Denman, who gave him a\nposition in his business. On Denman's death he returned to his former\ntrade, and shortly set up a printing house of his own from which he\npublished \"The Pennsylvania Gazette,\" to which he contributed many\nessays, and which he made a medium for agitating a variety of local\nreforms. In 1732 he began to issue his famous \"Poor Richard's Almanac\"\nfor the enrichment of which he borrowed or composed those pithy\nutterances of worldly wisdom which are the basis of a large part of his\npopular reputation. In 1758, the year in which he ceased writing for the\nAlmanac, he printed in it \"Father Abraham's Sermon,\" now regarded as the\nmost famous piece of literature produced in Colonial America.\n\nMeantime Franklin was concerning himself more and more with public\naffairs. He set forth a scheme for an Academy, which was taken up later\nand finally developed into the University of Pennsylvania; and he\nfounded an \"American Philosophical Society\" for the purpose of enabling\nscientific men to communicate their discoveries to one another. He\nhimself had already begun his electrical researches, which, with other\nscientific inquiries, he carried on in the intervals of money-making and\npolitics to the end of his life. In 1748 he sold his business in order\nto get leisure for study, having now acquired comparative wealth; and in\na few years he had made discoveries that gave him a reputation with the\nlearned throughout Europe. In politics he proved very able both as an\nadministrator and as a controversialist; but his record as an\noffice-holder is stained by the use he made of his position to advance\nhis relatives. His most notable service in home politics was his reform\nof the postal system; but his fame as a statesman rests chiefly on his\nservices in connection with the relations of the Colonies with Great\nBritain, and later with France. In 1757 he was sent to England to\nprotest against the influence of the Penns in the government of the\ncolony, and for five years he remained there, striving to enlighten the\npeople and the ministry of England as to Colonial conditions. On his\nreturn to America he played an honorable part in the Paxton affair,\nthrough which he lost his seat in the Assembly; but in 1764 he was again\ndespatched to England as agent for the colony, this time to petition the\nKing to resume the government from the hands of the proprietors. In\nLondon he actively opposed the proposed Stamp Act, but lost the credit\nfor this and much of his popularity through his securing for a friend\nthe office of stamp agent in America. Even his effective work in helping\nto obtain the repeal of the act left him still a suspect; but he\ncontinued his efforts to present the case for the Colonies as the\ntroubles thickened toward the crisis of the Revolution. In 1767 he\ncrossed to France, where he was received with honor; but before his\nreturn home in 1775 he lost his position as postmaster through his share\nin divulging to Massachusetts the famous letter of Hutchinson and\nOliver. On his arrival in Philadelphia he was chosen a member of the\nContinental Congress, and in 1777 he was despatched to France as\ncommissioner for the United States. Here he remained till 1785, the\nfavorite of French society; and with such success did he conduct the\naffairs of his country that when he finally returned he received a place\nonly second to that of Washington as the champion of American\nindependence. He died on April 17, 1790.\n\nThe first five chapters of the Autobiography were composed in England in\n1771, continued in 1784-5, and again in 1788, at which date he brought\nit down to 1757. After a most extraordinary series of adventures, the\noriginal form of the manuscript was finally printed by Mr. John Bigelow,\nand is here reproduced in recognition of its value as a picture of one\nof the most notable personalities of Colonial times, and of its\nacknowledged rank as one of the great autobiographies of the world.\n\n\n\n\nBENJAMIN FRANKLIN\n\nHIS AUTOBIOGRAPHY\n\n1706-1757\n\nTwyford, at the Bishop of St. Asaph's, [1] 1771.\n\nDear Son: I have ever had pleasure in obtaining any little anecdotes of\nmy ancestors. You may remember the inquiries I made among the remains of\nmy relations when you were with me in England, and the journey I\nundertook for that purpose. Imagining it may be equally agreeable to [2]\nyou to know the circumstances of my life, many of which you are yet\nunacquainted with, and expecting the enjoyment of a week's uninterrupted\nleisure in my present country retirement, I sit down to write them for\nyou. To which I have besides some other inducements. Having emerged from\nthe poverty and obscurity in which I was born and bred, to a state of\naffluence and some degree of reputation in the world, and having gone so\nfar through life with a considerable share of felicity, the conducing\nmeans I made use of, which with the blessing of God so well succeeded,\nmy posterity may like to know, as they may find some of them suitable to\ntheir own situations, and therefore fit to be imitated.\n\n[1] The country-seat of Bishop Shipley, the good bishop, as Dr. Franklin\nused to style him.--B.\n\n[2] After the words \"agreeable to\" the words \"some of\" were interlined\nand afterward effaced.--B.\n\nThat felicity, when I reflected on it, has induced me sometimes to say,\nthat were it offered to my choice, I should have no objection to a\nrepetition of the same life from its beginning, only asking the\nadvantages authors have in a second edition to correct some faults of\nthe first. So I might, besides correcting the faults, change some\nsinister accidents and events of it for others more favorable. But\nthough this were denied, I should still accept the offer. Since such a\nrepetition is not to be expected, the next thing most like living one's\nlife over again seems to be a recollection of that life, and to make\nthat recollection as durable as possible by putting it down in writing.\n\nHereby, too, I shall indulge the inclination so natural in old men, to\nbe talking of themselves and their own past actions; and I shall indulge\nit without being tiresome to others, who, through respect to age, might\nconceive themselves obliged to give me a hearing, since this may be read\n";
        // unsigned char test[] = "aaaaa"; 
    uint64_t num_chunks_1 = 1;
    unsigned int chunk_lengths_1[] = {5000, 2500, 2500, 300, 40, 80, 80, 400};
    unsigned int chunk_numbers_1[] = {20, 5, 18, 40, 50, 20, 40, 4};
    unsigned char is_dups_1[] = {0, 0, 0, 0, 1, 0, 0, 1};

    std::vector<unsigned char> sw_out_1;
    
    unsigned char* hw_output_code_1 = (unsigned char*)calloc(10000, sizeof(unsigned char));

    uint32_t* output_code_size_1 = (uint32_t*)calloc(1,sizeof(uint32_t));

    LZW_encoding_HW(test, chunk_lengths_1, chunk_numbers_1, is_dups_1, num_chunks_1, hw_output_code_1, output_code_size_1);


    sw_out_1 = LZW_SW(&test[2], is_dups_1, num_chunks_1, chunk_lengths_1, chunk_numbers_1);

    if (sw_out_1.size() == *output_code_size_1){
        printf("Lengths are equal\n");
        printf("HW length = %d SW length = %d \n", *output_code_size_1, sw_out_1.size());
    }else{
        printf("Lengths don't match HW length = %d SW length = %d \n", *output_code_size_1, sw_out_1.size());
    }
    bool Equal = compare_outputs(sw_out_1, hw_output_code_1, *output_code_size_1 );
    std::cout << "TEST " << (Equal ? "PASSED" : "FAILED") << std::endl;

    free(hw_output_code_1);
    free(output_code_size_1);

}
