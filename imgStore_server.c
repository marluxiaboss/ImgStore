/**
 * @file imgStore_server.c
 * @brief imgStore library: webserver implementation.
 *
 * @author ???
 */

#include "imgStore.h"
#include "error.h"
#include "util.h" // for _unused, atouint32
#include "mongoose.h"

#include <stdlib.h>
#include <string.h> // for strlen and strcmp
#include <stdint.h> // for uint32_t, uint64_t
#include <vips/vips.h>

// -- Constants --------------------------------------------------------

#define MIN_SERVER_ARGS 2
#define LISTENING_ADDRESS "localhost:8000"
#define ROOT "."
#define POLL_PERIOD_MS 1000
#define NB_HANDLERS 4

// HTTP Response codes
#define HTTP_RESPONSE_CODE 200
#define HTTP_RELOAD_CODE 302
#define HTTP_ERROR_CODE 500

// For queries
#define QUERY_LEN_RESOLUTION 9 // The maximum length of resolution options
#define QUERY_LEN_OFFSET 10 // ciel(log_10(2^32))

#define JPG_EXT 4 // strlen(".jpg")
#define ENCODE_URI_SCALE 4 // in index.html, encodeURIComponent may turn 1 character into 4

// This seems like standard use for mongoose programmes.
static const char* s_listening_address = LISTENING_ADDRESS;
static const char* s_web_directory = ROOT;

// -- Macros -----------------------------------------------------------

#define THROW_IF_CALL_FAILS_DO(call1, call2, nc) \
do { \
	const error_code err = call1; \
	if (err != ERR_NONE) { \
		call2; \
		mg_error_msg(nc, err); \
		return; \
	} \
} while (0)

#define THROW_ERR_IF_DO(cond, calls, nc, err) \
do { \
	if (cond) { \
		calls; \
		mg_error_msg(nc, err); \
		return; \
	} \
} while (0)

#define THROW_ERR_IF(cond, nc, err) \
THROW_ERR_IF_DO(cond,,nc,err)

#define IF_ERR_PRINT_EXIT(cond, err) \
do { \
	if(cond) { \
		fprintf(stderr, "%s", ERR_MESSAGES[err]); \
		return err; \
	} \
} while (0)

// -- Typedefs ---------------------------------------------------------

typedef void (*handler)(struct mg_connection *nc, struct mg_http_message *hm,
                        imgst_file* imgstfile);	// Handlers

typedef struct handler_mapping handler_mapping;
typedef struct data data;

// -- Structs ----------------------------------------------------------

// Maps an uri to a http method and a handler function
struct handler_mapping {
    const char* uri;
    const char* method;
    handler call;
};

// This is intended to be passed to the event handler as fn_data
struct data {
    const handler_mapping* handlers;
    imgst_file* imgstfile;
};

// -- Functions --------------------------------------------------------

/**
 * Produces a HTTP 302 reply in order to reload the page
 */
void mg_reload_msg(struct mg_connection* nc)
{
    mg_printf(nc,
              "HTTP/1.1 %d Found\r\n"
              "Location: http://%s/index.html\r\n\r\n", HTTP_RELOAD_CODE, s_listening_address);
    nc->is_draining = 1; // Necessary for the reload code 302
}


/**
 * Produces a HTTP 500 reply with an error message.
 */
void mg_error_msg(struct mg_connection* nc, int error)
{

    // Do nothing if no error
    if (error == ERR_NONE) {
        return;
    }

    // Handle bad inputs
    THROW_ERR_IF((nc == NULL) || (error < 0 || NB_ERR <= error),
                 nc, ERR_INVALID_ARGUMENT);

    // Reply with error message
    mg_http_reply(nc, HTTP_ERROR_CODE, NULL, "Error: %s\r\n", ERR_MESSAGES[error]);
}

/**
 * Returns the string value for a given key in the http message query
 */
char* get_var_from_query(struct mg_connection *nc, struct mg_http_message *hm,
                         const char* key, size_t val_len)
{
    if(nc == NULL || hm == NULL || key == NULL) {
        mg_error_msg(nc, ERR_INVALID_ARGUMENT);
        return NULL;
    }

    // Allocate memory for destination of value string
    char* dst = calloc(val_len, sizeof(char));

    if (dst == NULL) {
        mg_error_msg(nc, ERR_OUT_OF_MEMORY);
        return NULL;
    }

    // Read value string into destination array
    if (mg_http_get_var(&(hm->query), key, dst, val_len + 1) <= 0) {
        FREE_DEREF(dst);
        mg_error_msg(nc, ERR_INVALID_ARGUMENT);
    }

    return dst;
}

/**
 * Produces an HTTP 200 reply listing an imgStore file as JSON
 */
