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
#include "mod_collector.h"

/*
     __  __           _  _____       _ _           _
    |  \/  |         | |/ ____|     | | |         | |
    | \  / | ___   __| | |      ___ | | | ___  ___| |_  ___  _ __
    | |\/| |/ _ \ / _` | |     / _ \| | |/ _ \/ __| __|/ _ \| '__|
    | |  | | (_) | (_| | |____| (_) | | |  __/ (__| |_| (_) | |
    |_|  |_|\___/ \__,_|\_____|\___/|_|_|\___|\___|\__|\___/|_|

    Collects information about a request/response and sends the information
    in JSON format to a listener process so that it can be persisted and
    processed.
    [Pete Hanlon]
*/



typedef enum ConnectionStatus {
    DISCONNECTED,
    CONNECTED,
    FAILED
} ConnectionStatus;


typedef struct {
    char *collectorHostIP;
    int  collectorPort;
    ConnectionStatus connectionStatus;
    apr_thread_mutex_t *mutex;
    apr_socket_t *sock;
    long lastFailure;
} GlobalContext;




typedef enum SessionState {
    SESSION_VALID,
    SESSION_MD5_INVALID,
    SESSION_MISSING,
    SESSION_TIMEOUT
} SessionState;


// Global context information i.e. global data
static GlobalContext context;

// Used to lookup the context from user data
const char *context_key = CONTEXT_POOL_KEY;



static const char* setCollectorPort(cmd_parms* cmd, void* cfg, const char* val)
{
    context.collectorPort = atoi(val);
    return NULL ;
}


static const char* setCollectorHost(cmd_parms* cmd, void* cfg, const char* val)
{
    context.collectorHostIP = (char *)val;
    return NULL ;
}



/*
 * Calculate an MD5 of the payload + the secret
 */
char* calcSecureMD5(request_rec* r, char* payload) {
    char* secretPayload = apr_psprintf(r->pool, "%s-%s", payload, SECRET);
    return(ap_md5(r->pool, (unsigned char *)secretPayload));
}



/*
 * Create the session cookie
 */
void createSessionCookie(request_rec* r) {
    char buf[APR_UUID_FORMATTED_LENGTH + 1];
    time_t now = time(NULL);
    long timeStamp=now;
    apr_uuid_t uuid;

    // Create a UUID
    apr_uuid_get(&uuid);
    apr_uuid_format(buf, &uuid);

    // Create the session cookie value
    char* session_cookie = apr_psprintf(r->pool,"%ld|%s", timeStamp,buf);
    char* session_MD5 = calcSecureMD5(r, session_cookie);

    // Set the cookie
    setCookie(r,SESSION_COOKIE, session_cookie);
    setCookie(r,SESSION_MD5, session_MD5);
}



/*
 * Check if the session cookie is valid
 */
SessionState validateSessionCookie(request_rec* r) {
    Cookie *sessionCookie = getCookie(r, SESSION_COOKIE);
    Cookie *sessionMD5Cookie = getCookie(r, SESSION_MD5);

    if ((sessionCookie == NULL) || (sessionMD5Cookie == NULL)) {
        return SESSION_MISSING;
    }

    char *sessionCookieValue = sessionCookie->value;
    char *sessionMD5CookieValue = sessionMD5Cookie->value;

    if ((sessionCookieValue == NULL) || (sessionMD5CookieValue == NULL) ||
        (strlen(sessionCookieValue) ==0) || (strlen(sessionMD5CookieValue)==0)) {
        return SESSION_MISSING;
    }

    char* md5Check = calcSecureMD5(r,sessionCookieValue);

    if (strcmp(md5Check, sessionMD5CookieValue) != 0) {
        return SESSION_MD5_INVALID;
    }

    long sessionTimestamp = atol(sessionCookieValue);
    time_t now = time(NULL);
    long timeStamp=now;

    int timeDifference = ((now - sessionTimestamp) / 1000);

    if (timeDifference > SESSION_TIMEOUT_SECONDS) {
        return SESSION_TIMEOUT;
    }

    return SESSION_VALID;
}



/*
 * Convert the state enum into a text description
 */
