#ifndef LOG_H
#define LOG_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

void log_init(void);

// https://stackoverflow.com/a/5459929
#define LOG_STR_HELPER(x) #x
#define LOG_STR(x) LOG_STR_HELPER(x)

#define log_printf(...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __FILE__ ":" LOG_STR(__LINE__) ": "__VA_ARGS__)
#define log_printf_abort(...) do { log_printf(__VA_ARGS__); abort(); } while(0)

#endif   // LOG_H
