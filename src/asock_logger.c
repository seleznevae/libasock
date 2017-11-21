#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "asock_logger.h"

static int s_trace_level = DEFAULT_APPLICATION_TRACE_LEVEL;
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

FILE* s_log_file = NULL;

int get_trace_level()
{
    return s_trace_level;
}

void set_trace_level(int level)
{
    s_trace_level = level;
}

static const int MAX_PATH = 1024;

void trace_event(int event_trace_level,
                 int add_strerr_info,
                 const char* _file,
                 const int line,
                 const char * format,
                 ...)
{
    if (s_log_file == NULL)
        s_log_file = stdout;

    va_list va_ap;
    struct tm result;

    int errnum = errno;

    if((event_trace_level <= s_trace_level) && (s_trace_level > 0)) {
        char buf[65536], color_out_buf[65536], out_buf[65536];
        char theDate[32];
        char *file = (char*)_file;
        char extra_msg[100] = {'\0'};
        time_t theTime = time(NULL);
        char filebuf[MAX_PATH];
        const char *backslash = strrchr(_file, '/');

        if(backslash != NULL) {
            snprintf(filebuf, sizeof(filebuf), "%s", &backslash[1]);
            file = (char*)filebuf;
        }

        va_start(va_ap, format);

        pthread_mutex_lock(&s_mutex); //TODO: should check for zero ?

        memset(buf, 0, sizeof(buf));
        strftime(theDate, 32, "%d/%b/%Y %H:%M:%S", localtime_r(&theTime, &result));
        vsnprintf(buf, sizeof(buf) - 1, format, va_ap);

        if (event_trace_level == TRACE_LEVEL_ERROR) {
            strcat(extra_msg, "ERROR: ");
            if (add_strerr_info) {
                strcat(extra_msg, "(");
                strcat(extra_msg, strerror(errnum));
                strcat(extra_msg, ") ");
            }
        } else if(event_trace_level == TRACE_LEVEL_WARNING ) {
            strcat(extra_msg, "WARNING: ");
        }

        while(buf[strlen(buf) - 1] == '\n') buf[strlen(buf) - 1] = '\0';

        const char *begin_color_tag = "";
        switch (event_trace_level) {
            case (TRACE_LEVEL_ERROR):
                begin_color_tag = "\x1b[31m";
                break;
            case (TRACE_LEVEL_WARNING):
                begin_color_tag = "\x1b[33m";
                break;
            case (TRACE_LEVEL_NORMAL):
                begin_color_tag = "\x1b[32m";
                break;
            case (TRACE_LEVEL_INFO):
                begin_color_tag = "\x1b[34m";
                break;
            case (TRACE_LEVEL_DEBUG):
                begin_color_tag = "\x1b[0m";
                break;
        };
        const char *end_color_tag = "\x1b[0m";

        snprintf(color_out_buf, sizeof(color_out_buf), "%s%s [%s:%d]%s %s%s",
                 begin_color_tag, theDate, file, line, end_color_tag, extra_msg, buf);
        snprintf(out_buf, sizeof(out_buf), "%s [%s:%d] %s%s", theDate, file, line, extra_msg, buf);


        fprintf(s_log_file, "%s\n", isatty(fileno(s_log_file)) ? color_out_buf : out_buf);
        fflush(s_log_file);

        if (s_log_file != stdout) {
            printf("%s\n", isatty(fileno(stdout)) ? color_out_buf : out_buf);
            fflush(stdout);
        }

        va_end(va_ap);
        pthread_mutex_unlock(&s_mutex); //TODO: should check for zero ?
    }
}



