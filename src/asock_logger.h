#ifndef ASOCK_LOGGER_H
#define ASOCK_LOGGER_H



#define TRACE_LEVEL_ERROR     0
#define TRACE_LEVEL_WARNING   1
#define TRACE_LEVEL_NORMAL    2
#define TRACE_LEVEL_INFO      3
#define TRACE_LEVEL_DEBUG     6


#define EXTRA_INFO __FILE__, __LINE__


#define TRACE_SYSTEM_ERROR    TRACE_LEVEL_ERROR, 1, EXTRA_INFO
#define TRACE_ERROR           TRACE_LEVEL_ERROR, 0, EXTRA_INFO
#define TRACE_WARNING         TRACE_LEVEL_WARNING, 0, EXTRA_INFO
#define TRACE_NORMAL          TRACE_LEVEL_NORMAL, 0, EXTRA_INFO
#define TRACE_INFO            TRACE_LEVEL_INFO, 0, EXTRA_INFO
#define TRACE_DEBUG           TRACE_LEVEL_DEBUG, 0, EXTRA_INFO

#define MAX_TRACE_LEVEL       6
#define TRACE_DEBUGGING       MAX_TRACE_LEVEL


#define DEFAULT_APPLICATION_TRACE_LEVEL TRACE_LEVEL_DEBUG


void set_trace_level(int id);
int get_trace_level();
void trace_event(int event_trace_level,
                 int add_strerr_info,
                 const char* file,
                 const int line,
                 const char * format,
                 ...);


#define SYS_LOG_ERROR(...)  trace_event(TRACE_SYSTEM_ERROR  , __VA_ARGS__)
#define LOG_ERROR(...)      trace_event(TRACE_ERROR         , __VA_ARGS__)
#define LOG_WARNING(...)	trace_event(TRACE_WARNING       , __VA_ARGS__)
#define LOG_NORMAL(...)		trace_event(TRACE_NORMAL        , __VA_ARGS__)
#define LOG_INFO(...)		trace_event(TRACE_INFO          , __VA_ARGS__)
#define LOG_DEBUG(...)		trace_event(TRACE_DEBUG         , __VA_ARGS__)

#endif // ASOCK_LOGGER_H
