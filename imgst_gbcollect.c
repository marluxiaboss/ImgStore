/**
 * @file imgst_gbcollect.c
 * @brief implementation of do_gbcollect()
 *
 * @author ???
 */

#include "error.h"
#include "imgStore.h"
#include "image_content.h"
#include <stdio.h> // for remove and rename


/**
 * Handles garbage collecting ie removes the deleted images by moving the existing ones
 */
int do_gbcollect (const char *imgst_path, const char *imgst_tmp_bkp_path)
{

    // Null-pointer
    M_REQUIRE_NON_NULL(imgst_path);
    M_REQUIRE_NON_NULL(imgst_tmp_bkp_path);

    // Open imgStore
    imgst_file imgstfile_orig;
    M_EXIT_IF_ERR(do_open(imgst_path, "rb+", &imgstfile_orig));

    // Initialize the backup imgStore
    imgst_file imgstfile_temp;
    imgstfile_temp.header.max_files = imgstfile_orig.header.max_files;
    memcpy(imgstfile_temp.header.res_resized, imgstfile_orig.header.res_resized,
           2 * (NB_RES - 1) * sizeof(uint16_t));

    // Create the backup imgStore
    M_EXIT_IF_ERR_DO_SOMETHING(do_create(imgst_tmp_bkp_path, &imgstfile_temp),
                               do_close(&imgstfile_temp));

    // Loop over all the metadata of the imgStore where we do garbage collecting
    // and if the image is valid, read it and insert it in the new imgStore
    size_t temp_idx = 0;
    img_metadata* metadata_orig = imgstfile_orig.metadata;

    for (size_t i = 0; i < imgstfile_orig.header.max_files; ++i) {
        if (validMetadataIndex(i, &imgstfile_orig) == 0) {
            char* image_buffer = NULL;
            size_t image_size = 0;

            // Read an original valid image
            M_EXIT_IF_ERR_DO_SOMETHING(
            do_read(metadata_orig[i].img_id, RES_ORIG, &image_buffer, (uint32_t*)&image_size, &imgstfile_orig),
            do_close(&imgstfile_orig); do_close(&imgstfile_temp));

            // Insert it into a buffer
            M_EXIT_IF_ERR_DO_SOMETHING(
            do_insert(image_buffer, image_size, metadata_orig[i].img_id, &imgstfile_temp),
            do_close(&imgstfile_orig); do_close(&imgstfile_temp));

            // Find	its location in the temporary imgstfile
            M_EXIT_IF_ERR_DO_SOMETHING(findMetadataIndex(&temp_idx, metadata_orig[i].img_id, &imgstfile_temp),
                                       do_close(&imgstfile_orig); do_close(&imgstfile_temp));

            // Loop over the resolutions of the image and if it exists call lazily resize
            if (metadata_orig[i].offset[RES_THUMB] != INIT_OFFSET) {
                M_EXIT_IF_ERR_DO_SOMETHING(lazily_resize(RES_THUMB, &imgstfile_temp, temp_idx),
                                           do_close(&imgstfile_orig); do_close(&imgstfile_temp));

            }

            if (metadata_orig[i].offset[RES_SMALL] != INIT_OFFSET) {
                M_EXIT_IF_ERR_DO_SOMETHING(lazily_resize(RES_SMALL, &imgstfile_temp, temp_idx),
                                           do_close(&imgstfile_orig); do_close(&imgstfile_temp));
            }
        }
    }

    // Closes
    do_close(&imgstfile_orig);
    do_close(&imgstfile_temp);

    // Make the backup imgStore the new imgStore and delete the old imgStore
    M_EXIT_IF_ERR(remove(imgst_path));
    M_EXIT_IF_ERR(rename(imgst_tmp_bkp_path, imgst_path));


    return ERR_NONE;

}
