/**
 * @file imgst_create.c
 * @brief imgStore library: do_create implementation.
 *
 * @author ???
 */

#include "imgStore.h"
#include "error.h" // for errors

#include <string.h> // for strncpy
#include <stdlib.h> // for calloc
#include <stdio.h> // for fopen

/**
 * Creates the imgStore binary file
 */
int do_create(const char* imgst_filename, imgst_file* imgstfile)
{
    // Null-pointer checks
    M_REQUIRE_NON_NULL(imgst_filename);
    M_REQUIRE_NON_NULL(imgstfile);

    /// Explicitly initialize the header member

    // Sets the database header name
    strncpy(imgstfile->header.imgst_name, CAT_TXT,  MAX_IMGST_NAME);
    imgstfile->header.imgst_name[MAX_IMGST_NAME] = '\0';

    // Sets the version to 0 and the number of files to 0
    imgstfile->header.imgst_version = INIT_VER;
    imgstfile->header.num_files = INIT_NB_FILES;

    /// Explicitly initialize the metadata member
    M_EXIT_IF_NULL(imgstfile->metadata = calloc(imgstfile->header.max_files, sizeof(img_metadata)),
                   sizeof(img_metadata));

    /// Explicitly initialize the file member

    // The pointer to the file we write to
    imgstfile->file = ((FILE*) NULL);

    // Write to binary file
    size_t num_files_written = 0;

    imgstfile->file = fopen(imgst_filename, "wb+");

    if (imgstfile->file == NULL) {
        FREE_DEREF(imgstfile->metadata);
        return ERR_IO;
    }

    // Write the header and metadata to the file.
    M_EXIT_IF_ERR_DO_SOMETHING(updateHeader(imgstfile),
                               FREE_DEREF(imgstfile->metadata));
    num_files_written += 1;

    for(size_t i = 0; i < imgstfile->header.max_files; ++i) {
        M_EXIT_IF_ERR_DO_SOMETHING(updateMetadata(i, imgstfile),
                                   FREE_DEREF(imgstfile->metadata));
        num_files_written += 1;
    }

    M_EXIT_IF(num_files_written != imgstfile->header.max_files + 1,
              ERR_IO, "incorrect number of files written", );

    // Print the number of successfully written items.
    fprintf(stdout, "%zu item(s) written\n", num_files_written);

    return ERR_NONE;
}
