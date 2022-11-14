#ifndef _SHA_BLOCK_H
#define _SHA_BLOCK_H

#include "wolfssl/options.h"
#include <wolfssl/wolfcrypt/sha256.h>
#include "../common/common.h"

std::string SHA_384(chunk_t *chunk);

#endif
