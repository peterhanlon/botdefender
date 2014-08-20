#include <stdio.h>
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
#include <regex.h>

#include "common.h"
#include "mod_collector.h"

#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif


/*
     __  __           _ ____  _            _
    |  \/  |         | |  _ \| |          | |
    | \  / | ___   __| | |_) | | ___   ___| | __ ___ _ __
    | |\/| |/ _ \ / _` |  _ <| |/ _ \ / __| |/ // _ \ '__|
    | |  | | (_) | (_| | |_) | | (_) | (__|   <|  __/ |
    |_|  |_|\___/ \__,_|____/|_|\___/ \___|_|\_\\___|_|

    Blocks incoming requests based on the requestors IP address,
    Session ID or the URL fragment
    [Pete Hanlon]
*/

#define BLOCK_ACTION_ARRAY_SIZE 1000
#define MAX_ITEM_LENGTH 100
#define MOD_BLOCKER_INITIALIZED_KEY "mod_blocker_initialized"

typedef enum BlockListCommand {
    SET_BLOCK_LIST_CMD,
    GET_BLOCK_LIST_CMD,
    UNKNOWN_CMD
} BlockListCommand;


typedef struct {
    char *blockCommandPath;
    apr_thread_mutex_t *mutex;
    char *shmfilename;
    apr_shm_t *shm;
} GlobalContext;


typedef struct {
    char blockAction[MAX_ITEM_LENGTH];
    char blockValue[MAX_ITEM_LENGTH];
} BlockAction;


static GlobalContext context;


static const char* setBlockCommandPath(cmd_parms* cmd, void* cfg, const char* val)
{
    context.blockCommandPath = (char *)val;
    return NULL ;
}



void clearSharedMemory() {
    BlockAction *base = (BlockAction *)apr_shm_baseaddr_get(context.shm);
    int x=0;
    for (x=0; x<BLOCK_ACTION_ARRAY_SIZE; x++) {
        strcpy(base[x].blockAction, "");
        strcpy(base[x].blockValue, "");
    }
}



static apr_status_t shmCleanupWrapper(void *unused) {
    if (context.shm)
        return apr_shm_destroy(context.shm);
    return OK;
}



char* getPostData(request_rec *r)
{
    // Load the POST data into the postData string
    ap_setup_client_block(r, REQUEST_CHUNKED_DECHUNK);
    char *buffer = apr_pcalloc(r->pool, 1024);
    char *postData=apr_pstrdup(r->pool, "");
    if ( ap_should_client_block(r) == 1 ) {
        while ( ap_get_client_block(r, buffer, 1024) > 0 ) {
             postData = apr_pstrcat(r->pool, postData, buffer, NULL);
             memset(buffer,0,1024);
        }
    }

    return(postData);
}



void storeBlockActionsInSharedMemory(request_rec *r, char *postData)
{
    // Clear the shared memory that stored the actions to block
    clearSharedMemory();

    // The post command returns a textual description of what mod_block has done
    // to the calling client so set the content type to text/plain
    ap_set_content_type(r, "text/plain");

    // The post commands are sent as IP=<ipaddress>&SESSION=<session>&URLFRAG=<string>
    // Each action to block e.e. (IP=<ipaddress>) are placed into the shared memory array.
    BlockAction *base = (BlockAction *)apr_shm_baseaddr_get(context.shm);
    int blockCmdCount=0;

    char *lastBlockCmd;
    char *blockCmd = apr_strtok( apr_pstrdup(r->pool,postData), "&", &lastBlockCmd);
    while (blockCmd != NULL && blockCmdCount < BLOCK_ACTION_ARRAY_SIZE) {

        char *lastToken;
        char *blockAction = apr_strtok( apr_pstrdup(r->pool,blockCmd), "=", &lastToken);
        if (blockAction != NULL) {

            char* blockValue =  apr_strtok( NULL, "=", &lastToken);
            if (blockValue != NULL) {
                // Place the action to block in shared memory
                strcpy(base[blockCmdCount].blockAction, blockAction);
                strcpy(base[blockCmdCount].blockValue, blockValue);

                // Put a message into the HTTP response stream so the client posting
                // the data knows what happened
                ap_rputs(apr_psprintf(r->pool,"Actively blocking %s\n",blockCmd), r);

                ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                        "mod_blocker: Blocking [%s %s]",blockAction, blockValue);
            }
        }

        blockCmd = apr_strtok( NULL, "&", &lastBlockCmd);
        blockCmdCount++;
    }

    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
    "mod_blocker: items being blocked %d",blockCmdCount);
}





