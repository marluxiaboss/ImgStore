#pragma once

/**
 * @file image_content.h
 * @brief Header file for image_content.c.
 *
 * Prototypes the lazily_resize method.
 *
 * @author ???
 */

#include "imgStore.h"
#include <vips/vips.h>

/**
 * @brief Creates a resized image and appends it to the imgStore file.
 *
 * @param res_code The image resolution code defined in imgStore.h.
 * @param imgstfile The imgStore file.
 * @param idx The index of the image to resize.
 */
int lazily_resize(const int res_code, imgst_file* imgstfile, const size_t idx);


/**
 * @brief Gets the resolution of a JPEG image
 *
 * @param height will make point to image height
 * @param width will make point to image width
 * @param image_buffer pointer to a memory region containing JPEG image
 * @param image_size size in bytes of the JPEG image
 */
int get_resolution(uint32_t* height, uint32_t* width, const char* image_buffer, const size_t image_size);
