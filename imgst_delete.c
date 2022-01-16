/**
 * @file imgst_delete.c
 * @brief imgStore library: do_delete implementation.
 *
 * @author ???
 */

#include "imgStore.h"

#include <string.h>

int do_delete(const char* img_id, imgst_file* imgstfile)
{
    // Null-pointer checks
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgstfile);
    M_REQUIRE_NON_NULL(imgstfile->metadata);

    // If there are no valid images to delete
    M_EXIT_IF(imgstfile->header.num_files <= 0, ERR_FILE_NOT_FOUND,
              "No valid image to delete", );

    // Find the metadata index for the img_id.
    size_t idx = 0;
    M_EXIT_IF_ERR(findMetadataIndex(&idx, img_id, imgstfile));

    /// Deletion

    // "Delete" the file
    imgstfile->metadata[idx].is_valid = EMPTY;

    // Update the file's copy of the metadata
    M_EXIT_IF_ERR(updateMetadata(idx, imgstfile));

    // Update the header if deletion worked
    imgstfile->header.num_files -= 1;		// The number of valid files decrements
    imgstfile->header.imgst_version += 1;	// The version of the imgStore increments
    M_EXIT_IF_ERR(updateHeader(imgstfile));

    return ERR_NONE;
}








