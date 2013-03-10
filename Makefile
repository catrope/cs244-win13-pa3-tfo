ALL: tfoclient tfoserver tfomulticlient tfomultiserver

clean:
	rm -f tfoclient tfoserver

tfoclient: tfoclient.c
	gcc -g -Wall tfoclient.c -o tfoclient

tfoserver: tfoserver.c
	gcc -g -Wall tfoserver.c -o tfoserver

tfomulticlient: tfomulticlient.c
	gcc -g -Wall tfomulticlient.c -o tfomulticlient

tfomultiserver: tfomultiserver.c
	gcc -g -Wall tfomultiserver.c -o tfomultiserver