void processBlockCommands(request_rec *r) {
    // Get the posted data as a string
    char* blockActionPostData = getPostData(r);

    // POST data is in the format IP=127.0.0.1, SESSION=<sessionID> ..
    storeBlockActionsInSharedMemory(r, blockActionPostData);
}



/*
 * Check to see if the activity should be blocked or not
 */
int isBlockedActivity(request_rec *r)
{
    BlockAction *base = (BlockAction *)apr_shm_baseaddr_get(context.shm);
    Cookie *sessionCookie = getCookie(r, SESSION_COOKIE);
    char *client_ip=getClientIP(r);
    char *url = r->uri;

    int indx=0;
    while (indx<BLOCK_ACTION_ARRAY_SIZE) {
        if (strlen((char*)base[indx].blockAction) >0 && strlen((char*)base[indx].blockValue) >0) {
            if (strcmp((char*)base[indx].blockAction, "IP") == 0) {
                if (client_ip != NULL && strcmp(client_ip, (char *)base[indx].blockValue) == 0) {
                    return TRUE;
                }
            }

            if (strcmp((char*)base[indx].blockAction, "SESSION") == 0) {
                if (sessionCookie != NULL &&
                    sessionCookie->value != NULL &&
                    strcmp(sessionCookie->value, (char *)base[indx].blockValue) == 0) {
                    return TRUE;
                }
            }

            if (strcmp((char*)base[indx].blockAction, "URLFRAGMENT") == 0) {
                if (url != NULL && strcasestr(url, (char *)base[indx].blockValue) != NULL) {
                    return TRUE;
                }
            }
        }

        indx++;
    }

    return FALSE;
}



void getBlockList(request_rec *r) {
    BlockAction *base = (BlockAction *)apr_shm_baseaddr_get(context.shm);

    int indx=0;
    while (indx<BLOCK_ACTION_ARRAY_SIZE) {
        if (strlen((char *)base[indx].blockAction) > 0 && strlen((char *)base[indx].blockValue) > 0) {
            ap_rputs(apr_psprintf(r->pool,"Actively blocking %s %s\n",(char *)base[indx].blockAction,(char *)base[indx].blockValue), r);
        }
        indx++;
    }
}



/*
 * Check to see if the incoming requset is a
 * Format of URL : http://localhost/<block command path>?op=set
 * Format of URL : http://localhost/<block command path>?op=list
 */
BlockListCommand parseBlockListCommandURL(request_rec *r)
{
    if (strcmp(r->method,"PUT") == 0) {
        if (strncmp(context.blockCommandPath, r->uri, strlen(context.blockCommandPath)) == 0) {
            return SET_BLOCK_LIST_CMD;
        }
    }


    if (strcmp(r->method,"GET") == 0) {
        if (strncmp(context.blockCommandPath, r->uri, strlen(context.blockCommandPath)) == 0) {
            return GET_BLOCK_LIST_CMD;
        }
    }

    return UNKNOWN_CMD;
}



/*
 * This is the blocker module entry point
 */
static int blockerHandler(request_rec *r)
{
    criticalSectionStart(r, context.mutex);

    BlockListCommand cmd = parseBlockListCommandURL(r);

    if (cmd == SET_BLOCK_LIST_CMD) {
        processBlockCommands(r);
        criticalSectionEnd(r, context.mutex);
        return OK;
    }

    if (cmd == GET_BLOCK_LIST_CMD) {
        getBlockList(r);
        criticalSectionEnd(r, context.mutex);
        return OK;
    }

    if (isBlockedActivity(r) == TRUE) {
        apr_table_addn(r->headers_out,"X-Error","1");
        apr_table_addn(r->headers_out,"Cache-Control","no-cache, no-store, max-age=0, no-transform, private");
        criticalSectionEnd(r, context.mutex);
        return HTTP_NO_CONTENT;
    }

    criticalSectionEnd(r, context.mutex);

    return DECLINED;
}



static int initializeContext(apr_pool_t *pconf, apr_pool_t *plog,
                               apr_pool_t *ptemp, server_rec *s)
{
    context.shmfilename = NULL;
    context.blockCommandPath = apr_pstrdup(pconf,"/_block_list");
    return OK;
}




