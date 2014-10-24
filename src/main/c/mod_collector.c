/*
 The MIT License (MIT)

 Copyright (c)2014 Peter Hanlon

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

     __  __           _  _____       _ _           _
    |  \/  |         | |/ ____|     | | |         | |
    | \  / | ___   __| | |      ___ | | | ___  ___| |_  ___  _ __
    | |\/| |/ _ \ / _` | |     / _ \| | |/ _ \/ __| __|/ _ \| '__|
    | |  | | (_) | (_| | |____| (_) | | |  __/ (__| |_| (_) | |
    |_|  |_|\___/ \__,_|\_____|\___/|_|_|\___|\___|\__|\___/|_|

    Collects information about a request/response and sends the information
    in JSON format to a listener process so that it can be persisted and
    processed.
*/

#include <http_protocol.h>
#include <apr_strings.h>
#include <apr_uuid.h>
#include "http_config.h"
#include "http_log.h"
#include "http_request.h"
#include "mod_collector.h"
#include "common.h"


module AP_MODULE_DECLARE_DATA collector_module;



collector_module_state *get_module_configuration(request_rec *r) {
    // Load the module configuration
    collector_module_state *cfg =
            (collector_module_state *) ap_get_module_config(r->per_dir_config, &collector_module);
    return cfg;
}


collector_request_state *get_request_state(request_rec *r) {
    // Load the module configuration
    collector_request_state *state =
            (collector_request_state *) ap_get_module_config(r->request_config, &collector_module);
    return state;
}


/*
 * Create the session cookie
 */
static void create_session_cookie(request_rec *r) {
    char buf[APR_UUID_FORMATTED_LENGTH + 1];
    time_t now = time(NULL);
    long timeStamp = now;
    apr_uuid_t uuid;

    // Create a UUID
    apr_uuid_get(&uuid);
    apr_uuid_format(buf, &uuid);

    // Create the session cookie value
    char *session_cookie = apr_psprintf(r->pool, "%ld|%s", timeStamp, buf);
    char *session_MD5 = calc_secure_MD5(r, SECRET, session_cookie);

    // Set the cookie
    setCookie(r, SESSION_COOKIE, session_cookie);
    setCookie(r, SESSION_MD5, session_MD5);
}


/*
 * Check if the session cookie is valid
 */
static cookie_session_state validate_session_cookie(request_rec *r) {
    Cookie *session_cookie = getCookie(r, SESSION_COOKIE);
    Cookie *session_MD5_cookie = getCookie(r, SESSION_MD5);

    if ((session_cookie == NULL) || (session_MD5_cookie == NULL)) {
        return SESSION_MISSING;
    }

    char *session_cookie_value = session_cookie->value;
    char *session_MD5_cookie_value = session_MD5_cookie->value;

    if ((session_cookie_value == NULL) || (session_MD5_cookie_value == NULL) ||
            (strlen(session_cookie_value) == 0) || (strlen(session_MD5_cookie_value) == 0)) {
        return SESSION_MISSING;
    }

    char *md5Check = calc_secure_MD5(r, SECRET, session_cookie_value);

    if (strcmp(md5Check, session_MD5_cookie_value) != 0) {
        return SESSION_MD5_INVALID;
    }

    long session_timestamp = atol(session_cookie_value);
    time_t now = time(NULL);

    int timeDifference = ((now - session_timestamp) / 1000);

    if (timeDifference > SESSION_TIMEOUT_SECONDS) {
        return SESSION_TIMEOUT;
    }

    return SESSION_VALID;
}


/*
 * Convert the state enum into a text description
 */
static char *get_state_text(cookie_session_state state) {
    char *stateTxt = "UNKNOWN";

    switch (state) {
        case SESSION_VALID:
            stateTxt = "VALID";
            break;
        case SESSION_MD5_INVALID:
            stateTxt = "MD5_INVALID";
            break;
        case SESSION_MISSING:
            stateTxt = "MISSING";
            break;
        case SESSION_TIMEOUT:
            stateTxt = "TIMEOUT";
            break;
    }

    return stateTxt;
}


/*
 * Extract the session UUID from the session cookie
 */
static char *get_session_UUID(request_rec *r) {
    Cookie *c = getCookie(r, SESSION_COOKIE);
    if ((c == NULL) || (c->value == NULL)) return "";

    char *session_cookie = apr_pstrdup(r->pool, c->value);

    char *uuid = "";
    char *last;
    char *time = apr_strtok(session_cookie, "|", &last);
    if (time != NULL) {
        uuid = apr_strtok(NULL, "|", &last);
        if (uuid == NULL) uuid = "";
    }

    return uuid;
}


/*
 * Build up a JSON string with the HTTP request/response info
 */
