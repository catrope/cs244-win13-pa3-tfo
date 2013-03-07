ALL: tfoclient tfoserver

clean:
	rm -f tfoclient tfoserver

tfoclient: tfoclient.c
	gcc -g -Wall tfoclient.c -o tfoclient

tfoserver: tfoserver.c
	gcc -g -Wall tfoserver.c -o tfoserver