void handle_list_call(struct mg_connection *nc, struct mg_http_message *hm _unused,
                      imgst_file* imgstfile)
{
    // Invalid arguments
    THROW_ERR_IF(nc == NULL || imgstfile == NULL,
                 nc, ERR_INVALID_ARGUMENT);

    char* store_list = do_list(imgstfile, JSON);
    THROW_ERR_IF(store_list == NULL,
                 nc, ERR_IO);

    // If do_list worked, printf a HTTP reply with imgStore file as JSON
    mg_printf(nc,
              "HTTP/1.1 %d OK\r\n"
              "Content-Type: application/json\r\n"
              "Content-Length: %zu\r\n\r\n"
              "%s", HTTP_RESPONSE_CODE, strlen(store_list), store_list);

    free(store_list);
}

/**
 * Produces an HTTP 200 reply for a read command. Given a resolution code
 * and an imgID for query keys, reads the image in the imgStore and creates
 * a resized version if it doesn't yet exist under the requested resolution.
 */
void handle_read_call(struct mg_connection *nc, struct mg_http_message *hm,
                      imgst_file* imgstfile)
{
    // Invalid arguments
    THROW_ERR_IF(nc == NULL || hm == NULL || imgstfile == NULL,
                 nc, ERR_INVALID_ARGUMENT);

    // Get res variable from http message query
    char* res_str = get_var_from_query(nc, hm, "res", QUERY_LEN_RESOLUTION);

    if (res_str == NULL) return; // error sent in get_var

    const int res = resolution_atoi(res_str);
    THROW_ERR_IF_DO(res == NOT_RES,
                    FREE_DEREF(res_str),
                    nc, ERR_RESOLUTIONS);
    FREE_DEREF(res_str);

    // Get img_id variable from http message query
    char* img_id = get_var_from_query(nc, hm, "img_id", MAX_IMG_ID);

    if (img_id == NULL) return; // error sent in get_var

    THROW_ERR_IF_DO(strlen(img_id) == 0 || strlen(img_id) > MAX_IMG_ID,
                    FREE_DEREF(img_id),
                    nc, ERR_INVALID_IMGID);


    // Read image into buffer
    char* image_buffer = NULL;
    uint32_t image_size = 0;
    THROW_IF_CALL_FAILS_DO(do_read(img_id, res, &image_buffer, &image_size, imgstfile),
                           FREE_DEREF(img_id), nc);

    // Free img_id
    FREE_DEREF(img_id);

    // Formatted HTTP reply with imgStore file as JSON
    mg_printf(nc,
              "HTTP/1.1 %d OK\r\n"
              "Content-Type: image/jpeg\r\n"
              "Content-Length: %zu\r\n\r\n", HTTP_RESPONSE_CODE, (size_t) image_size);


    // Send the image to the server!
    THROW_ERR_IF_DO(mg_send(nc, image_buffer, (size_t) image_size) != (int) image_size,
                    FREE_DEREF(image_buffer),
                    nc, ERR_IO);

    // Free rest
    FREE_DEREF(image_buffer);
}

/**
 * Produces an HTTP 200 reply for a delete command. Given an imgID for a
 * query key, deletes the image from the imgStore file.
 */
void handle_delete_call(struct mg_connection *nc, struct mg_http_message *hm,
                        imgst_file* imgstfile)
{
    THROW_ERR_IF(nc == NULL || hm == NULL || imgstfile == NULL,
                 nc, ERR_INVALID_ARGUMENT);

    // Get imgID from http message query
    char* img_id = get_var_from_query(nc, hm, "img_id", MAX_IMG_ID);

    if (img_id == NULL) return; // error sent in get_var


    // Delete the image from the imgStore file
    THROW_IF_CALL_FAILS_DO(do_delete(img_id, imgstfile),
                           FREE_DEREF(img_id),
                           nc);

    // If do_delete went well, reload page.
    mg_reload_msg(nc);

    // Free rest
    FREE_DEREF(img_id);
}

/**
 * Produces an HTTP 200 reply for the insert command. Allows for the insertion
 * of an image into the imgStore file in a 2-phase strategy. First the image
 * is chunked and temporarily stored and then the image is inserted.
 */
