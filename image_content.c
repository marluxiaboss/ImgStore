/**
 * @file image_content.c
 * @brief imgStore library: lazily_resize implementation
 *
 * @author ???
 */

#include "imgStore.h"
#include "image_content.h"
#include "error.h"

#include <vips/vips.h>
#include <stdlib.h>

/**
 * Read a vips image from a file. Responsibility of caller to free buffer later.
 */
int read_vips_from_file(void** buffer, const size_t size, VipsImage** image, FILE* file)
{

    // Null-pointer check
    M_REQUIRE_NON_NULL(file);

    // Intermediate buffer to read the file
    M_EXIT_IF_NULL(*buffer = calloc(1, size), size);

    if (fread(*buffer, size, 1, file) != 1) {
        FREE_DEREF(*buffer);
        return ERR_IO;
    }

    // Loading the buffer into a VipsImage as a jpeg
    if(vips_jpegload_buffer(*buffer, size, image, NULL)) {
        FREE_DEREF(*buffer);
        return ERR_IMGLIB;
    }

    return ERR_NONE;
}
/**
 * Write a vips image to a file. Requires buffer with enough allocated memory.
 */
int write_vips_to_file(void** buffer, size_t* size, VipsImage** image, FILE* file)
{

    // Null-pointer checks
    M_REQUIRE_NON_NULL(image);
    M_REQUIRE_NON_NULL(file);

    // VipsImage -> Buffer
    if (vips_jpegsave_buffer(*image, buffer, size, NULL)) {
        FREE_DEREF(*buffer);
        g_object_unref(*image); *image = NULL;
        return ERR_IMGLIB;
    }

    // Buffer -> File
    if (fwrite(*buffer, *size, 1, file) != 1) {
        FREE_DEREF(*buffer);
        return ERR_IO;
    }

    return ERR_NONE;
}


/**
 * Helper method to calculate the ratio between the original and resized image
 */
double shrink_value(const VipsImage *image, int max_resized_width, int max_resized_height)
{
    const double h_shrink = (double) max_resized_width  / (double) image->Xsize ;
    const double v_shrink = (double) max_resized_height / (double) image->Ysize ;
    return h_shrink > v_shrink ? v_shrink : h_shrink ;
}

/**
 * Creates a resized image and appends it to the imgStore file.
 */
int lazily_resize(const int res_code, imgst_file* imgstfile, const size_t idx)
{

    // Null-pointer checks
    M_REQUIRE_NON_NULL(imgstfile);
    M_REQUIRE_NON_NULL(imgstfile->metadata);

    // Don't resize if the res_code is for the original resolution
    M_EXIT_IF(res_code == RES_ORIG, ERR_NONE, "no need to resize original", );

    // Check if valid resolution code
    M_EXIT_IF(res_code != RES_SMALL && res_code != RES_THUMB, ERR_RESOLUTIONS,
              "invalid resolution code %d", res_code);

    // Don't resize an already deleted image or an image which cannot be found
    M_EXIT_IF_ERR(validMetadataIndex(idx, imgstfile));

    // Check if the image already exists under the requested resolution
    // We check whether the file position in offset[] is already initialized
    M_EXIT_IF(imgstfile->metadata[idx].offset[res_code] != INIT_OFFSET, ERR_NONE,
              "the resized image already exists", );

    /// Create new variant of image in requested resolution

    // Move to position in file of original image
    fseek(imgstfile->file, imgstfile->metadata[idx].offset[RES_ORIG], SEEK_SET);

    // Load image file into a single VipsImage using a buffer
    void* buffer = NULL;
    VipsImage* original_image = NULL;
    const size_t orig_size = imgstfile->metadata[idx].size[RES_ORIG];
    M_EXIT_IF_ERR_DO_SOMETHING(read_vips_from_file(&buffer, orig_size, &original_image, imgstfile->file),
                               FREE_DEREF(buffer);
                               g_object_unref(original_image); original_image = NULL);

    // Constant used by shrink_value to determine the resize ration
    const double ratio = shrink_value(original_image,
                                      imgstfile->header.res_resized[2 * res_code],
                                      imgstfile->header.res_resized[2 * res_code + 1]);

    // Compute the resized image with the ratio
    VipsImage* resized_image = NULL;
    vips_resize(original_image, &resized_image, ratio, NULL);

    // The original VipsImage* is no longer needed.
    g_object_unref(original_image);

    // Append the content to the end of the imgStore file
    if (fseek(imgstfile->file, 0, SEEK_END) != 0) {
        FREE_DEREF(buffer);
        g_object_unref(resized_image); resized_image = NULL;
        return ERR_IO;
    }

    const long offset = ftell(imgstfile->file); // This is the new offset.

    // Write the resized VipsImage to file. Buffer has more than enough allocated memory.
    size_t resized_size = 0;
    M_EXIT_IF_ERR_DO_SOMETHING(write_vips_to_file(&buffer, &resized_size, &resized_image, imgstfile->file),
                               FREE_DEREF(buffer);
                               g_object_unref(resized_image); resized_image = NULL);

    // The resized VipsImage* and its buffer is no longer needed.
    g_object_unref(resized_image); resized_image = NULL;
    FREE_DEREF(buffer);

    // Update the metadata in memory and on disk
    imgstfile->metadata[idx].offset[res_code] = offset;
    imgstfile->metadata[idx].size[res_code] = resized_size;
    M_EXIT_IF_ERR(updateMetadata(idx, imgstfile));

    return ERR_NONE;
}

/**
 * Gets the resolution of a JPEG image
 */
int get_resolution(uint32_t* height, uint32_t* width, const char* image_buffer, const size_t image_size)
{

    // Null-pointer check
    if (image_buffer == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    // Load the buffer into a (single) VipsImage
    VipsImage* vipsimage = NULL;

    if(vips_jpegload_buffer((void*)image_buffer, image_size, &vipsimage, NULL)) {
        return ERR_IMGLIB;
    } // Careful to not free buffer while original is in use!

    // Set height and width pointers
    *height = (uint32_t) vipsimage->Ysize;
    *width = (uint32_t) vipsimage->Xsize;

    // The VipsImage* is no longer needed.
    g_object_unref(vipsimage);

    return ERR_NONE;
}




















