/**
 * @file imgst_read.c
 * @brief implementation of do_read for imgstore
 *
 * @author ???
 */

#include "imgStore.h"
#include "image_content.h"
#include "error.h"

#include <stdlib.h> // for calloc
#include <stdint.h> // for uint8_t

/**
 * Reads the content of an image from a imgStore
 */
int do_read(const char* img_id, const int resolution, char** image_buffer,
            uint32_t* image_size, imgst_file* imgstfile)
{

    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);
    M_REQUIRE_NON_NULL(imgstfile);

    // Check if valid resolution code
    M_EXIT_IF(resolution != RES_SMALL && resolution != RES_THUMB && resolution != RES_ORIG,
              ERR_RESOLUTIONS, "called do_read with an invalid error code", );

    // Find the metadata index for the img_id.
    size_t idx = 0;
    M_EXIT_IF_ERR(findMetadataIndex(&idx, img_id, imgstfile));

    // Resize if the image doesn't exist in the requested resolution
    if (resolution != RES_ORIG && imgstfile->metadata[idx].offset[resolution] == INIT_OFFSET) {
        M_EXIT_IF_ERR(lazily_resize(resolution, imgstfile, idx));
    }

    // Let image_size point to the location of the size value
    *image_size = imgstfile->metadata[idx].size[resolution];

    // Let image_buffer point to the image data
    void* buffer = NULL;
    M_EXIT_IF_NULL(buffer = calloc(1, *image_size), *image_size);

    // Read the 1 image from the file
    fseek(imgstfile->file, imgstfile->metadata[idx].offset[resolution], SEEK_SET);
    M_EXIT_IF_ERR_DO_SOMETHING((fread(buffer, *image_size, 1, imgstfile->file) == 1) ? ERR_NONE : ERR_IO,
                               FREE_DEREF(buffer));

    *image_buffer = buffer;

    return ERR_NONE;
}
