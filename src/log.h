#ifndef LOG_H
#define LOG_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define log_printf(...) do{ \
	/* don't let the first printf write errno in case __VA_ARGS__ contains errno */ \
	int log_printf_savno = errno; \
	fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
	errno = log_printf_savno; \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
	fflush(stderr); \
} while(0)

#define log_printf_abort(...) do { log_printf(__VA_ARGS__); abort(); } while(0)

#endif   // LOG_H
