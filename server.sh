#! /bin/bash

PORT=12345

# Enable TFO
echo 3 | sudo tee /proc/sys/net/ipv4/tcp_fastopen

echo Building...
make

echo Starting server for Wikipedia experiment on port $PORT
cd wikipedia
../tfomultiserver -p 12345 -f -r resp

