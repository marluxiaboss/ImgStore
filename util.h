#pragma once

/**
 * @file util.h
 * @brief PPS (CS-212) Tool macros/functions
 *
 * @author Jean-Cédric Chappelier
 * @date 2017-2019
 */

#include <stdint.h> // for uint16_t, uint32_t

/**
 * @brief tag a variable as POTENTIALLY unused, to avoid compiler warnings
 */
#define _unused __attribute__((unused))

/**
 * @brief useful to free pointers to const without warning. Use with care!
 */
#define free_const_ptr(X) free((void*)X)

/**
 * @brief useful to init a variable (typically a struct) directly or through a pointer
 */
#define zero_init_var(X) memset(&X, 0, sizeof(X))
#define zero_init_ptr(X) memset(X, 0, sizeof(*X))

/**
 * @brief useful to have C99 (!) %zu to compile in Windows
 */
#if defined _WIN32  || defined _WIN64
#define SIZE_T_FMT "%u"
#else
#define SIZE_T_FMT "%zu"
#endif

/**
 * @brief convert big-endian 16 bits integer to little-endian.
 *    If already little-endian, return same value.
 */
uint16_t b2l_16(const uint16_t);

/**
 * @brief convert little-endian 16 bits integer to big-endian.
 *    If already big-endian, return same value.
 */
uint16_t l2b_16(const uint16_t);

/**
 * @brief String to uint16_t conversion function
 *
 * @param str a string containing some integer value to be extracted
 * @return converted value in uint16_t format
 */
uint16_t
atouint16(const char* str);

/**
 * @brief String to uint32_t conversion function
 *
 * @param str a string containing some integer value to be extracted
 * @return converted value in uint32_t format
 */
uint32_t
atouint32(const char* str);
