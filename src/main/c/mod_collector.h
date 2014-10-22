/*******************************************************************************
 * Copyright (c) 2012 Hoang-Vu Dang <danghvu@gmail.com>
 * This file is part of mod_dumpost
 *
 * mod_dumpost is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mod_dumpost is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mod_dumpost. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "mod_collector_comms.h"

#ifndef __MOD_COLLECTOR__
#define __MOD_COLLECTOR__

#define DEFAULT_MAX_SIZE 1024*1024
#define min(a,b) (a)<(b)?(a):(b)

typedef struct {
    apr_pool_t *pool;
    apr_size_t max_post_size;
    apr_thread_mutex_t *mutex;
    server_connection *serverConnection;
} collector_module_state;


typedef struct {
    apr_pool_t *mp;
    int buffer_index;
    char *buffer;
} collector_request_state;


typedef enum {
    SESSION_VALID,
    SESSION_MD5_INVALID,
    SESSION_MISSING,
    SESSION_TIMEOUT
} cookie_session_state;


#define END_OF_MESSAGE           "\n"
#define SESSION_COOKIE           ":sc:"
#define SESSION_MD5              ":sd:"
#define SECRET                   "oPDgMd2$d5RW"
#define SESSION_TIMEOUT_SECONDS  3600
#define DEBUG(request, format, ...) ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, request, format, __VA_ARGS__);

#endif
