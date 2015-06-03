#ifndef ___FZ_TRACE_H
#define ___FZ_TRACE_H

#include "fz_common.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define USE_FZ_TRACE

namespace {

	enum LOG_LEVEL{
		FZ_LOG_DEFAULT = 1,
		FZ_LOG_INFO = 2,
		FZ_LOG_ERROR = 4,
		FZ_LOG_DEBUG = 8,
	};

	FILE* out_buffer = stdout;

	const int debug_level = 
		FZ_LOG_DEFAULT |
		FZ_LOG_INFO |
		FZ_LOG_ERROR |
		FZ_LOG_DEBUG 
		;
}

FZ_BEGIN

static void TRACE(LOG_LEVEL lv, const char *fmt, ...) 
{
#ifndef USE_FZ_TRACE
	return;
#else
	va_list list;
	char *s;
	char c;
	int i;

	switch(lv)
	{
		case FZ_LOG_INFO:
			if( !(debug_level & FZ_LOG_INFO) ) {
				return;
			}
			printf("[INFO]");
			break;
		case FZ_LOG_ERROR:
			if( !(debug_level & FZ_LOG_ERROR) ) {
				return;
			}
			printf("[ERROR]");
			break;
		case FZ_LOG_DEBUG:
			if( !(debug_level & FZ_LOG_DEBUG) ) {
				return;
			}
			printf("[DEBUG]");
			break;
		case FZ_LOG_DEFAULT:
			if( !(debug_level & FZ_LOG_DEFAULT) ) {
				return;
			}
			printf("[DEFAULT]");
			break;
		default:
			return;
	}
	va_start(list, fmt);

	while(*fmt){
		if(*fmt != '%') {
			putc(*fmt, out_buffer);
		}
		else {
			switch(*++fmt) {
				case 's':
					s = va_arg(list, char*);
					//fprintf(out_buffer, "%s", s);
					printf("%s", s);
					break;
				case 'd':
					i = va_arg(list, int);
					//fprintf(out_buffer, "%d", i);
					printf("%d", i);
					break;
				case 'x':
					i = va_arg(list, int);
					//fprintf(out_buffer, "%d", i);
					printf("%x", i);
					break;
				case 'c':
					c = va_arg(list, int);
					//fprintf(out_buffer, "%c", c);
					printf("%c", c);
					break;
				default:
					putc(*fmt, out_buffer);
					break;
			}
		}
		++fmt;
	}
	va_end(list);
	fflush(out_buffer);
#endif
}

FZ_END

#endif