static int initializationHandler(apr_pool_t *pconf, apr_pool_t *plog,
                               apr_pool_t *ptemp, server_rec *server)
{
    apr_status_t rv;
    const char *tempdir;

    /*
     * Do nothing if we are not creating the final configuration.
     * The parent process gets initialized a couple of times as the
     * server starts up, and we don't want to create any more mutexes
     * and shared memory segments than we're actually going to use.
     * Note ap_state_query only works on versions of Apache > 2.2
     */
#if (AP_SERVER_MAJORVERSION_NUMBER >= 2 && AP_SERVER_MAJORVERSION_NUMBER > 2)
        if (ap_state_query(AP_SQ_MAIN_STATE) == AP_SQ_MS_CREATE_PRE_CONFIG)
            return OK;
#endif


    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, server,
            "mod_blocker: initializing");

    // Get the temporary directory location for this box
    rv = apr_temp_dir_get(&tempdir, pconf);

    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, server,
                     "mod_blocker:Failed to find temporary directory");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // Attempt to attach to to the shared memory segment
    context.shmfilename = apr_psprintf(pconf, "%s/apache_mod_blocker.shm", tempdir);
    rv = apr_shm_attach(&context.shm, (const char *) context.shmfilename, pconf);

    // If that failed try to create a new segment
    if (rv != APR_SUCCESS) {
        rv = apr_shm_create(&context.shm, (sizeof(BlockAction) * BLOCK_ACTION_ARRAY_SIZE),
                            (const char *) context.shmfilename, pconf);

        if (rv != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, server,
                         "mod_blocker:Failed to create shared memory segment on file [%s] Apache can't start. "
                         "Remove the [LoadMdule modules/mod_blocker] line from httpd.conf if this persists.",
                         context.shmfilename);
            return HTTP_INTERNAL_SERVER_ERROR;
        } else {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, server,"Attaching to existing shared memory segment");
        }
    } else {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, server,"Creating a new shared memory segment");
    }

    /* Create the mutex used when accessing the shared memory */
    rv = apr_thread_mutex_create(&context.mutex, APR_THREAD_MUTEX_DEFAULT, pconf);

    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, server,
                     "mod_blocker:Failed to create mutex");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // Remove the shared memory when the pool pconf is destroyed
    apr_pool_cleanup_register(pconf, NULL, shmCleanupWrapper,
                              apr_pool_cleanup_null);

    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, server,
                     "mod_blocker:Failed to schedule shared memory cleanup");
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    // Initialize the shared memory
    clearSharedMemory();

    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, server, "mod_blocker: initialized");

    return OK;
}




/* Each function our module provides to handle a particular hook is 
* specified here. See mod_example.c for more information about this
* step. Suffice to say that we need to list all of our handlers in 
* here. */ 
static void registerHooks(apr_pool_t *p) 
{
    // Called before the http config file has been read
    ap_hook_post_config(initializeContext,NULL,NULL,APR_HOOK_LAST);

    // Called after the http config file has been read at startup
    ap_hook_post_config(initializationHandler, NULL, NULL, APR_HOOK_FIRST);

    // We need to be the first module loaded so APR_HOOK_REALLY_FIRST or other
    // modules like mod_proxy will intercept the traffic and mod_blocker wont
    // be called.
    ap_hook_handler(blockerHandler, NULL, NULL, APR_HOOK_REALLY_FIRST);
}



static const command_rec moduleCommands[] = {
  AP_INIT_TAKE1("BlockCommandPath", setBlockCommandPath, NULL, RSRC_CONF, "Path used by mod_blocker to accept incoming commands"),
  { NULL }
};


/* Module definition for configuration. We list all callback routines 
* that we provide the static hooks into our module from the other parts 
* of the server. This list tells the server what function will handle a 
* particular type or stage of request. If we don't provide a function 
* to handle a particular stage / callback, use NULL as a placeholder as 
* illustrated below. */ 
module AP_MODULE_DECLARE_DATA blocker_module = 
{ 
  STANDARD20_MODULE_STUFF, 
  NULL, /* per-directory config creator */ 
  NULL, /* directory config merger */ 
  NULL, /* server config creator */ 
  NULL, /* server config merger */ 
  moduleCommands, /* command table */
  registerHooks, /* other request processing hooks */ 
}; 

