#! /bin/bash

# CONFIGURE ME
SERVER=ec2-54-241-232-199.us-west-1.compute.amazonaws.com
SERVERIP=$(host $SERVER | cut -d ' ' -f 4)
PORT=12345
SAMPLES=20

# Enable TFO
echo 3 | sudo tee /proc/sys/net/ipv4/tcp_fastopen

# Determine RTT to the server
echo Measuring RTT to the server...
ping -c 10 $SERVERIP

echo Building...
make

for experiment in wikipedia amazon
do
	# Load 10 times without TFO
	cd $experiment
	SUM=0
	for (( i=1; $i - 1 - $SAMPLES; i=$(($i + 1)) ))
	do
		echo -n "$experiment without TFO run $i: "
		T=$(../tfomulticlient -s $SERVERIP -p $PORT -r reqs)
		echo $T us
		SUM=$(($SUM + $T))
	done
	echo Average: $(($SUM / $SAMPLES)) us
	echo

	# Load once with TFO so we have a cookie for sure
	../tfomulticlient -s $SERVERIP -p $PORT -r reqs -f

	# Load 10 times with TFO
	SUM=0
	for (( i=1; $i - 1 - $SAMPLES; i=$(($i + 1)) ))
	do
		echo -n "$experiment with TFO run $i: "
		T=$(../tfomulticlient -s $SERVERIP -p $PORT -r reqs -f)
		echo $T us
		SUM=$(($SUM + $T))
	done
	echo Average: $(($SUM / $SAMPLES)) us
	PORT=$(($PORT + 1))
done


