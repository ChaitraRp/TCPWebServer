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
#define CLIENT_REQ_SIZE 10000
#define STATUSLINE 2000

char *ROOTDIR;
char PORT[10];
char wsConfFileBuffer[MAXBUFSIZE];
char *wsConfFilePath;
int clients[MAXCONN] = {0};
int listenfd;
int clientId;
int COUNT = 0;

//This function gives size of the file
long getFileSize(FILE *fp){
	fseek(fp, 0, SEEK_END);
    return(ftell(fp));
}

//Read the WS config file
void readWSConfig(){
	FILE *fp;
	wsConfFilePath = getenv("PWD");
	long fileSize=0;
	char* token;
	strncat(wsConfFilePath,"/ws.conf",8);
	printf("wsconf file path: %s\n", wsConfFilePath);
	
	if(fp = fopen(wsConfFilePath, "r")){
		fileSize = getFileSize(fp);
		fseek(fp, 0, SEEK_SET);
		
		while(fgets(wsConfFileBuffer, fileSize, fp)!=NULL){
			if(strncmp(wsConfFileBuffer,"Listen",6)==0){
				token = strtok(wsConfFileBuffer," \t\n");
				token = strtok(NULL, " \t\n");
				strcpy(PORT, token);
				printf("Port Number: %s\n",PORT);
				bzero(wsConfFileBuffer,sizeof(wsConfFileBuffer));
			}
			if(strncmp(wsConfFileBuffer,"DocumentRoot",12)==0){
				token = strtok(wsConfFileBuffer," \t\n");
				token = strtok(NULL, " \t\n");
				ROOTDIR=(char*)malloc(100);
				strcpy(ROOTDIR, token);
				printf("Root Directory: %s\n",ROOTDIR);
				bzero(wsConfFileBuffer,sizeof(wsConfFileBuffer));
			}
		}//end of while
	}
	else{
		perror("Error: ws.conf");
		exit(1);
	}
	fclose(fp);
}

void startServer(){
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

int checkFileFormat(char *ext){
	FILE *fp;
	bzero(wsConfFileBuffer,sizeof(wsConfFileBuffer));
	printf("ws.conf file path: %s\n", wsConfFilePath);
	return 0;
}


void response(int i){
	FILE *fp;
	int recvBytes;
	clientId = i;
	char clientRequest[CLIENT_REQ_SIZE];
	char path[100];
	char clientReqFile[50] = "clientReqFile";
	char countString[50];
	char connStatus[50];
	char statusLine[STATUSLINE];
	char *reqLine[3];
	char version[8];
	while(1){
		bzero(clientRequest, sizeof(clientRequest));
		memset((void*)clientRequest, (int)'\0', CLIENT_REQ_SIZE);
		bzero(connStatus, sizeof(connStatus));
		bzero(statusLine, sizeof(statusLine));
		bzero(reqLine, sizeof(reqLine));
		bzero(path, sizeof(path));
		recvBytes = recv(clients[i],clientRequest,CLIENT_REQ_SIZE,0);
		
		sprintf(countString,"%d",COUNT);
		strcat(clientReqFile, countString);
		FILE *fpClientReqFile = fopen(clientReqFile,"w");
		if(fpClientReqFile!=NULL){
			fputs(clientRequest, fpClientReqFile);
			fclose(fpClientReqFile);
		}
		
		if(!strstr(clientRequest,"Connection: Keep-alive"))
			strncpy(connStatus,"Connection: Keep-alive",22);
		else
			strncpy(connStatus,"Connection: Close",17);
			
		if(recvBytes < 0)
			perror("Error: recv() error");
		else{
			if(!strstr(clientRequest,"Connection: Keep-alive"))
				strncpy(connStatus,"Connection: Keep-alive",22);
			else
				strncpy(connStatus,"Connection: Close",17);
			printf("Request from Client: %s\n", clientRequest);
			
			reqLine[0] = strtok(clientRequest, " \t\n");
			if((strncmp(reqLine[0],"GET\0",4)==0)){
				reqLine[1] = strtok(NULL, " \t");
				reqLine[2] = strtok(NULL, " \t\n");
				
				if(strncmp(reqLine[2],"HTTP/1.1",8)==0)
					strcpy(version,"HTTP/1.1");
				else if(strncmp(reqLine[2],"HTTP/1.0",8)==0)
					strcpy(version,"HTTP/1.0");
				else if(strncmp(reqLine[2],"HTTP/1.1",8)!=0 && strncmp(reqLine[2],"HTTP/1.0",8)!=0){
					strncat(statusLine,version,strlen(version));
					strncat(statusLine," 400 Bad Request",16);
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Type:NONE",17);
					strncat(statusLine,"Content-Length:NONE",19);
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,connStatus,strlen(connStatus));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					strncat(statusLine,
							"<head><title>400 Bad Request</title></head><html><body>400 Bad Request: Invalid HTTP-Version</body></html>",strlen("<head><title>400 Bad Request</title></head><html><body>400 Bad Request: Invalid HTTP-Version</body></html>"));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					write(clients[i],statusLine,strlen(statusLine));
				}
				else{
					if(strncmp(reqLine[1],"/\0",2)==0)
						reqLine[1] = "/index.html";
					
					stpcpy(path, ROOTDIR);
					strcpy(&path[strlen(ROOTDIR)], reqLine[1]);
					printf("file: %s\n", path);
					
					int isFileFormat;
					char *ext = strchr(clientReqFile,'.');
					if(ext == NULL)
						isFileFormat = 0;
					else
						isFileFormat = checkFileFormat(ext);
				}
				
			}
		}
			
		
	}//end of while
}

int main(){
	struct sockaddr_in clientServer;
	int clientServerSize;
	int port;
	int i;
	readWSConfig();
	
	port = atoi(PORT);
	//printf("Port Number: %d\n", port);
	
	startServer();
	
	while(1){
		clientServerSize = sizeof(clientServer);
		clients[i] = accept(listenfd, (struct sockaddr*)&clientServer, &clientServerSize);
		if(clients[i]<0)
			perror("Error: accept()");
		else if(fork()==0)
			response(i);
		
		while(i<=MAXCONN)
			i = (i+1)%MAXCONN;
	}//end of while
	return 0;
}