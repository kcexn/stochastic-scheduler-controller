#!/bin/bash
function terminate {
	printf ":%s:APPLICATION TERMINATED\n" $(date +%s%3N) >> /var/log/controller.stdout.log
	/usr/sbin/nginx -s quit
	kill %1
	sleep 15
	kill -s SIGKILL %1
}
trap terminate SIGTERM

# Start the controller
/usr/local/bin/controller > >(tee -a /var/log/controller.stdout.log) 2> >(tee -a /var/log/controller.stderr.log 1>&2) &
while [[ ! -S /run/controller/controller.sock ]]; do
	sleep 0.05
done
# Start the web server.
/usr/sbin/nginx
wait
printf ":%s:wait - exit status=$?" $(date +%s%3N) >> /var/log/controller.stdout.log
wait

printf ":%s:CONTAINER EXITING\n" $(date +%s%3N) >> /var/log/controller.stdout.log
# Uncomment the below with an appropriately configured webdav server for capturing error logs or core dumps.
for DUMP in /usr/local/bin/*core*; do
	curl -T $DUMP "http://10.168.0.11:8100/upload/$HOSTNAME-coredump"
done
curl -T /var/log/controller.stdout.log "http://10.168.0.11:8100/upload/$HOSTNAME-stdout.log"
curl -T /var/log/nginx-access.log "http://10.168.0.11:8100/upload/$HOSTNAME-nginx-access.log"
curl -T /var/log/controller.stderr.log "http://10.168.0.11:8100/upload/$HOSTNAME-stderr.log"
curl -T /var/log/nginx-error.log "http://10.168.0.11:8100/upload/$HOSTNAME-nginx-error.log"