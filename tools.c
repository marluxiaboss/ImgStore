/**
 * @file tools.c
 * @brief implementation of several tool functions for imgStore
 *
 * @author Mia Primorac
 */

#include "imgStore.h"

#include <stdlib.h> // for calloc
#include <stdint.h> // for uint8_t
#include <stdio.h> // for sprintf
#include <openssl/sha.h> // for SHA256_DIGEST_LENGTH
#include <vips/vips.h> // for vips image manips

/**
 * Human-readable SHA
 */
static void sha_to_string (const unsigned char* SHA, char* sha_string)
{
    if (SHA == NULL) {
        return;
    }

    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(&sha_string[i*2], "%02x", SHA[i]);
    }

    sha_string[2*SHA256_DIGEST_LENGTH] = '\0';
}

/**
 * imgStore header display.
 */
void print_header(const imgst_header* header)
{
    if (header != NULL) {
        printf("*****************************************\n");
        printf("**********IMGSTORE HEADER START**********\n");
        printf("TYPE:\t%31s\n",
               header->imgst_name);
        printf("VERSION: %" PRIu32 "\n",
               header->imgst_version);
        printf("IMAGE COUNT: %" PRIu32 "\t\tMAX IMAGES: %" PRIu32 "\n",
               header->num_files, header->max_files);
        printf("THUMBNAIL: %" PRIu16 " x %" PRIu16 "\tSMALL: %" PRIu16 " x %" PRIu16 "\n",
               header->res_resized[2 * RES_THUMB], header->res_resized[2 * RES_THUMB + 1],
               header->res_resized[2 * RES_SMALL], header->res_resized[2 * RES_SMALL + 1]);
        printf("***********IMGSTORE HEADER END***********\n");
        printf("*****************************************\n");
    }
}

/**
 * Metadata display.
 */
void print_metadata(const img_metadata* metadata)
{
    char sha_printable[2*SHA256_DIGEST_LENGTH+1];
    sha_to_string(metadata->SHA, sha_printable);

    if (metadata != NULL) {
        printf("IMAGE ID: %s\n",
               metadata->img_id);
        printf("SHA: %s\n",
               sha_printable);
        printf("VALID: %" PRIu16 "\n",
               metadata->is_valid);
        printf("UNUSED: %" PRIu16 "\n",
               metadata->unused_16);
        printf("OFFSET ORIG. : %" PRIu64 "\t\tSIZE ORIG. : %" PRIu32 "\n",
               metadata->offset[RES_ORIG], metadata->size[RES_ORIG]);
        printf("OFFSET THUMB.: %" PRIu64 "\t\tSIZE THUMB.: %" PRIu32 "\n",
               metadata->offset[RES_THUMB], metadata->size[RES_THUMB]);
        printf("OFFSET SMALL : %" PRIu64 "\t\tSIZE SMALL : %" PRIu32 "\n",
               metadata->offset[RES_SMALL], metadata->size[RES_SMALL]);
        printf("ORIGINAL: %" PRIu32 " x %" PRIu32 "\n",
               metadata->res_orig[0], metadata->res_orig[1]);
        printf("*****************************************\n");
    }
}

/**
 * Opens the imgStore file and reads the header and metadata into imgstfile.
 */
