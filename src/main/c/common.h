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
*/

#define MAX_SIZE                 1048576

typedef struct {
    char* name;
    char* value;
} Cookie;



#define SESSION_COOKIE           ":sc:"


void criticalSectionStart(request_rec *r, apr_thread_mutex_t *context);
void criticalSectionEnd(request_rec *r, apr_thread_mutex_t *context);
void setCookie(request_rec *r, char* cookieName, char* cookieValue);
Cookie *getCookie(request_rec *r, const char *name);
char* safeJSONString(request_rec* r, const char* text);
char *getRequestHeaderValue(request_rec *r, char *key, char *nullval);
char *getResponseHeaderValue(request_rec *r, char *key, char *nullval);
char* getClientIP(request_rec *r);
char* getUserAgentIP(request_rec *r);
char* tableToJSON(request_rec *r, const apr_table_t *tab, char* name);
char *calc_secure_MD5(request_rec *r, char *secret, char *payload);

