#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

int main() {
	int s, c;
	struct sockaddr_in sa, ca;
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;
	sa.sin_port = 12345;
	sa.sin_addr.s_addr = INADDR_ANY;
	s = socket(AF_INET, SOCK_STREAM, 0);
	bind(s, (struct sockaddr *)&sa, sizeof(sa));
	listen(s, 50);
	c = accept(s, (struct sockaddr*)ca, sizeof(struct sockaddr_in));
	return 1;
}	
	
