APXS=apxs
APACHECTL=apachectl
APXS_OPTS=-Wc,-Wstrict-prototypes -Wc,-Werror -Wc,-Wall -g

all: common mod_collector mod_blocker 

common: 
	$(APXS) -ca mod_blocker.c common.c -g -Wall -O

mod_blocker:
	$(APXS) -ca mod_blocker.c common.c -g -Wall
	cp .libs/mod_blocker.so .

mod_collector: 
	$(APXS) -ca mod_collector.c mod_collector_comms.c common.c -g -Wall
	cp .libs/mod_collector.so .

install: all
	$(APXS) -n 'collector' -i -a .libs/mod_collector.so
	$(APXS) -n 'blocker' -i -a .libs/mod_blocker.so

stop_apache:
	- killall -9 httpd
	sleep 1

start_apache:
	$(APACHECTL) start

clean:
	rm -rf *.o *.bak *.loT *.lo *.slo .libs *.la
