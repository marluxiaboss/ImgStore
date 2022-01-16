/**
 * @file dedup.c
 * @brief deduplicate if two image have same content
 *
 * @author ???
 */


#include "dedup.h"
#include "error.h"
#include "imgStore.h"
#include <string.h>


/**
 * Compares two SHA
 */
int shaCompare(const unsigned char* sha1, const unsigned char* sha2)
{
    // 0 if equal, 1 if greater, -1 if smaller lexicographically.
    int compareValue = 0;
    int i = 0;

    // Loop through letters.
    while (i < SHA256_DIGEST_LENGTH && compareValue == 0) {
        if(sha1[i] > sha2[i]) {
            compareValue = 1;

        } else if(sha1[i] < sha2[i]) {
            compareValue = -1;
        }

        ++i;
    }

    return compareValue;
}

/**
 * Deduplicates the image at the given index if there exists another with the same content
 */
int do_name_and_content_dedup(imgst_file* imgstfile, const uint32_t index)
{

    // If pointer is Null return an error
    M_REQUIRE_NON_NULL(imgstfile);
    M_REQUIRE_NON_NULL(imgstfile->metadata);

    // Check if index is in range
    M_EXIT_IF(imgstfile->header.max_files <= index,
              ERR_INVALID_ARGUMENT, "dedup index out of range", );


    const char* id = imgstfile->metadata[index].img_id;
    const unsigned char* sha = imgstfile->metadata[index].SHA;

    // Loop over valid metadata.
    // If an image has the same name, return an error.
    // If an image has the same SHA(ie. content) de-duplicate.

    size_t i = 0;
    size_t has_content_clone = 0;

    while(i < imgstfile->header.max_files) {
        if(i != index && imgstfile->metadata[i].is_valid) {
            M_EXIT_IF(!strncmp(id, imgstfile->metadata[i].img_id, MAX_IMG_ID),
                      ERR_DUPLICATE_ID, "image with same imgID exists", );

            if(shaCompare(sha, imgstfile->metadata[i].SHA) == 0) {
                memcpy(imgstfile->metadata[index].offset, imgstfile->metadata[i].offset, NB_RES * sizeof(uint64_t));
                memcpy(imgstfile->metadata[index].size, imgstfile->metadata[i].size, NB_RES * sizeof(uint32_t));
                has_content_clone = 1;
            }
        }

        ++i;
    }

    // Tells the function caller that metadata[index] is content-unique
    if(has_content_clone == 0) {
        imgstfile->metadata[index].offset[RES_ORIG] = 0;
    }

    return ERR_NONE;
}
