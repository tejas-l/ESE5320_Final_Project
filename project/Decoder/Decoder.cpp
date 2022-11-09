#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdint.h>

#define CODE_LENGTH (13)

typedef std::vector<std::string> code_table;
typedef std::vector<std::string> chunk_list;

static code_table Code_table;

static std::ifstream Input;
static size_t Input_position;

static int Read_code(void)
{
  static unsigned char Byte;

  int Code = 0;
  int Length = CODE_LENGTH;
  for (int i = 0; i < Length; i++)
  {
    if (Input_position % 8 == 0)
      Byte = Input.get();
    Code = (Code << 1) | ((Byte >> (7 - Input_position % 8)) & 1);
    Input_position++;
  }
  return Code;
}

static const std::string Decompress(size_t Size)
{
  Input_position = 0;

  Code_table.clear();
  for (int i = 0; i < 256; i++)
    Code_table.push_back(std::string(1, (char) i));

  int Old = Read_code();
  std::string Symbol(1, Old);
  std::string Output = Symbol;
  while (Input_position / 8 < Size - 1)
  {
    //printf("%d\n", Old);
    int New = Read_code();
    std::string Symbols;
    //printf("1\n");
    if (New >= (int) Code_table.size()){
      Symbols = Code_table[Old] + Symbol;
      //printf("2\n");
    }
    else{
      Symbols = Code_table[New];
      //printf("3\n");
    }
    Output += Symbols;
    Symbol = std::string(1, Symbols[0]);
    //printf("4\n");
    Code_table.push_back(Code_table[Old] + Symbol);
    Old = New;
  }

  return Output;
}

int main(int Parameter_count, char * Parameters[])
{
  printf("Hello 1\n");
  if (Parameter_count < 3)
  {
    std::cout << "Usage: " << Parameters[0] << " <Compressed file> <Decompressed file>\n";
    return EXIT_SUCCESS;
  }

  printf("Hello 2\n");
  Input.open(Parameters[1], std::ios::binary);
  if (!Input.good())
  {
    std::cerr << "Could not open input file.\n";
    return EXIT_FAILURE;
  }

  printf("Hello 3\n");
  std::ofstream Output(Parameters[2], std::ios::binary);
  if (!Output.good())
  {
    std::cerr << "Could not open output file.\n";
    return EXIT_FAILURE;
  }

  std::cout << "entering the while loop" << std::endl;

  chunk_list Chunks;
  int i = 0;
  int num_chunks = 0;
  while (true)
  {
    uint32_t Header;
    Input.read((char *) &Header, sizeof(int32_t));
    printf("header = %d chunk no = %d\n",Header,num_chunks);
    if (Input.eof())
      break;

    if ((Header & 1) == 0)
    {
      int Chunk_size = Header >> 1;
      const std::string & Chunk = Decompress(Chunk_size);
      Chunks.push_back(Chunk);
      std::cout << "Decompressed chunk of size " << Chunk.length() << ".\n";
      Output.write(&Chunk[0], Chunk.length());
    }
    else
    {
     int Location = Header >> 1;
      if (Location<Chunks.size()) {  // defensive programming to avoid out-of-bounds reference
          const std::string & Chunk = Chunks[Location];
          std::cout << "Found chunk of size " << Chunk.length() << " in database.\n";
          Output.write(&Chunk[0], Chunk.length());
	  }
      else
      {
       std::cerr << "Location " << Location << " not in database of length " << Chunks.size() << " ignoring block.  Likely encoder error.\n";
       }
    }
    i++;
    num_chunks++;
  }

  return EXIT_SUCCESS;
}
