#!/bin/bash
function terminate {
	/usr/sbin/nginx -s quit
	kill %1
}
trap terminate SIGTERM

# Start the controller
nice -n -5 /usr/local/bin/controller > >(tee -a /var/log/controller.stdout.log) 2> >(tee -a /var/log/controller.stderr.log) &
while [[ ! -S /run/controller/controller.sock ]]; do
	sleep 0.05
done
# Start the web server.
/usr/sbin/nginx
wait
wait

# Uncomment the below with an appropriately configured webdav server for capturing error logs or core dumps.
# if [[ -f /usr/local/bin/core ]]; then
# 	curl -T /usr/local/bin/core "http://10.168.0.11:8100/upload/$HOSTNAME-core"
# fi
# curl -T /var/log/controller.stdout.log "http://10.168.0.11:8100/upload/$HOSTNAME-stdout.log"
# curl -T /var/log/controller.stderr.log "http://10.168.0.11:8100/upload/$HOSTNAME-stderr.log"
