#BotDefender
BotDefender is a system that works with Apache to identify bad robots and block the activity in real time.
The system is made up of two Apache modules responsible for collecting traffic information in real time and blocking robots.
The analysis of the traffic data is performed within a java based server that is responsible for identifying pattens in activity and informing Apache about what should be blocked.

High Level Block Scenarios
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

It is possible to build bespoke blocking rules over and above the basic rule set.

#Building BotDefender
##Linux
First download and install the apache web server on the machine your going to use to build BotDefender.
In order to build 