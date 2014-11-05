#Bot Defender

This project is under development.

Bot Defender is a system that works with Apache 2.4.x to identify web scraping activity on a website so that is can be blocked in real-time.
The system is made up of two Apache modules and a series of Java processes.

The first Apache module "mod_collector" is responsible for collecting all incoming HTTP requests and the second "mod_blocker" is responsible for blocking scraping activity.
The HTTP requests collected by mod_collector are sent to Java servers that are responsible for storing the requests in a mongoDB database as efficiently as possible.
There is another Java process responsible for analysing the data in the database to identify patterns in the traffic that look like scraping activity.
This analyzer process builds up a list of browser sessions that should be blocked. The block list is sent back to Apache in real-time and used by mod_blocker to block further access by the suspicious session.

Bot Defender has been designed to work on high traffic websites and as such the analysis of activity data is performed outside of the request/response flow.
The impact of collecting traffic and blocking suspicious activity in Apache is an extremely optimized process that should be imperceptible (i.e. < 1 ms elapse).
In comparison the analysis of the data will take seconds to process however as this is outside of the request/response flow it won't adversely impact website visitors.



##High Level Flow
    +---------+                   +-----------------+
    |         |                   | Apache          |
    |         |                   +-----------------+
    |         |--http request---->| mod_collector   |--async---->[Java Collector]-------+
    |         |                   +-----------------+                                   |
    | Web     |<--block response--| mod_blocker     |<--[Java Analyzer]---[MongoDB]<----+
    | Browser |                   +-----------------+
    |         |                   | Normal Apache   |
    |         |<--normal response-| request handler |
    +---------+                   +-----------------+



There are a number of scenarios that are used to block traffic, an example of the sort of blocking rules that exist are below. There is additional logic
around these scenarios that stops the rule being actioned if Bot Defender isn't totally confident that the traffic is a bot.

##High Level Block Scenarios
* Block excessive activity for a given IP
* Block excessive activity by the Bot Defender session cookie
* Block excessive activity by URL fragment i.e. postcode or some other identifier
* Block excessive activity by IP with a high number of non standard HTTP status codes
* Block excessive activity by session with a high number of non standard HTTP status codes
* Block excessive activity by url fragment with a high number of non standard HTTP status codes
* Block sessions that have high activity but the referrer is always empty
* Block sessions that replay the same session cookie over and over
* Block sessions that replay invalid session cookies
* Block sessions that constantly forget to send valid session cookies
* Block known bad user agents

It is possible to build bespoke blocking rules over and above the basic rule set. These can be tailored to the specific website and will have access to the
data in the mongoDB store in order to determine the context of a session when determining if the traffic looks mechanical.


##Building Apache
-----------------
In order for botdefender to work your httpd apache server must be compiled so that it can load modules dynamically.
The following steps worked for me on Ubuntu.

Create a directory called ~/httpd
* cd ~/httpd

Download the Apache Portable Runtime from the following website (https://apr.apache.org/download.cgi)
Unpack the APR source, for example if you downloaded the gzipped 1.5.1 version you can unpack the file using the following command
* tar -xzvf apr-1.5.1.tar.gz

Download the Apache Portable Runtime Util from the following website (https://apr.apache.org/download.cgi)
Unpack the APR Util source, for example if you downloaded the gzipped 1.5.3 version you can unpack the file using the following command
* tar -xzvf apr-util-1.5.3.tar.gz

Download PCRE from the following website (http://sourceforge.net/projects/pcre/files/pcre/8.35/)
Unpack the source, for example if you downloaded the gzipped 8.35 version you can unpack the file using the following command
* tar -xzvf pcre-8.35.tar.gz
* cd pcre-8.35
* ./configure --disable-cpp
* make
* sudo make install
* cd ..


Download the Apache 2.4.x source from the apache website (http://httpd.apache.org/download.cgi#apache24)
Unpack the apache source, for example if you downloaded the gzipped 2.4.10 version you can unpack the file using the following command

* tar -xzvf httpd-2.4.10.tar.gz
* cd httpd-2.4.10
* mv ~/httpd/apr-1.5.1 srclib/apr
* mv ~/httpd/apr-util-1.5.3 srclib/apr-util
* mv ~/httpd/pcre-8.35 srclib/pcre
* ./configure --prefix=/usr/local/apache2 --enable-mods-shared=all --enable-so --with-included-apr
* make
* sudo make install


##Building botdefender
Checkout the botdefender projects from GitHub

Add the apache bin directory to the PATH so that the apxs compiler is in the path
* export PATH=/usr/local/apache2/bin:$PATH

cd to the root of the botdefender project
* cd botdefender
* mvn compile

This will build the apache modules along with the java modules. At the moment the mod_collector.so and mod_blocker.so modules need to be manually
copied into the Apache installation. This is done by copying the files from src/main/c/*.so to the ${apache_home}/modules directory.


##Configuring Apache
Add the following to the conf/httpd.conf file

LoadModule collector_module   modules/mod_collector.so
LoadModule blocker_module     modules/mod_blocker.so

BlockCommandPath        /_admin
CollectorServerPort     9999
CollectorServerHost     localhost



---------
mod_request.c uses this approach

KEEP_BODY flag keeps data, should then be able to use ap_parse_form_data

    keep_body_input_filter_handle =
        ap_register_input_filter(KEEP_BODY_FILTER, keep_body_filter,
                                 NULL, AP_FTYPE_RESOURCE);
    kept_body_input_filter_handle =
        ap_register_input_filter(KEPT_BODY_FILTER, kept_body_filter,
                                 kept_body_filter_init, AP_FTYPE_RESOURCE);

 CONTEXT - holding request context
 ﻿    log_request_state *state = (log_request_state *)ap_get_module_config(r->request_config,
                                                                          &log_config_module);
     if (!state) {
         state = apr_pcalloc(r->pool, sizeof(log_request_state));
         ap_set_module_config(r->request_config, &log_config_module, state);
     }
     if (state->request



     Need to implement the
kept_body_filter
