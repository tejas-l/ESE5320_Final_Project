#include <bits/stdc++.h>
#include "LZW_HW.h"

using namespace std;
vector<int> encoding(string s1)
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
    vector<int> output_code;
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

int main()
{
    unsigned int test_output [1000]; 
    string s = "WYS*WYGWYS*WYSWYSG";
    unsigned char HW_input[19] =  "WYS*WYGWYS*WYSWYSG";
    vector<int> output_code = encoding(s);
    LZW_encoding_HW(HW_input,s.size(),test_output); 

    int Equal = 1; 
    printf("\n");
    printf("%d\n",output_code.size());
    for(int i =0; i<output_code.size();i++){

        printf("%d\n",test_output[i]);
        if(output_code[i] != test_output[i]){
            Equal = 0; 
            //break;
        }
    }


    std::cout << "TEST " << (Equal ? "PASSED" : "FAILED") << std::endl;

     return Equal ? 0 : 1;

    // cout << "Output Codes are: ";
    // for (int i = 0; i < output_code.size(); i++) {
    //     cout << output_code[i] << " ";
    // }
    // cout << endl;
    // decoding(output_code);
}