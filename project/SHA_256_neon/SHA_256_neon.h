




/* sha256-arm.c - ARMv8 SHA extensions using C intrinsics     */
/*   Written and placed in public domain by Jeffrey Walton    */
/*   Based on code from ARM, and by Johannes Schneiders, Skip */
/*   Hovsmith and Barry O'Rourke for the mbedTLS project.     */

/* For some reason we need to use the C++ compiler. Otherwise       */
/* all the intrinsics functions, like vsha256hq_u32, are missing.   */
/* GCC118 on the compile farm with GCC 4.8.5 suffers the issue.     */
/* g++ -DTEST_MAIN -march=armv8-a+crypto sha256-arm.c -o sha256.exe */

/* Visual Studio 2017 and above supports ARMv8, but its not clear how to detect */
/* it or use it at the moment. Also see http://stackoverflow.com/q/37244202,    */
/* http://stackoverflow.com/q/41646026, and http://stackoverflow.com/q/41688101 */


// #if defined(__arm__) || defined(__aarch32__) || defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM)
// # if defined(__GNUC__)
// #  include <stdint.h>
// # endif
// # if defined(__ARM_NEON) || defined(_MSC_VER) || defined(__GNUC__)
// #  include <arm_neon.h>
// # endif
// /* GCC and LLVM Clang, but not Apple Clang */
// # if defined(__GNUC__) && !defined(__apple_build_version__)
// #  if defined(__ARM_ACLE) || defined(__ARM_FEATURE_CRYPTO)
// #   include <arm_acle.h>
// #  endif
// # endif
// #endif  /* ARM Headers */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <iostream>

#if defined(__arm__) || defined(__aarch32__) || defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM)
# if defined(__GNUC__)
#  include <stdint.h>
# endif
# if defined(__ARM_NEON) || defined(_MSC_VER) || defined(__GNUC__)
#  include <arm_neon.h>
# endif
/* GCC and LLVM Clang, but not Apple Clang */
# if defined(__GNUC__) && !defined(__apple_build_version__)
#  if defined(__ARM_ACLE) || defined(__ARM_FEATURE_CRYPTO)
#   include <arm_acle.h>
#  endif
# endif
#endif  /* ARM Headers */