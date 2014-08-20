#include <stdio.h>
#include <netdb.h>
#include <time.h>
#include <ctype.h>
#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_protocol.h"
#include "http_request.h"
#include "util_script.h"
#include "http_connection.h"
#include "ap_config.h"
#include "apr_general.h"
#include "apr_pools.h"
#include "apr_hash.h"
#include "apr_strings.h"
#include "apr_atomic.h"
#include "apr_anylock.h"
#include "apr_errno.h"
#include "util_md5.h"
#include "apr_uuid.h"

#include "common.h"


/*
 * Try to lock a mutext, used to enter a critical code section
 */
void criticalSectionStart(request_rec *r, apr_thread_mutex_t *mutex) {
    apr_status_t rv;
    rv = apr_thread_mutex_lock(mutex);

    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
        "mod_collector: Mutex lock failed");
    }
}


/*
 * Try to unlock a mutext, used to release a critical code section
 */
void criticalSectionEnd(request_rec *r, apr_thread_mutex_t *mutex) {
    apr_status_t rv;
    rv = apr_thread_mutex_unlock(mutex);

    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
        "mod_collector: Mutex unlock failed");
    }
}


/*
 * Returns a value from the HTTP request header
 */
char *getRequestHeaderValue(request_rec *r, char *key, char *nullval) {
    char* ret = (char *)apr_table_get(r->headers_in, key);
    if (ret == NULL) return nullval;
    return ret;
}




/*
 * Returns a value from the HTTP response header
 */
char *getResponseHeaderValue(request_rec *r, char *key, char *nullval) {
    char* ret = (char *)apr_table_get(r->headers_out, key);
    if (ret == NULL) return nullval;
    return ret;
}



/*
 * Escape a string so that it can be used in a
 * JSON payload.
 */
char* safeJSONString(request_rec* r, const char* text) {
  if (text == NULL || strlen(text) == 0) return "";

  char *oldString = apr_pstrdup(r->pool, text);
  char *newString = apr_palloc(r->pool, (strlen(oldString)*3));

  int newIndx=0;
  int oldIndx=0;

  for (; oldIndx<strlen(oldString); oldIndx++) {
    if (oldString[oldIndx] == '"' || oldString[oldIndx] == '\\') {
        if (oldString[oldIndx] == '"') {
            newString[newIndx++]='\\';
            newString[newIndx++]='"';
        }
        if (oldString[oldIndx] == '\\') {
            newString[newIndx++]='\\';
            newString[newIndx++]='\\';
        }
    } else {
        newString[newIndx++] = oldString[oldIndx];
    }
  }
  newString[newIndx++] ='\0';

  return(newString);
}



/*
 * Set the value of a cookie
 */
void setCookie(request_rec *r, char* cookieName, char* cookieValue) {
    char *newCookie = apr_pstrcat(r->pool, cookieName,"=",cookieValue,"; Path=/; HttpOnly;",NULL);
    apr_table_addn(r->headers_out,"Set-Cookie",newCookie);
}


/*
 * Get the value of a cookie
 */
Cookie *getCookie(request_rec *r, const char *name) {
    const char *cookies;
    const char *start_cookie;

    if ((cookies = apr_table_get(r->headers_in, "Cookie"))) {
        for (start_cookie = ap_strstr_c(cookies, name); start_cookie;
             start_cookie = ap_strstr_c(start_cookie + 1, name)) {
            if (start_cookie == cookies ||
                start_cookie[-1] == ';' ||
                start_cookie[-1] == ',' ||
                isspace(start_cookie[-1])) {

                start_cookie += strlen(name);
                while(*start_cookie && isspace(*start_cookie))
                    ++start_cookie;
                if (*start_cookie == '=' && start_cookie[1]) {
                    /*
                     * Session cookie was found, get it's value
                     */
                    char *end_cookie, *cookie;
                    ++start_cookie;
                    cookie = apr_pstrdup(r->pool, start_cookie);
                    if ((end_cookie = strchr(cookie, ';')) != NULL)
                        *end_cookie = '\0';
                    if((end_cookie = strchr(cookie, ',')) != NULL)
                        *end_cookie = '\0';

                    Cookie* ret = (Cookie *)apr_palloc(r->pool, sizeof(Cookie));
                    ret->name=apr_pstrdup(r->pool, name);
                    ret->value=apr_pstrdup(r->pool, cookie);
                    return ret;
                }
            }
        }
    }
    return NULL;
}



char* getClientIP(request_rec *r)
{
    return(r->connection->client_ip);
}


char* getUserAgentIP(request_rec *r)
{
    return(r->useragent_ip);
}
