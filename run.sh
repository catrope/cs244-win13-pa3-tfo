#! /bin/bash

# CONFIGURE ME
SERVER=ec2-54-241-232-199.us-west-1.compute.amazonaws.com
SERVERIP=$(host $SERVER | cut -d ' ' -f 4)
PORT=12345

# Enable TFO
echo 3 | sudo tee /proc/sys/net/ipv4/tcp_fastopen

# Determine RTT to the server
echo Measuring RTT to the server...
ping -c 10 $SERVERIP

echo Building...
make

# Load Wikipedia 10 times without TFO
cd wikipedia
SUM=0
for i in {1..10}
do
	echo -n "Wikipedia without TFO run $i: "
	T=$(../tfomulticlient -s $SERVERIP -p $PORT -r reqs)
	echo $T us
	SUM=$(($SUM + $T))
done
echo Total: $SUM us
echo

# Load Wikipedia once with TFO so we have a cookie for sure
../tfomulticlient -s $SERVERIP -p $PORT -r reqs -f

# Load Wikipedia 10 times with TFO
SUM=0
for i in {1..10}
do
	echo -n "Wikipedia with TFO run $i: "
	T=$(../tfomulticlient -s $SERVERIP -p $PORT -r reqs -f)
	echo $T us
	SUM=$(($SUM + $T))
done
echo Total: $SUM us

