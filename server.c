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

//------------------------------GLOBAL VARIABLES---------------------------
char *ROOTDIR;
char PORT[10];
char wsConfFileBuffer[MAXBUFSIZE];
char *wsConfFilePath;
int clients[MAXCONN];
int listenfd;
int clientId;
int COUNT = 0;
long wsConfFileSize=0;


//-----------------------------GET FILESIZE--------------------------------
long getFileSize(FILE *fp){
	fseek(fp, 0, SEEK_END);
    return(ftell(fp));
}


//------------------------------READ WSCONFIG------------------------------
void readWSConfigFile(){
	FILE *fp;
	wsConfFilePath = getenv("PWD");
	char* token;
	strncat(wsConfFilePath,"/ws.conf",8);
	printf("wsconf file path: %s\n", wsConfFilePath);
	
	if(fp = fopen(wsConfFilePath, "r")){
		wsConfFileSize = getFileSize(fp);
		fseek(fp, 0, SEEK_SET);
		
		while(fgets(wsConfFileBuffer, wsConfFileSize, fp)!=NULL){
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


//----------------------------START WEB SERVER------------------------------
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
		perror("Error: socket() or bind()");
		exit(1);
	}
	
	freeaddrinfo(result);
	
	if(listen(listenfd, MAXCONN)!=0){
		perror("Error: listen()");
		exit(1);
	}
}


//-------------------------CHECK FILE FORMAT---------------------------------
int checkFileFormat(char *ext){
	FILE *fp;
	char formats[20][100];
	char *wsConfFilePath1 = getenv("PWD");
	char wsBuffer[MAXBUFSIZE];
	int i;
	int fileSupported=0;
	int wsSize=0;
	
	printf("**************************************");
	
	if(wsConfFilePath1 != NULL)
		printf("ws.conf path: %s\n", wsConfFilePath1);
	
	fp = fopen(wsConfFilePath1, "r");
	wsSize = getFileSize(fp);
	fseek(fp, 0, SEEK_SET);
	
	while(fgets(wsBuffer, wsSize, fp)!=NULL){
		strcpy(formats[i],wsBuffer);
		i++;
	}
	
	int j=0;
	for(j=0;j<i+1;j++){
		if(strncmp(formats[j],ext,3)==0){
			fileSupported = 1;
			break;
		}
	}
	
	fclose(fp);
	printf("File supported: %d\n", fileSupported);
	return fileSupported;
}


//---------------------------GET CONTENT TYPE-----------------------------
char* getContentType(char *ext){
	char *extn;
	if((strcmp(ext,".html"))==0 || (strcmp(ext,".htm"))==0)
		extn = "text/html";
	else if((strcmp(ext,".txt"))==0)
		extn = "text/plain";
	else if((strcmp(ext,".png"))==0)
		extn = "image/png";
	else if((strcmp(ext,".gif"))==0)
		extn = "image/gif";
	else if((strcmp(ext,".jpg"))==0)
		extn = "image/jpg";
	else if((strcmp(ext,".css"))==0)
		extn = "text/css";
	else if((strcmp(ext,".js"))==0)
		extn = "text/javascript";
	else if((strcmp(ext,".ico"))==0)
		extn = "image/x-icon";
	else
		extn = "application/unknown";
	
	printf("Type: %s\n", extn);
	return extn;
}


//----------------------------GENERATE HTTP RESPONSE------------------------
void httpResponse(int i){
	clientId = i; //global value
	FILE *fp;
	int recvBytes;
	int readBytes;
	int fd;
	char clientRequest[CLIENT_REQ_SIZE];
	char path[100];
	//char clientReqFile[50] = "clientReqFile";
	char countString[50];
	char connStatus[50];
	char statusLine[STATUSLINE];
	char *reqLine[3];
	char version[8];
	char contentLength[20];
	char sendData[MAXBUFSIZE];
	while(1){
		bzero(clientRequest, sizeof(clientRequest));
		bzero(connStatus,sizeof(connStatus));
		bzero(statusLine,sizeof(statusLine));
		bzero(reqLine,sizeof(reqLine));
		bzero(path,sizeof(path));
		bzero(sendData,sizeof(sendData));
		memset((void*)clientRequest, (int)'\0', CLIENT_REQ_SIZE);
		recvBytes = recv(clients[i],clientRequest,CLIENT_REQ_SIZE,0);
		
		/*sprintf(countString,"%d",COUNT);
		strcat(clientReqFile, countString);
		FILE *fpClientReqFile = fopen(clientReqFile,"w");
		
		if(fpClientReqFile!=NULL){
			fputs(clientRequest, fpClientReqFile);
			fclose(fpClientReqFile);
		}*/
		
		if(!strstr(clientRequest,"Connection: Keep-alive"))
			strncpy(connStatus,"Connection: Keep-alive",strlen("Connection: Keep-alive"));
		else
			strncpy(connStatus,"Connection: Close",strlen("Connection: Close"));
		
		bzero(statusLine,sizeof(statusLine));
		if(recvBytes < 0)
			perror("Error: recv() error");
		else if(recvBytes == 0)
			readBytes = 0;
		else{
			printf("Client #%d says: %s\n", i,clientRequest);
			
			reqLine[0] = strtok(clientRequest, " \t\n");
			
			if((strncmp(reqLine[0],"GET\0",4)==0)){
				reqLine[1] = strtok(NULL, " \t");
				reqLine[2] = strtok(NULL, " \t\n");
				
				if(strncmp(reqLine[2],"HTTP/1.1",strlen("HTTP/1.1"))==0)
					strcpy(version,"HTTP/1.1");
				else
					strcpy(version,"HTTP/1.0");
				
				if(strncmp(reqLine[2],"HTTP/1.1",strlen("HTTP/1.1"))!=0 && strncmp(reqLine[2],"HTTP/1.0",strlen("HTTP/1.0"))!=0){
					strncat(statusLine,version,strlen(version));
					strncat(statusLine," 400 Bad Request",strlen(" 400 Bad Request"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Type:NONE",strlen("Content-Type:NONE"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Length:NONE",strlen("Content-Length:NONE"));
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
					printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
					printf("file: %s\n", path);
					
					int isFileFormat;
					char *ext = strrchr(path,'.');
					printf("Extension: %s\n",ext);
					if(ext == NULL)
						isFileFormat = 0;
					else
						isFileFormat = checkFileFormat(ext);
					
					if(isFileFormat == 1){
						if((fd=open(path,O_RDONLY))!=-1){
							fp = fopen(path,"r");
							char *contentType = getContentType(ext);
							int fileSize = getFileSize(fp);
							sprintf(contentLength,"%d",fileSize);
						
							strncat(statusLine,version,strlen(version));
							strncat(statusLine," 200 OK",strlen(" 200 OK"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Type:",strlen("Content-Type:"));
							strncat(statusLine,contentType,strlen(contentType));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
							strncat(statusLine,contentLength,strlen(contentLength));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,connStatus,strlen(connStatus));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							//printf("Status Line: %s\n\n",statusLine);
							send(clients[i],statusLine,strlen(statusLine),0);
							
							while((readBytes = read(fd,sendData,MAXBUFSIZE))>0)
								write(clients[i],sendData,readBytes);
							
							fclose(fp);
						}
						//else if(fd == -1) file not found
						else{
							strncat(statusLine,version,strlen(version));
							strncat(statusLine," 404 Not Found",strlen(" 404 Not Found"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Type:Invalid",strlen("Content-Type:Invalid"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Length:Invalid",strlen("Content-Length:Invalid"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,connStatus,strlen(connStatus));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							strncat(statusLine,
									"<head><title>404 Not Found</title></head><html><body>404 Not Found: URL does not exist: ",strlen("<head><title>404 Not Found</title></head><html><body>404 Not Found: URL does not exist: "));
							strncat(statusLine,path,strlen(path));
							strncat(statusLine,"</body></html>",strlen("</body></html>"));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							//printf("Status Line: %s\n\n",statusLine);
							write(clients[i],statusLine,strlen(statusLine));
						}
					}//end of if isFileFormat == 1
					
					else{ //if fileFormat is not supported then send 501
						strncat(statusLine,version,strlen(version));
						strncat(statusLine," 501 Not Implemented",strlen(" 501 Not Implemented"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,"Content-Type:None",strlen("Content-Type:None"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,"Content-Length:None",strlen("Content-Length:None"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,connStatus,strlen(connStatus));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						strncat(statusLine,
								"<head><title>501 Not Implemented</title></head><html><body>501 Not Implemented: Unsupported file format</body></html>",strlen("<head><title>501 Not Implemented</title></head><html><body>501 Not Implemented: Unsupported file format</body></html>"));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						//printf("Status Line: %s\n\n",statusLine);
						write(clients[i],statusLine,strlen(statusLine));
					}//end of 501 error
				}//end of 200 OK
			}//end of GET check
			
			//Methods other than GET
			else{
				strncat(statusLine,version,strlen(version));
				strncat(statusLine," 501 Not Implemented",strlen(" 501 Not Implemented"));
				strncat(statusLine,"\n",strlen("\n"));
				strncat(statusLine,"Content-Type:None",strlen("Content-Type:None"));
				strncat(statusLine,"\n",strlen("\n"));
				strncat(statusLine,"Content-Length:None",strlen("Content-Length:None"));
				strncat(statusLine,"\n",strlen("\n"));
				strncat(statusLine,connStatus,strlen(connStatus));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				strncat(statusLine,
						"<head><title>501 Not Implemented</title></head><html><body>501 Not Implemented: Unsupported method</body></html>",strlen("<head><title>501 Not Implemented</title></head><html><body>501 Not Implemented: Unsupported method</body></html>"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				//printf("Status Line: %s\n\n",statusLine);
				write(clients[i],statusLine,strlen(statusLine));
			}//end of non GET/POST methods	
		}//end of else recvBytes>0
	}//end of while
	
	//close the socket here
	shutdown(clients[i],SHUT_RDWR);
	close(clients[i]);
	clients[i]=-1;
}


//-----------------------------------MAIN----------------------------------
int main(){
	struct sockaddr_in clientServer;
	int clientServerSize;
	int i;
	
	for(i=0;i<MAXCONN;i++)
		clients[i]=-1;
	
	readWSConfigFile();	
	startServer();
	
	i=0;
	while(1){
		clientServerSize = sizeof(clientServer);
		clients[i] = accept(listenfd, (struct sockaddr*)&clientServer, &clientServerSize);
		if(clients[i]<0)
			perror("Error: accept()");
		else{
			if(fork()==0)
				httpResponse(i);
		}
		while(clients[i]!=-1)
			i = (i+1)%MAXCONN;
	}//end of while
	return 0;
}