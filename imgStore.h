/**
 * @file imgStore.h
 * @brief Main header file for imgStore core library.
 *
 * Defines the format of the data structures that will be stored on the disk
 * and provides interface functions.
 *
 * The image imgStore starts with exactly one header structure
 * followed by exactly imgst_header.max_files metadata
 * structures. The actual content is not defined by these structures
 * because it should be stored as raw bytes appended at the end of the
 * imgStore file and addressed by offsets in the metadata structure.
 *
 * @author Mia Primorac
 */

#ifndef IMGSTOREPRJ_IMGSTORE_H
#define IMGSTOREPRJ_IMGSTORE_H

#include "error.h" /* not needed in this very file,
                    * but we provide it here, as it is required by
                    * all the functions of this lib.
                    */
#include <stdio.h> // for FILE
#include <stdint.h> // for uint32_t, uint64_t
#include <openssl/sha.h> // for SHA256_DIGEST_LENGTH

/// MACROS

// Useful for freeing pointers to pointers
#define FREE_DEREF(x) free((x)); (x) = NULL;

/// CONSTANTS

#define CAT_TXT "EPFL ImgStore binary"

/* 2D images */
#define DIMS 2

/* constraints */
#define MAX_IMGST_NAME  31  // max. size of a ImgStore name
#define MAX_IMG_ID     127  // max. size of an image id
#define DEF_MAX_FILES 10	// default number of images storable
#define MAX_MAX_FILES 100000  // max. number of images storable

/* for is_valid in imgst_metadata */
#define EMPTY 0
#define NON_EMPTY 1

/* imgStore library internal codes for different image resolutions. */
#define NOT_RES -1
#define RES_THUMB 0
#define RES_SMALL 1
#define RES_ORIG  2

/* the number of imgStore library internal codes for image resolutions. */
#define NB_RES 3

/* default and maximum image resolutions */
#define DEF_RES_THUMB 64
#define DEF_RES_SMALL 256
#define MAX_RES_THUMB 128
#define MAX_RES_SMALL 512

/* initial values for imgst_file fields and subfields*/
#define INIT_NB_FILES 0
#define INIT_VER 0
#define INIT_OFFSET 0

