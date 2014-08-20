typedef struct {
    char* name;
    char* value;
} Cookie;


void criticalSectionStart(request_rec *r, apr_thread_mutex_t *context);
void criticalSectionEnd(request_rec *r, apr_thread_mutex_t *context);
void setCookie(request_rec *r, char* cookieName, char* cookieValue);
Cookie *getCookie(request_rec *r, const char *name);
char* safeJSONString(request_rec* r, const char* text);
char *getRequestHeaderValue(request_rec *r, char *key, char *nullval);
char *getResponseHeaderValue(request_rec *r, char *key, char *nullval);
char* getClientIP(request_rec *r);
char* getUserAgentIP(request_rec *r);