int do_open(const char* imgst_filename, const char* open_mode, imgst_file* imgstfile)
{
    // Null-pointer error handling
    M_REQUIRE_NON_NULL(imgst_filename);
    M_REQUIRE_NON_NULL(open_mode);
    M_REQUIRE_NON_NULL(imgstfile);

    // Valid open modes are rb and rb+
    M_EXIT_IF(!(strcmp(open_mode, "rb") == 0 || strcmp(open_mode, "rb+") == 0),
              ERR_INVALID_ARGUMENT, "invalid open mode", );

    // Init values
    imgstfile->metadata = NULL;
    imgstfile->file = NULL;

    // Open the file
    imgstfile->file = fopen(imgst_filename, open_mode);

    // File is guarenteed to be in read mode, so if it doesn't open then it doesn't exist.
    if (imgstfile->file == NULL) {
        do_close(imgstfile);
        return ERR_IO;
    }

    // Read the header to glean information about the metadata
    size_t nb_elems_read = fread(&(imgstfile->header),
                                 sizeof(imgst_header), 1, imgstfile->file);

    // The (single) header should have been read.
    if (nb_elems_read != 1) {
        do_close(imgstfile);
        return ERR_IO;
    }

    // Dynamically allocate memory for every valid and invalid metadatum.
    imgstfile->metadata = calloc(imgstfile->header.max_files, sizeof(img_metadata));

    // If allocating memory failed
    if (imgstfile->metadata == NULL) {
        do_close(imgstfile);
        return ERR_OUT_OF_MEMORY;
    }

    // Read the metadata
    nb_elems_read += fread(imgstfile->metadata,
                           sizeof(img_metadata), imgstfile->header.max_files, imgstfile->file);

    // Check that the correct number of elements were read.
    if (nb_elems_read != imgstfile->header.max_files + 1) {
        do_close(imgstfile);
        return ERR_IO;
    }

    return ERR_NONE;
}
/**
 * Do some clean-up for imgStore file handling.
 */
void do_close(imgst_file* imgstfile)
{
    /// Clean up the ->file and the ->metadata

    if (imgstfile != NULL) {
        if (imgstfile->file != NULL) {
            // Close and nullify the pointer
            fclose(imgstfile->file);
            imgstfile->file = NULL;
        }

        if (imgstfile->metadata != NULL) {
            // Free and nullify the pointer
            FREE_DEREF(imgstfile->metadata);
        }
    }
}

/**
 * Finds index in metadata for a given img_id
 */
int findMetadataIndex(size_t* idx, const char* img_id, const imgst_file* imgstfile)
{

    // Null-pointer checks
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgstfile);
    M_REQUIRE_NON_NULL(imgstfile->metadata);

    // (Optimisation) Avoid searching if the database is "empty" (no valid files)
    if (imgstfile->header.num_files == 0) {
        return ERR_FILE_NOT_FOUND;
    }

    size_t i = 0;

    while((i < imgstfile->header.max_files)
          && ((strcmp(imgstfile->metadata[i].img_id, img_id) != 0)
              || (imgstfile->metadata[i].is_valid == EMPTY))) {

        ++i;
    }

    // If invalid metadata, return error
    if (validMetadataIndex(i, imgstfile) != ERR_NONE) {
        return ERR_FILE_NOT_FOUND;
    }

    // Point to correct value
    *idx = i;

    return ERR_NONE;
}

/**
 * Decides whether the index is of a valid metadata
 */
int validMetadataIndex(const size_t idx, const imgst_file* imgstfile)
{
    // Null pointer checks
    M_REQUIRE_NON_NULL(imgstfile);
    M_REQUIRE_NON_NULL(imgstfile->metadata);

    // The index must be smaller than the number of metadata
    if (imgstfile->header.max_files <= idx) {
        return ERR_INVALID_ARGUMENT;
    }

    // The metadata must be valid.
    if (imgstfile->metadata[idx].is_valid == EMPTY) {
        return ERR_INVALID_ARGUMENT;
    }

    return ERR_NONE;
}
/**
 * Updates the metadata of the given index in the imgStore file
 */
int updateMetadata(const size_t idx, imgst_file* imgstfile)
{
    // Null pointer checks
    M_REQUIRE_NON_NULL(imgstfile->metadata);
    M_REQUIRE_NON_NULL(imgstfile->file);
    M_REQUIRE_NON_NULL(imgstfile);

    // Check if the index is of a metadata that exists (valid or not)
    M_EXIT_IF(imgstfile->header.max_files <= idx, ERR_FILE_NOT_FOUND,
              "the metadata of that index doesn't exist", );

    // Find the correct position in the file. Take header into account.
    rewind(imgstfile->file);

    if (fseek(imgstfile->file, (long)(idx * sizeof(img_metadata) + sizeof(imgst_header)), SEEK_SET) != 0) {
        return ERR_IO;
    }

    // Attempt to overwrite the metadata.
    if (fwrite(&(imgstfile->metadata[idx]), sizeof(img_metadata), 1, imgstfile->file) != 1) {
        return ERR_IO;
    }

    // New : rewind after writing!
    rewind(imgstfile->file);

    return ERR_NONE;
}