void handle_insert_call(struct mg_connection *nc, struct mg_http_message *hm,
                        imgst_file* imgstfile)
{
    THROW_ERR_IF(nc == NULL || hm == NULL || imgstfile == NULL,
                 nc, ERR_INVALID_ARGUMENT);

    // Make sure there is enough space
    THROW_ERR_IF(imgstfile->header.num_files >= imgstfile->header.max_files,
                 nc, ERR_FULL_IMGSTORE);

    // Mode 1: chunk uploading
    if (hm->body.len != 0) {

        // Simply upload the chunk
        THROW_ERR_IF(mg_http_upload(nc, hm, "/tmp") != (int) hm->body.len,
                     nc, ERR_IO);

        // Mode 2: image insertion

    } else {

        // Get chunk offset. Image size at most 2^32. The offset need not be larger.
        char* offset_str = get_var_from_query(nc, hm, "offset", QUERY_LEN_OFFSET);

        if (offset_str == NULL) return; // error sent get_var

        uint32_t offset = atouint32(offset_str);
        FREE_DEREF(offset_str);

        // Get jpg image file name, scaling for encoding and leaving space for .jpg extension.
        size_t name_len = ENCODE_URI_SCALE * MAX_IMG_ID + JPG_EXT;
        char* img_id = get_var_from_query(nc, hm, "name", name_len);

        if (img_id == NULL) return; // error sent in get_var

        THROW_ERR_IF_DO(strlen(img_id) == 0 || strlen(img_id) > MAX_IMG_ID,
                        FREE_DEREF(img_id),
                        nc, ERR_INVALID_IMGID);

        // Read the temporary image.
        void* image_buffer = calloc(1, offset);

        if (image_buffer == NULL) {
            FREE_DEREF(img_id);
            mg_error_msg(nc, ERR_OUT_OF_MEMORY); return;
        }

        // Concatenate to get path /tmp/img_id
        char* filename = malloc(strlen("/tmp/") + name_len + 1);
        filename[0] = '\0';
        strcat(filename, "/tmp/");
        strcat(filename, img_id);

        // Open the image file and read it into the buffer
        FILE* image_file = fopen(filename, "rb");
        THROW_ERR_IF_DO(image_file == NULL,
                        FREE_DEREF(filename); FREE_DEREF(img_id); FREE_DEREF(image_buffer),
                        nc, ERR_IO);

        THROW_ERR_IF_DO(fread(image_buffer, offset, 1, image_file) != 1,
                        fclose(image_file);
                        FREE_DEREF(filename); FREE_DEREF(img_id); FREE_DEREF(image_buffer),
                        nc, ERR_IO);

        // Insert the image into the imgStore
        THROW_IF_CALL_FAILS_DO(do_insert(image_buffer, offset, img_id, imgstfile),
                               fclose(image_file);
                               FREE_DEREF(filename); FREE_DEREF(img_id); FREE_DEREF(image_buffer),
                               nc);

        // If do_insert went well, reload page.
        mg_reload_msg(nc);

        // Free and close rest
        fclose(image_file);
        FREE_DEREF(filename);
        FREE_DEREF(img_id);
        FREE_DEREF(image_buffer);
    }
}


/**
 * Attempts to serve the HTTP message with an appropriate handler.
 */
static void imgst_event_handler(struct mg_connection *nc,
                                int ev,
                                void *ev_data,
                                void *fn_data)
{
    // If the event is of type HTTP message
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message*) ev_data;
        data* d = (data*) fn_data;
        imgst_file* imgstfile = d->imgstfile;
        const handler_mapping* handlers = d->handlers;

        // Search for a handler
        int found = 0;

        for (size_t i = 0; i < NB_HANDLERS; ++i) {

            // The URI and the method type should match
            const char* method = handlers[i].method;

            if (mg_http_match_uri(hm, handlers[i].uri)
                && mg_globmatch(method, strlen(method), hm->method.ptr, hm->method.len)) {

                handlers[i].call(nc, hm, imgstfile);
                found = 1;
            }
        }

        // If no handlers were found, just return static data.
        if (!found) {

            // No SSI file name pattern, no extra HTTP headers
            struct mg_http_serve_opts opts = {.root_dir = s_web_directory};
            mg_http_serve_dir(nc, ev_data, &opts);
        }
    }
}

int main(int argc, char *argv[])
{
    // VIPS_INIT
    if (VIPS_INIT(argv[0])) {
        vips_error_exit("unable to start VIPS");
        return ERR_IMGLIB;
    }

    IF_ERR_PRINT_EXIT(argc < MIN_SERVER_ARGS, ERR_NOT_ENOUGH_ARGUMENTS);

    argc--; argv++; // skips command call name

    // Get the name of the imgStore file
    const char* imgstore_filename = argv[0];
    IF_ERR_PRINT_EXIT(imgstore_filename == NULL, ERR_INVALID_ARGUMENT);

    // Open the imgStore file
    imgst_file imgstfile;
    IF_ERR_PRINT_EXIT(do_open(imgstore_filename, "rb+", &imgstfile) != ERR_NONE, ERR_IO);

    // Map the handlers
    handler_mapping handlers[NB_HANDLERS] = {
        {"/imgStore/list", "GET", handle_list_call},
        {"/imgStore/read", "GET", handle_read_call},
        {"/imgStore/delete", "GET", handle_delete_call},
        {"/imgStore/insert", "POST", handle_insert_call},
    };

    // Create the data structure to be sent to the event handler!
    data d = (data) {
        handlers, &imgstfile
    };

    // Create the server
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    if (mg_http_listen(&mgr, s_listening_address, imgst_event_handler, &d) == NULL) {
        fprintf(stderr, "Error starting server on address %s\n", s_listening_address);
    }

    // Print once after server starts.
    fprintf(stdout, "Starting imgStore server on http://%s\n", s_listening_address);
    print_header(&(imgstfile.header));

    // Poll the event handler every second.
    for (;;) mg_mgr_poll(&mgr, POLL_PERIOD_MS);

    // Shut down the server
    mg_mgr_free(&mgr);
    do_close(&imgstfile);

    // Shut down VIPS
    vips_shutdown();

    return ERR_NONE;
}
