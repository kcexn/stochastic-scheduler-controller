#include <curl/curl.h>

/* 8MB is the maximum streambuf size for an in-memory CURL buffer */
#define MAX_BUF_SIZE 8388608

typedef enum {
    EASY_OPT_STRING_T,
    EASY_OPT_LONG_T
} easy_opt_param_type;

typedef struct {
    easy_opt_param_type t;
    const char* param;
} easy_opt_param_str_t;

typedef struct {
    easy_opt_param_type t;
    long param;
} easy_opt_param_long_t;

typedef union {
    struct {
        easy_opt_param_type t;
    } type;
    easy_opt_param_str_t param_str;
    easy_opt_param_long_t param_long;
} easy_opt_parameter_t;