/**
 * Updates the header in the imgStore file
 */
int updateHeader(imgst_file* imgstfile)
{
    // Null pointer checks
    M_REQUIRE_NON_NULL(imgstfile);
    M_REQUIRE_NON_NULL(imgstfile->file);

    // Attempt to overwrite the header.
    rewind(imgstfile->file);

    if (fwrite(&(imgstfile->header), sizeof(imgst_header), 1, imgstfile->file) != 1) {
        return ERR_IO;
    }

    // New : rewind after writing!
    rewind(imgstfile->file);

    return ERR_NONE;
}

/**
 * Transforms resolution string to its int value.
 */
int resolution_atoi(const char* resolution)
{

    // Null pointer check
    M_REQUIRE_NON_NULL_CUSTOM_ERR(resolution, NOT_RES);

    // Switch resolution for RES_CODE
    if (!strcmp("thumb", resolution) || !strcmp("thumbnail", resolution)) {
        return RES_THUMB;

    } else if (!strcmp("small", resolution)) {
        return RES_SMALL;

    } else if (!strcmp("orig", resolution) || !strcmp("original", resolution)) {
        return RES_ORIG;

    } else {
        return NOT_RES;
    }
}

/**
 * Creates a new name image_id + resolution_suffix + .jpg and stores it in newname
 */
int create_name(const char* img_id, const int resolution, char** newname)
{

    // Null-pointer checks
    M_REQUIRE_NON_NULL(img_id);

    // Check if valid resolution code
    M_EXIT_IF(resolution != RES_SMALL && resolution != RES_THUMB && resolution != RES_ORIG,
              ERR_RESOLUTIONS, "The resolution is not a valid resolution code", );

    // Set resolution suffix
    const char* res_suffix = NULL;

    if (resolution == RES_ORIG) {
        res_suffix = "_orig";

    } else if (resolution == RES_SMALL) {
        res_suffix = "_small";

    } else {
        res_suffix = "_thumb";
    }

    // Extension
    const char* ext = ".jpg";

    // String lengths
    const size_t ext_len = strlen(ext);
    const size_t img_id_len = strlen(img_id);
    const size_t res_suffix_len = strlen(res_suffix);
    const size_t new_len = img_id_len + res_suffix_len + ext_len;

    // Concatenation
    char* temp_name = NULL;
    M_EXIT_IF_NULL(temp_name = malloc(new_len + 1), new_len);

    temp_name[0] = '\0';
    strcat(temp_name, img_id);
    strcat(temp_name, res_suffix);
    strcat(temp_name, ext);

    *newname = temp_name;

    return ERR_NONE;
}

/**
 * Reads the image file from disk into a buffer
 */
int read_disk_image(const char* filename, char** buffer, size_t* size)
{
    M_REQUIRE_NON_NULL(filename);

    // Open the file
    FILE* image_file = fopen(filename, "rb");

    if (image_file == NULL) {
        return ERR_IO;
    }

    // Figure out the file's size
    fseek(image_file, 0, SEEK_END);
    *size = ftell(image_file);
    rewind(image_file);

    // Allocate space for the buffer
    *buffer = calloc(1, *size);

    if (*buffer == NULL) {
        fclose(image_file);
        return ERR_OUT_OF_MEMORY;
    }

    // Read the image_file into the buffer
    if (fread(*buffer, *size, 1, image_file) != 1) {
        fclose(image_file);
        FREE_DEREF(*buffer);
        return ERR_IO;
    }

    // Close the file
    fclose(image_file);

    return ERR_NONE;
}


/**
 * Writes buffer as an image file onto the disk.
 */
int write_disk_image(const char* filename, const char* buffer, size_t size)
{
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(buffer);

    // Open the file in write mode
    FILE* image_file = fopen(filename, "wb");

    // Write the buffer to the file
    if (fwrite(buffer, size, 1, image_file) != 1) {
        fclose(image_file);
        return ERR_IO;
    }

    // Close the file
    fclose(image_file);

    return ERR_NONE;
}
