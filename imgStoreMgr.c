/**
 * @file imgStoreMgr.c
 * @brief imgStore Manager: command line interpretor for imgStore core commands.
 *
 * Image Database Management Tool
 *
 * @author Mia Primorac
 */

#include "util.h" // for _unused
#include "imgStore.h"
#include "error.h"

#include <stdlib.h>
#include <string.h> // for strlen and strcmp
#include <vips/vips.h>

// Constants : commands
#define NB_COMMANDS 7
#define MIN_COMMAND_ARGS 2

#define MIN_LIST_ARGS 2
#define MIN_CREATE_ARGS 2
#define MIN_DELETE_ARGS 3
#define MIN_READ_ARGS 3
#define MIN_INSERT_ARGS 4
#define MIN_GC_ARGS 3

// Constants : create command
#define NB_CREATE_OPTIONS 3
#define MAX_FILES_UINT_BITS 32
#define RES_UINT_BITS 16
#define CREATE_OPTION_STRLEN 10
#define ARGC_MAX_FILES 1
#define ARGC_THUMB_RES 2
#define ARGC_SMALL_RES 2

// Typedefs
typedef int (*command)(int args, char* argv[]);	// Commands

typedef struct command_mapping command_mapping;
typedef struct option_mapping option_mapping;

/**
 * This maps command names to functions
 */
struct command_mapping {
    const char* name;
    command comm;
};

/**
 * This maps command options to everything it needs
 */
struct option_mapping {
    const char* name;    // option name
    size_t argc;         // option obligatory nb of arguments
    size_t bits;         // unsigned integer type number of bits
    uint32_t max_val;    // largest value that an argument can take
    int range_error;     // error if argument too large
    void* arguments;     // option arguments
};


/**
 *  Do garbage collecting
 */