static char *build_JSON_message(request_rec *r) {
    apr_time_t now = apr_time_now();

    // Load the request state
    collector_request_state *request_state = get_request_state(r);

    // Check to see if we have a valid session cookie
    cookie_session_state cookie_sess_state = validate_session_cookie(r);

    // Check to see if this request has been blocked
    // mod_blocker sets a response header called X-Error if a request is blocked.
    char *x_error_status = getResponseHeaderValue(r, "X-Error", "0");

    char *client_ip = getClientIP(r);
    char *user_agent_ip = getUserAgentIP(r);

    char *ret = apr_psprintf(r->pool, "{\"hit\":{");
    ret = apr_pstrcat(r->pool, ret, "\"apacheTimestamp\":", apr_ltoa(r->pool, r->request_time), ",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"blockedRequest\":\"", (const char *) safeJSONString(r, x_error_status), "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"sessionMD5\":\"", get_session_UUID(r), "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"sessionState\":\"", get_state_text(cookie_sess_state), "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"elapseTimeMs\":", apr_itoa(r->pool, ((now - r->request_time) / 1000)), ",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"apacheHost\":\"", r->server->server_hostname, "\"", NULL);
    ret = apr_pstrcat(r->pool, ret, "},", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"request\":{", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"ip\":{", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"client\":\"", safeJSONString(r, client_ip), "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"userAgent\":\"", safeJSONString(r, user_agent_ip), "\"" , NULL);
    ret = apr_pstrcat(r->pool, ret, "},", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"protocol\":\"", safeJSONString(r, r->protocol), "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"url\":\"", safeJSONString(r, r->unparsed_uri), "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"method\":\"", safeJSONString(r, r->method), "\",", NULL);

    if (request_state->buffer_index > 0) {
        ret = apr_pstrcat(r->pool, ret, "\"postData\":\"", request_state->buffer, "\",", NULL);
    }

    ret = apr_pstrcat(r->pool, ret, tableToJSON(r, r->headers_in, "headers"), NULL);
    ret = apr_pstrcat(r->pool, ret, "},", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"response\":{", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"statusCode\":", apr_itoa(r->pool, r->status), ",", NULL);
    ret = apr_pstrcat(r->pool, ret, tableToJSON(r, r->headers_out, "headers"), NULL);
    ret = apr_pstrcat(r->pool, ret, "}}", NULL);

    return (ret);
}





/*
 * This method is called after the request has completed sending it's response to the browser
 . At this point we know how long the request took and the status code for the request. We log the
 * request here because it doesn't impact the time a browser takes to make a call.
 */
static int write_request_JSON(request_rec *r) {
    collector_module_state *cfg = get_module_configuration(r);

    // Create the JSON request/response string to send to the
    // TrafficMgr process.
    char *response_JSON = build_JSON_message(r);

    // Multiple threads could be trying to write to the socket
    // at the same time, so wrap in a critical section
    criticalSectionStart(r, cfg->mutex);
    socket_write(cfg->pool, cfg->serverConnection, r, response_JSON);
    criticalSectionEnd(r, cfg->mutex);

    return OK;
}


static void append_post_data(request_rec *r, apr_bucket *b, char *buf, apr_size_t *current_post_size) {

    collector_module_state *module_state = get_module_configuration(r);

    if (*current_post_size < module_state->max_post_size && !(APR_BUCKET_IS_METADATA(b))) {
        const char *ibuf;
        apr_size_t nbytes;

        if (apr_bucket_read(b, &ibuf, &nbytes, APR_BLOCK_READ) == APR_SUCCESS) {
            if (nbytes) {
                DEBUG(r, "%ld bytes read from bucket for request %s", nbytes, r->the_request);
                nbytes = min(nbytes, module_state->max_post_size - *current_post_size);
                strncpy(buf, ibuf, nbytes);
                *current_post_size += nbytes;
            }
        } else {
            ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
                    "mod_dumpost: error reading data");
        }
    }
    else {
        if (APR_BUCKET_IS_EOS(b)) {
            DEBUG(r, "EOS bucket detected for request %s", r->the_request);
        }
    }
}



/*
 * Called at the start of the request handler chain
 * At this point we simply check to see if the session cookie is set, if not we
 * create it. We can't do this in the logging handler as this fires after the response
 * has been sent to the client so there is no opportunity to set cookies.
 */
static int check_session_cookie_is_set(request_rec *r) {
    cookie_session_state session_state = validate_session_cookie(r);

    if (session_state != SESSION_VALID) {
        create_session_cookie(r);
    }

    return DECLINED;
}



static int initialise_request_state(request_rec *r) {
    // Load the module config
    collector_module_state *module_state = get_module_configuration(r);

    // Create a structure to hold the request information
    collector_request_state *request_state = (collector_request_state *) apr_palloc(r->pool, sizeof *request_state);
    request_state->buffer_index = 0;
    request_state->buffer = apr_palloc(r->pool, module_state->max_post_size + 1);
    request_state->buffer[0] = '\0';

    // Store the request_state structure against the request_config, this will be automatically
    // cleaned up once the request had been processed
    ap_set_module_config(r->request_config, &collector_module, request_state);

    return APR_SUCCESS;
}




