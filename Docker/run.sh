#!/bin/bash
function terminate(){
	for pid in $( ps --ppid 1 -o pid --no-headers ); do
		kill -s SIGTERM "$pid"
	done
}
trap terminate SIGTERM
/usr/sbin/nginx
/usr/local/bin/controller &
wait
wait