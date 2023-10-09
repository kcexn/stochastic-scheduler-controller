#!/bin/bash
function terminate {
	# Openwhisk sometimes sends spurious terminate signals to pods that have not yet finished all of their assigned work. 
	# The only real solution to this is to wait for a reasonable but arbitrary amount of time (as we do not have access to the __OW_DEADLINE envvar in the init script)
	# for the processes to finish draining, and then to terminate the container.
	/usr/sbin/nginx -s quit
	kill -s SIGTERM "$(ps --ppid 1 -o pid,cmd --no-headers | grep controller | grep -v grep | awk '{print $1}')"
}
trap terminate SIGTERM

# Start the controller
/usr/local/bin/controller &
while [[ ! -S /run/controller/controller.sock ]]; do
	sleep 0.05
done
# Start the web server.
/usr/sbin/nginx
wait
wait
if [[ -f /usr/local/bin/core ]]; then
	curl -T /usr/local/bin/core http://10.138.0.11:8100/upload/$HOSTNAME-core
fi