int do_gbcollect_cmd(int args, char* argv[])
{
    // GC needs at least filename and backup file as argument
    if (args < MIN_GC_ARGS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // Get filename argument
    const char* filename = argv[1];
    M_REQUIRE_NON_NULL(filename);

    // Get backup file argument
    const char* backup_filename = argv[2];
    M_REQUIRE_NON_NULL(backup_filename);

    M_EXIT_IF_ERR(do_gbcollect(filename, backup_filename));

    return ERR_NONE;
}

/**
 * Opens imgStore file and calls do_list command.
 */
int do_list_cmd (int args, char* argv[])
{
    // List needs at least filename as argument
    if (args < MIN_LIST_ARGS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // Get filename argument
    const char* filename = argv[1];
    M_REQUIRE_NON_NULL(filename);

    // Declare an imgst_file
    imgst_file imgstfile;

    // Open the file with the given filename in binary read mode
    M_EXIT_IF_ERR(do_open(filename, "rb", &imgstfile));

    // List the contents and then close the file.
    do_list(&imgstfile, STDOUT);
    do_close(&imgstfile);

    return ERR_NONE;
}

/**
 * Prepares and calls do_create command.
 */
int do_create_cmd (int args, char* argv[])
{
    // Create needs at least create filename as argument
    if (args < MIN_CREATE_ARGS) {
        return  ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // Get non-null and size-capped filename argument
    const char* filename = argv[1];
    M_EXIT_IF((filename == NULL) || (strlen(filename) > MAX_IMGST_NAME),
              ERR_INVALID_ARGUMENT, "invalid filename argument", );

    // Skips "create" and "<imgstore_filename>"
    args -= MIN_CREATE_ARGS; argv += MIN_CREATE_ARGS;

    /// We parse the options

    // Default option values
    uint32_t max_files_args[ARGC_MAX_FILES] = {DEF_MAX_FILES};
    uint16_t thumb_res_args[ARGC_THUMB_RES] = {DEF_RES_THUMB, DEF_RES_THUMB};
    uint16_t small_res_args[ARGC_SMALL_RES] = {DEF_RES_SMALL, DEF_RES_SMALL};
    option_mapping options[NB_CREATE_OPTIONS] = {
        {
            .name = "-max_files", .argc = ARGC_MAX_FILES, .bits = MAX_FILES_UINT_BITS,
            .max_val = MAX_MAX_FILES, .range_error = ERR_MAX_FILES,
            .arguments = max_files_args
        },
        {
            .name = "-thumb_res", .argc = ARGC_THUMB_RES, .bits = RES_UINT_BITS,
            .max_val = MAX_RES_THUMB, .range_error = ERR_RESOLUTIONS,
            .arguments = thumb_res_args
        },
        {
            .name = "-small_res", .argc = ARGC_SMALL_RES, .bits = RES_UINT_BITS,
            .max_val = MAX_RES_SMALL, .range_error = ERR_RESOLUTIONS,
            .arguments = small_res_args
        }
    };

    // Loop over all arguments
    size_t i = 0;

    while (i < (size_t) args) {

        // Loop over all options
        int found = 0;

        for (size_t j = 0; j < NB_CREATE_OPTIONS && !found; ++j) {

            if (!strncmp(options[j].name, argv[i], CREATE_OPTION_STRLEN)) {

                // Check if there are enough arguments
                if ((size_t) args <= i + options[j].argc) {
                    return ERR_NOT_ENOUGH_ARGUMENTS;
                }

                // Loop over all arguments for this option
                for (size_t k = 0; k < options[j].argc; ++k) {

                    // Switch over number of bits in uint representation
                    if (options[j].bits == MAX_FILES_UINT_BITS) {
                        uint32_t conv = atouint32(argv[i + 1 + k]);

                        // Check if the arguments are in range
                        if (conv == 0 || options[j].max_val < conv) {
                            return options[j].range_error;
                        }

                        // Assign the argument
                        ((uint32_t*) options[j].arguments)[k] = conv;

                    } else if (options[j].bits == RES_UINT_BITS) {
                        uint16_t conv = atouint16(argv[i + 1 + k]);

                        // Check if the arguments are in range
                        if (conv == 0 || options[j].max_val < conv) {
                            return options[j].range_error;
                        }

                        // Assign the argument
                        ((uint16_t*) options[j].arguments)[k] = conv;

                    } else {
                        return ERR_INVALID_ARGUMENT;
                    }
                }

                // Increment i to move to location of next option
                i += options[j].argc + 1;

                // break
                found = 1;
            }
        }

        // If no option was found that matched the string
        if (!found) {
            return ERR_INVALID_ARGUMENT;
        }
    }

    puts("Create");

    // Declare an imgst_file
    imgst_file imgstfile;

    // Initialize the values that aren't meant to be changed
    imgstfile.header = (imgst_header) {
        .res_resized = {
            ((uint16_t*)options[1].arguments)[0], ((uint16_t*)options[1].arguments)[1],
            ((uint16_t*)options[2].arguments)[0], ((uint16_t*)options[2].arguments)[1]
        },
        .max_files = ((uint16_t*)options[0].arguments)[0]
    };

    // Explicitly initialize the rest of the imgst_file.
    M_EXIT_IF_ERR_DO_SOMETHING(do_create(filename, &imgstfile),
                               do_close(&imgstfile));

    // Print the header and clean up the file if nothing went wrong.
    print_header(&(imgstfile.header));
    do_close(&imgstfile);

    return ERR_NONE;
}
/**
 * Displays some explanations.
 */
int help (int args _unused, char* argv[] _unused)
{
    printf("imgStoreMgr [COMMAND] [ARGUMENTS]\n"
           "  help: displays this help.\n"
           "  list <imgstore_filename>: list imgStore content.\n"
           "  create <imgstore_filename> [options]: create a new imgStore.\n"
           "      options are:\n"
           "          -max_files <MAX_FILES>: maximum number of files.\n"
           "                                  default value is %d\n"
           "                                  maximum value is %d\n"
           "          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n"
           "                                  default value is %dx%d\n"
           "                                  maximum value is %dx%d\n"
           "          -small_res <X_RES> <Y_RES>: resolution for small images.\n"
           "                                  default value is %dx%d\n"
           "                                  maximum value is %dx%d\n"
           "  read   <imgstore_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n"
           "      read an image from the imgStore and save it to a file.\n"
           "      default resolution is \"original\".\n"
           "  insert <imgstore_filename> <imgID> <filename>: insert a new image in the imgStore.\n"
           "  delete <imgstore_filename> <imgID>: delete image imgID from imgStore.\n"
           "  gc <imgstore_filename> <tmp imgstore_filename>: performs garbage collecting on imgStore. Requires a temporary filename for copying the imgStore.\n",
           DEF_MAX_FILES, MAX_MAX_FILES,
           DEF_RES_THUMB, DEF_RES_THUMB, MAX_RES_THUMB, MAX_RES_THUMB,
           DEF_RES_SMALL, DEF_RES_SMALL, MAX_RES_SMALL, MAX_RES_SMALL);

    // We'll assume that calling help never fails.
    return ERR_NONE;
}

/**
 * Deletes an image from an imgStore
 */
int do_delete_cmd (int args, char* argv[])
{
    // Delete needs at least filename and imgID as argument
    if (args < MIN_DELETE_ARGS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // Get non-null filename argument
    const char* filename = argv[1];
    M_REQUIRE_NON_NULL(filename);

    // Get non-null non-degenerate capped-length imgID argument
    const char* img_id = argv[2];
    M_EXIT_IF((img_id == NULL || strlen(img_id) == 0 || strlen(img_id) > MAX_IMG_ID),
              ERR_INVALID_IMGID, "invalid imgID argument", );

    // Open the file
    imgst_file imgstfile;
    M_EXIT_IF_ERR(do_open(filename, "rb+", &imgstfile));

    // If correctly opened, then delete.
    M_EXIT_IF_ERR_DO_SOMETHING(do_delete(img_id, &imgstfile),
                               do_close(&imgstfile));

    // Clean up the file
    do_close(&imgstfile);

    return ERR_NONE;
}

/**
 * Reads the content of an image from a imgStore
 */
int do_read_cmd (int args, char* argv[])
{

    // Read needs at least <imgstore_filename>, <imgID>
    if (args < MIN_READ_ARGS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // Get non-null filename argument
    const char* imgstore_filename = argv[1];
    M_REQUIRE_NON_NULL(imgstore_filename);

    // Get non-null non-degenerate capped-length imgID argument
    const char* img_id = argv[2];
    M_EXIT_IF((img_id == NULL || strlen(img_id) == 0 || strlen(img_id) > MAX_IMG_ID),
              ERR_INVALID_IMGID, "invalid imgID argument", );

    // Optional argument is [orig|thumbnail|...]
    const int resolution = (args >= MIN_READ_ARGS + 1) ? resolution_atoi(argv[3]) : RES_ORIG;
    M_EXIT_IF(resolution == NOT_RES, ERR_RESOLUTIONS, "invalid resolution code", );

    // Open the file
    imgst_file imgstfile;
    M_EXIT_IF_ERR(do_open(imgstore_filename, "rb+", &imgstfile));

    // Read into image_buffer and image_size
    char* image_buffer = NULL;
    uint32_t image_size = 0;
    M_EXIT_IF_ERR_DO_SOMETHING(do_read(img_id, resolution, &image_buffer, &image_size, &imgstfile),
                               do_close(&imgstfile));

    // Generate a new name
    char* new_name;
    M_EXIT_IF_ERR_DO_SOMETHING(create_name(img_id, resolution, &new_name),
                               do_close(&imgstfile);
                               FREE_DEREF(image_buffer));

    // Write to jpg in folder where imgStoreMgr is located
    M_EXIT_IF_ERR_DO_SOMETHING(write_disk_image(new_name, image_buffer, (size_t) image_size),
                               FREE_DEREF(new_name);
                               FREE_DEREF(image_buffer);
                               do_close(&imgstfile));



    // Free pointers
    FREE_DEREF(new_name);
    FREE_DEREF(image_buffer);

    // Close the files
    do_close(&imgstfile);

    return ERR_NONE;
}


/**
 * Reads the content of an image from a imgStore
 */
int do_insert_cmd (int args, char* argv[])
{

    // Insert needs at least <imgstore_filename> <imgID> <filename>
    if (args < MIN_INSERT_ARGS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // Get non-null imgstore filename
    const char* imgstore_filename = argv[1];
    M_REQUIRE_NON_NULL(imgstore_filename);

    // Get non-null non-degenerate capped-length imgID argument
    const char* img_id = argv[2];
    M_EXIT_IF((img_id == NULL || strlen(img_id) == 0 || strlen(img_id) > MAX_IMG_ID),
              ERR_INVALID_IMGID, "invalid imgID argument", );

    // Get non-null image filename
    const char* image_filename = argv[3];
    M_REQUIRE_NON_NULL(image_filename);

    // Open the imgStore file
    imgst_file imgstfile;
    M_EXIT_IF_ERR(do_open(imgstore_filename, "rb+", &imgstfile));

    // Make sure there is enough space
    if (imgstfile.header.num_files >= imgstfile.header.max_files) {
        do_close(&imgstfile);
        return ERR_FULL_IMGSTORE;
    }

    // Load image_buffer with contents of the image
    char* image_buffer = NULL;
    size_t image_size = 0;
    M_EXIT_IF_ERR_DO_SOMETHING(read_disk_image(image_filename, &image_buffer, &image_size),
                               do_close(&imgstfile));

    // Insert
    M_EXIT_IF_ERR_DO_SOMETHING(do_insert(image_buffer, image_size, img_id, &imgstfile),
                               FREE_DEREF(image_buffer);
                               do_close(&imgstfile));

    // Free buffer and clean up the file
    FREE_DEREF(image_buffer);
    do_close(&imgstfile);

    return ERR_NONE;
}

/**
 * MAIN
 */
int main (int argc, char* argv[])
{
    // VIPS_INIT
    if (VIPS_INIT(argv[0])) {
        vips_error_exit("unable to start VIPS");
        return ERR_IMGLIB;
    }

    // Array of struct command_mapping containing all possible commands
    const command_mapping commands[NB_COMMANDS] = {
        {"list", do_list_cmd},
        {"create", do_create_cmd},
        {"help", help},
        {"delete", do_delete_cmd},
        {"read", do_read_cmd},
        {"insert", do_insert_cmd},
        {"gc", do_gbcollect_cmd}
    };


    int ret = ERR_NONE;

    // Every command takes at least one argument
    if (argc < MIN_COMMAND_ARGS) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;

    } else {
        argc--; argv++; // skips command call name

        // Loop over commands and call the function if found
        int found = 0;

        for (size_t i = 0; i < NB_COMMANDS && !found; ++i) {

            if(!strcmp(commands[i].name, argv[0])) {
                ret = commands[i].comm(argc, argv);
                found = 1;
            }
        }

        if (!found) {
            ret = ERR_INVALID_COMMAND;
        }
    }

    // Print error message if error is not ERR_NONE
    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MESSAGES[ret]);
        help(argc, argv);
    }

    vips_shutdown();

    return ret;
}