char *getStateText(SessionState state) {
    char *stateTxt="UNKNOWN";

    switch(state) {
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
char *getSessionUUID(request_rec *r) {
    Cookie *c = getCookie(r, SESSION_COOKIE);
    if ((c == NULL) || (c->value == NULL)) return "";

    char *sessionCookie = apr_pstrdup(r->pool, c->value);

    char *uuid="";
    char *last;
    char *time = apr_strtok(sessionCookie, "|", &last);
    if (time != NULL) {
        uuid = apr_strtok(NULL, "|", &last);
        if (uuid == NULL) uuid = "";
    }

    return uuid;
}


char* tableToJSON(request_rec *r, const apr_table_t *tab) {
    char* ret = apr_pstrcat(r->pool, "\"headers\":[", NULL);

    if (tab != NULL) {
        const apr_array_header_t *tarr = apr_table_elts(tab);
        const apr_table_entry_t *telts = (const apr_table_entry_t*)tarr->elts;
        int i;

        for (i = 0; i < tarr->nelts; i++) {
            if (i>0) {
                ret = apr_pstrcat(r->pool, ret, ",", NULL);
            }

            ret = apr_pstrcat(r->pool, ret, "{\"", telts[i].key , "\":", "\"", (const char*)safeJSONString(r,telts[i].val), "\"}", NULL);
        }
    }

    ret = apr_pstrcat(r->pool, ret, "]", NULL);

    return ret;
}



/*
 * Build up a JSON string with the HTTP request/response info
 */
char* buildJSONMessage(request_rec *r, SessionState state) {

    apr_time_t now = apr_time_now();

    char *clientIP = "";
    char *userAgentIp = "";

    clientIP = getClientIP(r);
    userAgentIp = getUserAgentIP(r);

    char* ret = apr_psprintf(r->pool,"{\"hit\":{");

    ret = apr_pstrcat(r->pool, ret, "\"apacheTimestamp\":",apr_ltoa(r->pool, r->request_time) , ",", NULL);
    // mod_blocker sets a response header called X-Error if a request is blocked.
    // Didn't want the name to be too obvious that the request had been blocked by mod_blocker hence
    // the generic name.
    ret = apr_pstrcat(r->pool, ret, "\"blockedRequest\":\"",(const char*)safeJSONString(r,getResponseHeaderValue(r,"X-Error","0")) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"sessionMD5\":\"",getSessionUUID(r) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"sessionState\":\"",getStateText(state) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"elapseTimeMs\":",apr_itoa(r->pool, ((now - r->request_time)/1000)) , ",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"apacheHost\":\"",r->server->server_hostname, "\"" , NULL);

    ret = apr_pstrcat(r->pool, ret, "},", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"request\":{", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"clientIp\":\"",safeJSONString(r,clientIP) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"userAgentIp\":\"",safeJSONString(r,userAgentIp) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"urlHostname\":\"",safeJSONString(r,r->hostname) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"urlProtocol\":\"",safeJSONString(r,r->protocol) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"urlUri\":\"",safeJSONString(r,r->uri) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"urlArgs\":\"",safeJSONString(r,r->args) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"method\":\"",safeJSONString(r,r->method) , "\",", NULL);
    ret = apr_pstrcat(r->pool, ret, tableToJSON(r, r->headers_in), NULL);
    ret = apr_pstrcat(r->pool, ret, "},", NULL);

    ret = apr_pstrcat(r->pool, ret, "\"response\":{", NULL);
    ret = apr_pstrcat(r->pool, ret, "\"statusCode\":",apr_itoa(r->pool, r->status) , ",", NULL);
    ret = apr_pstrcat(r->pool, ret, tableToJSON(r, r->headers_out), NULL);
    ret = apr_pstrcat(r->pool, ret, "}}", NULL);

  return(ret);
}




void socketFailure() {
    if (context.sock != NULL) {
        apr_socket_close(context.sock);
    }

    apr_time_t now = apr_time_now();
    context.lastFailure = (now/1000);

    context.connectionStatus = FAILED;
    context.sock = NULL;
}



/*
 * Get a socket to the Java Collector server
 */
apr_socket_t* getSocket(request_rec *r)
{
    apr_sockaddr_t *sa;
    apr_status_t rv;
    int family = APR_INET;

    apr_time_t now = apr_time_now();
    long now_ms = (now/1000);

    // If the current socket is NULL i.e. no connection and the time since the last failure is
    // greater than 1 second, try to connect again.
    if ((context.sock == NULL) && ((now_ms - context.lastFailure) > 1000)) {
        rv = apr_sockaddr_info_get(&sa, context.collectorHostIP, family, context.collectorPort, 0, r->server->process->pool);

        if (rv != APR_SUCCESS) {
            if (context.connectionStatus != FAILED) {
                ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                "mod_collector: [1] listener socket not open, unable to get socket info");
            }
            socketFailure();
            return NULL;
        }

        rv = apr_socket_create(&context.sock, sa->family, SOCK_STREAM, APR_PROTO_TCP, r->server->process->pool);

        if (rv != APR_SUCCESS) {
            if (context.connectionStatus != FAILED) {
                ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                "mod_collector: [2] listener socket not open, unable to create socket");
            }
            socketFailure();
            return NULL;
        }

        rv = apr_socket_connect(context.sock, sa);

        if (rv != APR_SUCCESS) {
            if (context.connectionStatus != FAILED) {
                ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                "mod_collector: listener socket not open, unable to connect to socket [%s] [%d]",
                context.collectorHostIP, context.collectorPort);
            }
            socketFailure();
            return NULL;
        }

        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
        "mod_collector: Socket created to listener");

        context.connectionStatus = CONNECTED;
    }

    return context.sock;
}



/*
 * Write the JSON string containing request/response to the socket
 */
