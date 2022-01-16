#pragma once

/**
 * @file dedup.h
 * @brief deduplicate if two image have same content
 *
 * @author ???
 */

#include "imgStore.h"


/**
 * @brief compares two SHA strings
 *
 * @param sha1 the first SHA
 * @param sha2 the second SHA
 */
int shaCompare(const unsigned char* sha1, const unsigned char* sha2);


/**
 * @brief Deduplicates the image at the given index if there exists another with the same content
 *
 * @param imgstfile the imgStoreFile
 * @param index the index of the images
 */
int do_name_and_content_dedup(imgst_file* imgstfile, const uint32_t index);
