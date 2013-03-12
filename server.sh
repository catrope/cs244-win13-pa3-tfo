#! /bin/bash

PORT=12345

# Enable TFO
echo 3 | sudo tee /proc/sys/net/ipv4/tcp_fastopen

echo Building...
make

for experiment in wikipedia amazon
do
	echo Starting server for $experiment experiment on port $PORT
	( cd $experiment && ../tfomultiserver -p $PORT -f -r resp ) &
	PORT=$(($PORT + 1))
done

