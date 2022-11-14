
#include "SHA_block.h"

extern int offset;
extern unsigned char* file;

std::string SHA_384(chunk_t *chunk)
{
    byte shaSum[SHA256_DIGEST_SIZE];
    byte buffer[1024];
    /*fill buffer with data to hash*/

    wc_Sha256 sha;
    wc_InitSha256(&sha);

    wc_Sha256Update(&sha, (const unsigned char*)chunk->start, chunk->length);  /*can be called again and again*/
    wc_Sha256Final(&sha, shaSum);

    // printf("\n");
    // for(int x=0; x < SHA256_DIGEST_SIZE; x++){
    //     printf("%02x",shaSum[x]);
    // }
    // printf("\n");

    LOG(LOG_DEBUG,"\n");
    for(int x=0; x < SHA256_DIGEST_SIZE; x++){
        LOG(LOG_DEBUG,"%02x",shaSum[x]);
    }
    LOG(LOG_DEBUG,"\n");


    return std::string(reinterpret_cast<char*>(shaSum),SHA256_DIGEST_SIZE);

}