/**
 * @util.c
 * @brief Some tool functions for CS-212
 *
 * @author J.-C. Chappelier, EPFL
 * @date 08.2019
 */

#include <stdint.h> // for uint16_t, uint32_t
#include <errno.h>
#include <inttypes.h> // strtoumax()

// See util.h
uint16_t b2l_16(const uint16_t input)
{
    const unsigned char * const bytes = (const unsigned char *) &input;
    return (uint16_t) (((uint16_t) bytes[1] << 8) | (uint16_t) bytes[0]);
}

// See util.h
uint16_t l2b_16(const uint16_t input)
{
    const unsigned char * const bytes = (const unsigned char *) &input;
    return (uint16_t) (((uint16_t) bytes[0] << 8) | (uint16_t) bytes[1]);
}

/********************************************************************//**
 * Tool functions for string to uint<N>_t conversion. See util.h
 ********************************************************************** */
#define define_atouintN(N) \
uint ## N ## _t \
atouint ## N(const char* str) \
{ \
    char* endptr; \
    errno = 0; \
    uintmax_t val = strtoumax(str, &endptr, 10); \
    if (errno == ERANGE || val > UINT ## N ## _MAX \
        || endptr == str || *endptr != '\0') { \
        errno = ERANGE; \
        return 0; \
    } \
    return (uint ## N ## _t) val; \
}

define_atouintN(16)
define_atouintN(32)
