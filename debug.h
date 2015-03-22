/*
 * Convenience logging functions.
 *
 * Author : Prashant Malani
 * Date:    03/14/2015
 */
#ifndef _DEBUG_H_
#define _DEBUG_H_
#include <stdio.h>

#define LOG_ERROR		3
#define LOG_INFO		2
#define LOG_DEBUG		1

#define LOG(level, ...) do { \
				if (level >= debug_level) { \
				printf(__VA_ARGS__); \
				} \
			} while (0)

#define LOGE(...)	LOG(LOG_ERROR, __VA_ARGS__)
#define LOGI(...)	LOG(LOG_INFO, __VA_ARGS__)
#define LOGD(...)	LOG(LOG_DEBUG, __VA_ARGS__)

extern int debug_level;

#endif
