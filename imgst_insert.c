/**
 * @file imgst_insert.c
 * @brief imgStore library: do_insert implementation
 *
 * @author ???
 */
#include "imgStore.h"
#include "dedup.h"
#include "error.h"
#include "image_content.h"
#include <stdlib.h> // for realloc
#include <openssl/sha.h> // for SHA256_DIGEST_LENGTH and SHA256()

int do_insert(const char* image_buffer, size_t image_size, const char* img_id, imgst_file* imgstfile)
{

    // Null-pointer checks
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgstfile);
    M_REQUIRE_NON_NULL(imgstfile->metadata);

    // Check if database is full
    M_EXIT_IF(imgstfile->header.num_files >= imgstfile->header.max_files,
              ERR_FULL_IMGSTORE, "insert with full imgstore", );

    // Find index of empty slot (ie. isValid == 0) which is guarenteed to exist!
    size_t index = 0;

    while(index < imgstfile->header.max_files
          && imgstfile->metadata[index].is_valid != 0) {

        ++index;
    }

    /// Initialize the metadata for the image to insert.

    // Get SHA and ID and check against all other images for duplicates
    unsigned char sha[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)image_buffer, image_size, sha);
    memcpy(imgstfile->metadata[index].SHA, sha, SHA256_DIGEST_LENGTH * sizeof(unsigned char));
    memcpy(imgstfile->metadata[index].img_id, img_id, (MAX_IMG_ID + 1) * sizeof(char));

    // De-dup if content-duplicate, or exit if name-duplicate
    M_EXIT_IF_ERR(do_name_and_content_dedup(imgstfile, index));

    // If content-original then the previous function sets offset[RES_ORIG] to 0
    if(imgstfile->metadata[index].offset[RES_ORIG] == 0) {

        // If the image content is new, add it to end of file
        if (fseek(imgstfile->file, 0, SEEK_END) != 0) {
            return ERR_IO;
        }

        const long offset_endfile = ftell(imgstfile->file);

        // Initialize the metadata to 0 in case old content is still there.
        imgstfile->metadata[index].offset[RES_SMALL] = 0;
        imgstfile->metadata[index].offset[RES_THUMB] = 0;
        imgstfile->metadata[index].size[RES_THUMB] = 0;
        imgstfile->metadata[index].size[RES_THUMB] = 0;

        // Update offset metadata field with the location in file of the newly inserted image
        imgstfile->metadata[index].offset[RES_ORIG] = offset_endfile;

        // Append the original image to the store
        size_t num_image_written = 0;
        num_image_written += fwrite(image_buffer, image_size, 1, imgstfile->file);

        if(num_image_written != 1) {
            return ERR_IO;
        }
    }

    // Get resolution of the image and update the metadatum accordingly
    uint32_t height = 0, width = 0;
    M_EXIT_IF_ERR(get_resolution(&height, &width, image_buffer, image_size));

    imgstfile->metadata[index].res_orig[0] = width;
    imgstfile->metadata[index].res_orig[1] = height;

    // Rest: metadata fields that don't depend on being a duplicate (or overlap)
    imgstfile->metadata[index].is_valid = NON_EMPTY;
    imgstfile->metadata[index].size[RES_ORIG] = (uint32_t)image_size;

    // Update header
    imgstfile->header.imgst_version += 1;
    imgstfile->header.num_files += 1;

    // Write change of header and metadata to disk
    M_EXIT_IF_ERR(updateHeader(imgstfile));
    M_EXIT_IF_ERR(updateMetadata(index, imgstfile));

    return ERR_NONE;
}