void socketWrite(request_rec *r, char *buffer)
{
    apr_socket_t *sock = getSocket(r);

    if (sock == NULL) {
        return;
    }

    // Send message
    apr_size_t eomSize = strlen(END_OF_MESSAGE);
    apr_size_t msgSize = strlen(buffer);

    int rc = apr_socket_send(sock, buffer, &msgSize);
    if (rc != 0) {
        socketFailure();
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
        "mod_collector: Failed to send message (1), status %d", rc);
        return;
    }

    rc = apr_socket_send(sock, END_OF_MESSAGE, &eomSize);
    if (rc != 0) {
        socketFailure();
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
        "mod_collector: Failed to send message (2), status %d", rc);
        return;
    }
}



/*
 * Set the context defaults, this is called before the apache httpd.conf file is loaded.
 * Any settings in the httpd.conf will override these defaults
 */
static int initializeContext(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp)
{
    context.collectorHostIP = apr_pstrdup(pconf,"localhost");
    context.collectorPort = 9090;
    context.connectionStatus = DISCONNECTED;
    context.sock = NULL;
    return OK;
}


/*
 * Called at startup, initialize the mutex used to lock critical sections of
 * code to stop threads clobbering each other. Initialize the area that will hold
 * the socket handle to the Collector Java process.
 */
static int initializationHandler(apr_pool_t *pconf, apr_pool_t *plog,
                               apr_pool_t *ptemp, server_rec *s)
{
    apr_status_t rc;

    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, s, "mod_collector: initializing");

    rc = apr_thread_mutex_create(&context.mutex, APR_THREAD_MUTEX_DEFAULT, pconf);

    if (rc != 0) {
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, s,
        "mod_collector: FAILED TO INITIALIZE");
        // Need to find the right return code for this, probably internal server error
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, s, "mod_collector: initialized");

    return OK;
}



/*
 * This is the collector module entry point called when a new request comes in.
 * At this point we simply check to see if the session cookie is set, if not we
 * create it. We can't do this in the logging handler as this fires after the response
 * has been sent to the client so there is no opportunity to set cookies.
 */
static int collectorRequestHandler(request_rec *r)
{
    apr_array_header_t* POST;
    SessionState state = validateSessionCookie(r);

    if (state != SESSION_VALID) {
        createSessionCookie(r);
    }

    return DECLINED;
}



/*
 * This method is called after the request has completed sending it's response to the browser
 . At this point we know how long the request took and the status code for the request. We log the
 * request here because it doesn't impact the time a browser takes to make a call.
 */
static int recordLoggerHandler(request_rec *r)
{
    // Check we have a valid session cookie
    SessionState state = validateSessionCookie(r);

    // Create the JSON request/response string to send to the
    // TrafficMgr process.
    char* responseJSON = buildJSONMessage(r, state);

    // Sending data on a socket isn't an atomic operation so
    // it's done in a critical section
    criticalSectionStart(r, context.mutex);
    socketWrite(r, responseJSON);
    criticalSectionEnd(r, context.mutex);

    return OK;
}



/*
 * We define the Apache hooks and methods that should be called here. This basically wires the
 * module up into Apache so that methods get called when the configuration is loaded, a new
 * request is made and when a request has completed.
 */
static void registerHooks(apr_pool_t *p) 
{
    // Called before the http config file has been read
    ap_hook_pre_config(initializeContext,NULL,NULL,APR_HOOK_LAST);

    // Called after the http config file has been read at startup
    ap_hook_post_config(initializationHandler,NULL,NULL,APR_HOOK_LAST);

    // Called when a HTTP request is made to Apache
    // We need to be the first module loaded so APR_HOOK_REALLY_FIRST or other
    // modules like mod_proxy will intercept the traffic and mod_collector wont
    // be called.
    ap_hook_handler(collectorRequestHandler, NULL, NULL, APR_HOOK_REALLY_FIRST);


    // Called after the HTTP response has completed for a given request
    // Log the request details after the HTML response has been transmitted
    ap_hook_log_transaction(recordLoggerHandler, NULL, NULL, APR_HOOK_REALLY_LAST);
}



static const command_rec moduleCommands[] = {
  AP_INIT_TAKE1("CollectorServerPort", setCollectorPort, NULL, RSRC_CONF, "Port to send all information"),
  AP_INIT_TAKE1("CollectorServerHost", setCollectorHost, NULL, RSRC_CONF, "Host to send all information"),
  { NULL }
};


/*
 * Apache structure needed to wire together the module.
 */
module AP_MODULE_DECLARE_DATA collector_module =
{ 
  STANDARD20_MODULE_STUFF, 
  NULL, /* per-directory config creator */ 
  NULL, /* directory config merger */ 
  NULL, /* server config creator */ 
  NULL, /* server config merger */ 
  moduleCommands, /* structure defining the config parameters in http.conf */
  registerHooks, /* method to wire together the module */
}; 

