#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#define MAXBUFSIZE 1024
#define MAXCONN 100

char *ROOTDIR;
char PORT[10];
int clients[MAXCONN] = {0};
int listenfd;

//This function gives size of the file
long getFileSize(FILE *fp){
	fseek(fp, 0, SEEK_END);
    return(ftell(fp));
}

//Read the WS config file
void readWSConfig(){
	FILE *fp;
	char fileBuffer[MAXBUFSIZE];
	char *filePath = getenv("PWD");
	long fileSize=0;
	char* token;
	strncat(filePath,"/ws.conf",8);
	printf("wsconf file path: %s\n", filePath);
	
	if(fp = fopen(filePath, "r")){
		fileSize = getFileSize(fp);
		fseek(fp, 0, SEEK_SET);
		
		while(fgets(fileBuffer, fileSize, fp)!=NULL){
			if(strncmp(fileBuffer,"Listen",6)==0){
				token = strtok(fileBuffer," \t\n");
				token = strtok(NULL, " \t\n");
				strcpy(PORT, token);
				printf("Port Number: %s\n",PORT);
				bzero(fileBuffer,sizeof(fileBuffer));
			}
			if(strncmp(fileBuffer,"DocumentRoot",12)==0){
				token = strtok(fileBuffer," \t\n");
				token = strtok(NULL, " \t\n");
				ROOTDIR=(char*)malloc(100);
				strcpy(ROOTDIR, token);
				printf("Root Directory: %s\n",ROOTDIR);
				bzero(fileBuffer,sizeof(fileBuffer));
			}
		}//end of while
	}
	else{
		perror("Error: ws.conf");
		exit(1);
	}
	fclose(fp);
}

void startServer(int port){
	struct addrinfo *result = NULL;
    struct addrinfo *ptr = NULL;
    struct addrinfo hints;
	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	//Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/ms737530(v=vs.85).aspx
	if(getaddrinfo(NULL, PORT, &hints, &result)!=0){
		perror("Error: getaddrinfo()");
		exit(1);
	}
	
	for(ptr=result; ptr!=NULL; ptr=ptr->ai_next){
		if((listenfd=socket(ptr->ai_family, ptr->ai_socktype, 0))==-1)
			continue;
		if(bind(listenfd, ptr->ai_addr, ptr->ai_addrlen)==0)
			break;
	}
	
	if(ptr==NULL){
		perror("Error: bind()");
		exit(1);
	}
	
	freeaddrinfo(result);
	
	if(listen(listenfd, MAXCONN)!=0){
		perror("Error: listen()");
		exit(1);
	}
}


int main(){
	struct sockaddr_in clientServer;
	int clientServerSize;
	int port;
	readWSConfig();
	
	port = atoi(PORT);
	//printf("Port Number: %d\n", port);
	
	startServer(port);
	
}