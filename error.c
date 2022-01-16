/**
 * @file error.c
 * @brief PPS (CS-212) error messages
 */

const char * const ERR_MESSAGES[] = {
    "", // no error
    "I/O Error",
    "(re|m|c)alloc failled",
    "Not enough arguments",
    "Invalid filename",
    "Invalid command",
    "Invalid argument",
    "Invalid max_files number",
    "Invalid resolution(s)",
    "Invalid image ID",
    "Full imgStore",
    "File not found",
    "Not implemented (yet?)",
    "Existing image ID",
    "Image manipulation library error",
    "Debug",

    "no error (shall not be displayed)" // ERR_LAST
};