#ifdef __cplusplus
extern "C" {
#endif

/// TYPEDEFS

typedef struct imgst_header imgst_header;
typedef struct img_metadata img_metadata;
typedef struct imgst_file imgst_file;

/// STRUCT DEFINTIIONS

struct imgst_header {
    /* The name of the imgStore file.
     */
    char imgst_name[MAX_IMGST_NAME + 1];

    /* The version number.
     */
    uint32_t imgst_version;

    /* The number of valid images.
     */
    uint32_t num_files;

    /* The total number of images.
     */
    uint32_t max_files;

    /* The default maximal values for resized images for this imgStore
     * Should not be modified.
     */
    uint16_t res_resized [2 * (NB_RES - 1)];

    /* Unused
     */
    uint32_t unused_32;
    uint64_t unused_64;
};

struct img_metadata {
    /* The name of the image.
     */
    char img_id[MAX_IMG_ID + 1];

    /* The hashcode of the image.
     */
    unsigned char SHA[SHA256_DIGEST_LENGTH];

    /* The original resolution of the image. Width x Height
     */
    uint32_t res_orig[DIMS];

    /* The number of bytes in memory of the different image versions.
     */
    uint32_t size[NB_RES];

    /* The location in the imgStore file of each image version.
     */
    uint64_t offset[NB_RES];

    /* Whether the image was previously deleted.
     */
    uint16_t is_valid;

    /* Unused.
     */
    uint16_t unused_16;
};

struct imgst_file {
    /* A pointer to the imgStore file.
     */
    FILE* file;

    /* The header of the imgStore file.
     */
    imgst_header header;

    /* A dynamic array containing the image metadata.
     */
    img_metadata* metadata;
};


/**
 * @brief Prints imgStore header informations.
 *
 * @param header The header to be displayed.
 */
void print_header(const imgst_header* header);


/**
 * @brief Prints image metadata informations.
 *
 * @param metadata The metadata of one image.
 */
void print_metadata(const img_metadata* metadata);

/**
 * @brief Open imgStore file, read the header and all the metadata.
 *
 * @param imgst_filename Path to the imgStore file
 * @param open_mode Mode for fopen(), eg.: "rb", "rb+", etc.
 * @param imgst_file Structure for header, metadata and file pointer.
 */
int do_open(const char* imgst_filename, const char* open_mode, imgst_file* imgstfile);

/**
 * @brief Do some clean-up for imgStore file handling.
 *
 * @param imgst_file Structure for header, metadata and file pointer to be freed/closed.
 */
void do_close(imgst_file* imgstfile);

/**
 * @brief List of possible output modes for do_list
 *
 * @param imgst_file Structure for header, metadata and file pointer to be freed/closed.
 */

enum do_list_mode {STDOUT, JSON};

/**
 * @brief Displays (on stdout) imgStore metadata.
 *
 * @param imgst_file In memory structure with header and metadata.
 */
char* do_list(const imgst_file* imgstfile, enum do_list_mode);

/**
 * @brief Creates the imgStore called imgst_filename. Writes the header and the
 *        preallocated empty metadata array to imgStore file.
 *
 * @param imgst_filename Path to the imgStore file
 * @param imgst_file In memory structure with header and metadata.
 * @return Some error code. 0 if no error.
 */
int do_create(const char* imgst_filename, imgst_file* imgstfile);

/**
 * @brief Deletes an image from a imgStore imgStore.
 *
 * Effectively, it only invalidates the is_valid field and updates the
 * metadata.  The raw data content is not erased, it stays where it
 * was (and  new content is always appended to the end; no garbage
 * collection).
 *
 * @param img_id The ID of the image to be deleted.
 * @param imgst_file The main in-memory data structure
 * @return Some error code. 0 if no error.
 */
int do_delete(const char* img_id, imgst_file* imgstfile);

/**
 * @brief Transforms resolution string to its int value.
 *
 * @param resolution The resolution string. Shall be "original",
 *        "orig", "thumbnail", "thumb" or "small".
 * @return The corresponding value or -1 if error.
 */
int resolution_atoi(const char* resolution);

/**
 * @brief Reads the content of an image from a imgStore.
 *
 * @param img_id The ID of the image to be read.
 * @param resolution The desired resolution for the image read.
 * @param image_buffer Location of the location of the image content
 * @param image_size Location of the image size variable
 * @param imgst_file The main in-memory data structure
 *
 * @return Some error code. 0 if no error.
 */
int do_read(const char* img_id, const int resolution, char** image_buffer,
            uint32_t* image_size, imgst_file* imgstfile);

/**
 * @brief Insert image in the imgStore file
 *
 * @param buffer Pointer to the raw image content
 * @param size Image size
 * @param img_id Image ID
 *
 * @return Some error code. 0 if no error.
 */

int do_insert(const char* image_buffer, size_t image_size, const char* img_id, imgst_file* imgstfile);

/**
 * @brief Finds index in metadata for a given img_id
 *
 * @param idx Index to point to the correct value
 * @param img_id Image ID
 * @param imgstfile The imgst_file in memory
 *
 * @return Some error code. 0 if no error
 */
int findMetadataIndex(size_t* idx, const char* img_id, const imgst_file* imgstfile);

/**
 * @brief Decides whether the index is of a valid metadata
 *
 * @param index The index of the metadata
 * @param imgstfile the imgst_file in memory
 *
 * @return Some error code. 0 if no error
 */
int validMetadataIndex(const size_t idx, const imgst_file* imgstfile);

/**
 * @brief Updates the metadata of the given index in the imgStore file
 *
 * @param index The index of the metadata
 * @param imgstfile The imgst_file in memory
 *
 * @return Some error code. 0 if no error
 */
int updateMetadata(const size_t idx, imgst_file* imgstfile);


/**
 * @brief Updates the header in the imgStore file
 *
 * @param file The imgStore file to update
 * @param imgstfile The imgst_file in memory
 *
 * @return Some error code. 0 if no error
 */
int updateHeader(imgst_file* imgstfile);

/**
 * @brief Creates a new name image_id + resolution_suffix + .jpg and stores it in newname
 *
 * @param img_id The image ID
 * @param resolution The resolution code
 * @param newname The string to store the new name
 */
int create_name(const char* img_id, const int resolution, char** newname);


/**
 * @brief Reads the image file from disk into a buffer
 *
 * @param image_filename the name of the file on disk
 * @param buffer will point to the buffer to contain the image file's contents
 * @param size will point to the image file's number of bytes
 */
int read_disk_image(const char* image_filename, char** buffer, size_t* size);


/**
 * @brief Writes buffer as an image file onto the disk.
 *
 * @param image_filename the name of the file on disk
 * @param buffer the buffer containing the image file's contents
 * @param size the image file's number of bytes
 */
int write_disk_image(const char* image_filename, const char* buffer, size_t size);

/**
 * @brief Removes the deleted images by moving the existing ones
 *
 * @param imgst_path The path to the imgStore file
 * @param imgst_tmp_bkp_path The path to the a (to be created) temporary imgStore backup file
 *
 * @return Some error code. 0 if no error.
 */
int do_gbcollect (const char *imgst_path, const char *imgst_tmp_bkp_path);

#ifdef __cplusplus
}
#endif
#endif