apr_status_t post_input_filter(ap_filter_t *f, apr_bucket_brigade *bb,
        ap_input_mode_t mode, apr_read_type_e block, apr_off_t readbytes) {
    apr_bucket *b;
    apr_status_t ret;

    // Load the module configuration
    collector_module_state *module_state = get_module_configuration(f->r);

    // Load the request state
    collector_request_state *request_state = get_request_state(f->r);

    char *buf = request_state->buffer;
    apr_size_t buf_index = request_state->buffer_index;

    if ((ret = ap_get_brigade(f->next, bb, mode, block, readbytes)) != APR_SUCCESS)
        return ret;

    DEBUG(f->r, "Start brigade for request: %s", f->r->the_request)
    for (b = APR_BRIGADE_FIRST(bb); b != APR_BRIGADE_SENTINEL(bb); b = APR_BUCKET_NEXT(b))
        append_post_data(f->r, b, buf + buf_index, &buf_index);
    DEBUG(f->r, "End brigade for request: %s, buffer: %ld bytes", f->r->the_request, buf_index)

    request_state->buffer_index = buf_index;

    return APR_SUCCESS;
}


static void post_collector_insert_filter(request_rec *req) {
    ap_add_input_filter("COLLECTOR_IN", NULL, req, req->connection);
}


static const char *set_collector_post_max_size(cmd_parms *cmd, void *_cfg, const char *arg) {
    collector_module_state *module_state = (collector_module_state *) _cfg;
    module_state->max_post_size = atoi(arg);
    if (module_state->max_post_size == 0)
        module_state->max_post_size = DEFAULT_MAX_SIZE;
    return NULL;
}


static const char *set_collector_port(cmd_parms *cmd, void *_cfg, const char *arg) {
    collector_module_state *module_state = (collector_module_state *) _cfg;
    module_state->serverConnection->server_port = atoi(arg);
    return NULL;
}


static const char *set_collector_host(cmd_parms *cmd, void *_cfg, const char *arg) {
    collector_module_state *module_state = (collector_module_state *) _cfg;
    module_state->serverConnection->server_ip = (char *) arg;
    return NULL;
}



/*
 * Called when the module is being unloaded
 */
static apr_status_t cleanup_module_state(void *data) {
    ap_log_error(APLOG_MARK, APLOG_NOERRNO | APLOG_NOTICE, 0, 0, "Cleaning up module state");
    collector_module_state *module_state = (collector_module_state*) data;
    return APR_SUCCESS;
}



/*
 * Called when the module is being loaded
 */
static void *create_module_state(apr_pool_t *mp, char *path) {
    collector_module_state *module_state = apr_pcalloc(mp, sizeof(collector_module_state));

    module_state->pool = mp;
    module_state->max_post_size = DEFAULT_MAX_SIZE;
    module_state->serverConnection = (server_connection *) apr_palloc(mp, sizeof *module_state->serverConnection);
    initialize_connection(module_state->serverConnection,"127.0.0.1",9090);

    apr_status_t rc = apr_thread_mutex_create(&module_state->mutex, APR_THREAD_MUTEX_DEFAULT, mp);

    if (rc != 0) {
        ap_log_error(APLOG_MARK, APLOG_NOERRNO | APLOG_NOTICE, 0, 0,
                "mod_collector: FAILED TO INITIALIZE MUTEX");
    }

    apr_pool_cleanup_register(mp, module_state, cleanup_module_state, cleanup_module_state);
    return module_state;
}



static void register_hooks(apr_pool_t *p) {
    ap_register_input_filter("COLLECTOR_IN", post_input_filter, NULL, AP_FTYPE_CONTENT_SET);

    // Called when the request is recieved, the connection has been accepted but no other
    // request based event has fired yet i.e. the start of the request.
    ap_hook_post_read_request(initialise_request_state, NULL, NULL, APR_HOOK_FIRST);

    // Hooks into the input stream containing post data, this allows us to sample
    // the incoming post data.
    ap_hook_insert_filter(post_collector_insert_filter, NULL, NULL, APR_HOOK_FIRST);

    // We need to be the first module loaded so APR_HOOK_REALLY_FIRST or other
    // modules like mod_proxy will intercept the traffic and mod_collector wont
    // be called.
    ap_hook_handler(check_session_cookie_is_set, NULL, NULL, APR_HOOK_REALLY_FIRST);


    // Called after the HTTP response has completed for a given request
    // Log the request details after the HTML response has been transmitted
    ap_hook_log_transaction(write_request_JSON, NULL, NULL, APR_HOOK_REALLY_LAST);

}



static const command_rec mod_collector_cmds[] = {
        AP_INIT_TAKE1("CollectorPostMaxSize", set_collector_post_max_size, NULL, RSRC_CONF, "Set maximum data size"),
        AP_INIT_TAKE1("CollectorServerPort", set_collector_port, NULL, RSRC_CONF, "Port to send all information"),
        AP_INIT_TAKE1("CollectorServerHost", set_collector_host, NULL, RSRC_CONF, "Host to send all information"),
        {NULL}
};



module AP_MODULE_DECLARE_DATA collector_module = {
        STANDARD20_MODULE_STUFF,
        create_module_state, // Called when module is loaded
        NULL,
        NULL,
        NULL,
        mod_collector_cmds,
        register_hooks
};
