/**
 * @file imgst_list.c
 * @brief imgStore library: do_list implementation
 *
 * @author ???
 */

#include "imgStore.h"
#include <json-c/json.h>

/*
 * Creates a dynamic pointer char* from a const char*,
 * the resulting pointer should be freed at one point!
 */

char* dynamic_from_const_str(const char* const_str)
{
    const size_t const_str_len = strlen(const_str);
    char* dynamic_str = calloc(const_str_len + 1, sizeof(char));
    strncpy(dynamic_str, const_str, const_str_len);
    dynamic_str[const_str_len] = '\0';
    return dynamic_str;
}


char* do_list(const imgst_file* imgstfile, enum do_list_mode mode)
{
    // Null-pointer check (no macro because void return type)
    if (imgstfile == NULL || imgstfile->metadata == NULL) {
        return NULL;
    }

    switch(mode) {
    // Print mode : simply prints the content of the imgStore file
    case STDOUT : {

        // Print header
        print_header(&(imgstfile->header));

        // Print metadata
        if(imgstfile->header.num_files == 0) {
            printf("<< empty imgStore >>\n");

        } else {
            // Loop through all metadata, printing when valid
            for (size_t idx = 0; idx < imgstfile->header.max_files; ++idx) {

                if(imgstfile->metadata[idx].is_valid == NON_EMPTY) {
                    print_metadata(&(imgstfile->metadata[idx]));
                }
            }
        }

        return NULL;
    }

    // JSON mode : returns a string
    case JSON : {

        // Initialize an array of JSON objects
        struct json_object* array = json_object_new_array();

        if(array == NULL) {
            return "";
        }

        // Loop through all valid metadata
        for (size_t idx = 0; idx < imgstfile->header.max_files; ++idx) {
            if(imgstfile->metadata[idx].is_valid == NON_EMPTY) {

                struct json_object* img_id = json_object_new_string((char*)&(imgstfile->metadata[idx]));

                // Append the img_id to the array
                if(json_object_array_add(array, img_id) != 0) {
                    return "";
                }
            }
        }

        // Create a json object with the array
        struct json_object* object = json_object_new_object();

        if(object == NULL) {
            return "";
        }

        // Generate json string and copy it into a char* pointer
        json_object_object_add(object,"Images", array);
        char* json_string = dynamic_from_const_str(json_object_to_json_string(object));

        // Free the json object (actually, if you do so now you also lose the string!)
        json_object_put(object);

        return json_string;
        break;
    }

    default : {
        return dynamic_from_const_str("unimplemented do_list output mode");
    }
    }
}
