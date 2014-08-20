#Bot Defender

This project is under development.

Bot Defender is a system that works with Apache 2.4.x to identify web scraping activity on a website so that is can be blocked in real-time.
The system is made up of two Apache modules, the first "mod_collector" is responsible for collecting all incoming HTTP requests and the second "mod_blocker" is responsible for blocking scraping activity.

The HTTP requests collected by mod_collector are sent to Java servers that are responsible for storing the requests in a mongoDB database as efficiently as possible.
There is another Java process responsible for analysing the data in the database to identify patterns in the traffic that look like scraping activity.
This analyzer process builds up a list of browser sessions that should be blocked. The block list is then sent back to Apache in real-time and used by mod_blocker to block further access by the suspicious session.

Bot Defender has been designed to work on high traffic websites and as such the analysis of activity data is performed outside of the request/response flow.
The impact of collecting traffic and blocking suspicious activity in Apache is an extremely optimized process that should be imperceptible (i.e. < 1 ms elapse).
In comparison the analysis of the data will take seconds to process however as this is outside of the request/response flow it won't adversely impact website visitors.



    +---------+                   +-----------------+
    |         |                   | Apache          |
    |         |                   +-----------------+
    |         |--http request---->| mod_collector   |--async---->[Java Collector]-------+
    |         |                   +-----------------+                                   |                      |
    | Web     |<--block response--| mod_blocker     |<--[Java Analyzer]---[MongoDB]<----+
    | Browser |                   +-----------------+
    |         |                   | Normal Apache   |
    |         |<--normal response-| request handler |
    +---------+                   +-----------------+



There are a number of scenarios that are used to block traffic, an example of the sort of blocking rules that exist are below. It's worth mentioning there
are circuit breakers around these scenarios that break if BotDefender isn't totally confident that the traffic is a bot.

##High Level Block Scenarios
* Block excessive activity for a given IP
* Block excessive activity by the session MD5
* Block excessive activity by URL fragment i.e. postcode or some other identifier
* Block excessive activity by IP with a high number of non standard HTTPD status codes
* Block excessive activity by session with a high number of non standard HTTPD status codes
* Block excessive activity by url fragment with a high number of non standard HTTPD status codes
* Block sessions that have high activity but the referrer is always empty
* Block sessions that replay the same session cookie over and over
* Block sessions that replay invalid MD5 cookies
* Block sessions that constantly forget to send valid session cookies
* Block known bad user agents

It is possible to build bespoke blocking rules over and above the basic rule set. These can be tailored to the specific website and will have access to the
data in the mongoDB store in order to determine the context of a session when determining if the traffic looks mechanical.


##Building BotDefender
Linux / OSX
First download and install the apache web server on the machine where you intend to build BotDefender.
Check that apxs is installed and in the path and that apachectl is installed and in the path (these should have been installed when apache was installed).

cd to the root directory and run
* mvn deploy

This will build the apache modules along with the java modules. At the moment the mod_collector.so and mod_blocker.so modules need to be manually
copied into the Apache installation. This is done by copying the files from src/main/c/*.so to