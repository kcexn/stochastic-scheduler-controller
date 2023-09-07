#!/bin/bash
function terminate(){
	for pid in $( ps --ppid 1 -o pid --no-headers ); do
		kill -s SIGTERM "$pid"
	done
}
trap terminate SIGTERM

# Start the controller
/usr/local/bin/controller &

# Start the web server.
/usr/sbin/nginx
wait
wait