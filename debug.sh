killall -9 httpd
rm /usr/local/apache2/logs/*

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

cd src/main/c/
make clean
make
cp *.so /usr/local/apache2/modules/.
cd ../../..

/usr/local/apache2/bin/apachectl stop

gdb -x debug.script /usr/local/apache2/bin/httpd
