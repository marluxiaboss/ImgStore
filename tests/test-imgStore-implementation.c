/**
 * @file test-imgStore-implementation.c
 * @brief testing implementation of struct imgst_header, struct img_metadata and struct imgst_file.
 *   If launched with --ok, always return 0, independently of tests results.
 *
 * @author J.-C. Chappelier, EPFL
 * @date 2021
 */

#include "imgStore.h"

#include <stdio.h>  // printf
#include <stddef.h> // offsetof

// ======================================================================
#define SIZE_imgst_header   64
#define SIZE_img_metadata  216

#define SIZE_imgst_file   80

#define OFFSET_imgst_header_imgst_name       0
#define OFFSET_imgst_header_imgst_version   32
#define OFFSET_imgst_header_num_files    36
#define OFFSET_imgst_header_max_files    40
#define OFFSET_imgst_header_res_resized  44

#define OFFSET_img_metadata_img_id       0
#define OFFSET_img_metadata_SHA         128
#define OFFSET_img_metadata_res_orig    160
#define OFFSET_img_metadata_size        168
#define OFFSET_img_metadata_offset      184
#define OFFSET_img_metadata_is_valid    208

#define OFFSET_imgst_file_file           0
#define OFFSET_imgst_file_header         8
#define OFFSET_imgst_file_metadata      72

// ======================================================================
#define test_member(T, M)                                                       \
    do{ if (offsetof(struct T, M) != (size_t) OFFSET_ ## T ## _ ## M) {         \
        printf(#M " wrongly placed in struct " #T ": %zu instead of %d\n"       \
               "(maybe you didn't put them in the same order as we expect.)\n", \
               offsetof(struct T, M), OFFSET_ ## T ## _ ## M);                  \
        status = 1; }                                                           \
    }while(0)

// ======================================================================
#define test_size(T)                                                      \
     do{ if (sizeof(struct T) != (size_t) SIZE_ ## T) {                   \
             printf("struct " #T " does not have the right size: "        \
                    "%zu instead of %d\n", sizeof(struct T), SIZE_ ## T); \
        status = 1; }                                                     \
  }while(0)

// ======================================================================
int main(int argc, char** argv)
{
    int status = 0;
    
    test_size(imgst_header);
    test_size(img_metadata);
    test_size(imgst_file  );
  
    test_member(imgst_header, imgst_name    );
    test_member(imgst_header, imgst_version );
    test_member(imgst_header, num_files  );
    test_member(imgst_header, max_files  );
    test_member(imgst_header, res_resized);

    test_member(img_metadata, img_id  );
    test_member(img_metadata, SHA      );
    test_member(img_metadata, res_orig );
    test_member(img_metadata, size     );
    test_member(img_metadata, offset   );
    test_member(img_metadata, is_valid );

    test_member(imgst_file, file    );
    test_member(imgst_file, header  );
    test_member(imgst_file, metadata);

    if ((argc > 1) && !strcmp(argv[1], "--ok")) status = 0;
    return status;
}
