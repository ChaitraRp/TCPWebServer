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
#define CLIENT_REQ_SIZE 99999
#define POSTMSGSIZE 50000
#define STATUSLINE 2000
#define TIMEOUT 30

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
struct itimerval timeout;

//-----------------------------GET FILESIZE--------------------------------
long getFileSize(FILE *fp){
	fseek(fp, 0, SEEK_END);
    return(ftell(fp));
}


//------------------------------READ WSCONFIG------------------------------
void readWSConfigFile(){
	FILE *fp;
	wsConfFilePath = getenv("PWD");
	char *token;
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
	struct addrinfo *result;
    struct addrinfo *ptr;
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
		extn = "image/jpeg";
	else if((strcmp(ext,".css"))==0)
		extn = "text/css";
	else if((strcmp(ext,".js"))==0)
		extn = "text/javascript";
	else if((strcmp(ext,".ico"))==0)
		extn = "image/x-icon";
	else
		extn = "application/octet-stream";
	
	printf("Type: %s\n", extn);
	return extn;
}


void alarmHandler(void)
{  
	printf("Timeout\n");
	printf("Closing client #%d\n", clientId);
	shutdown (clients[clientId], SHUT_RDWR);
	close(clients[clientId]);
	clients[clientId]=-1;
   	exit(0);
}

/*
//----------------------------GENERATE HTTP RESPONSE------------------------
void httpResponse(int i){
	clientId = i; //global value
	FILE *fp;
	int recvBytes;
	int readBytes;
	int fd;
	int checkPost=0;
	char clientRequest[CLIENT_REQ_SIZE];
	char path[CLIENT_REQ_SIZE];
	char postFile[50] = "postFile";
	char countString[50];
	char connStatus[50];
	char statusLine[STATUSLINE];
	char *reqLine[3];
	char version[8];
	char contentLength[20];
	char sendData[MAXBUFSIZE];
	char postData[POSTMSGSIZE];
	
	signal(SIGALRM, (void(*) (int)) alarmHandler);
	timeout.it_value.tv_sec = TIMEOUT;
	timeout.it_value.tv_usec = 0;
	timeout.it_interval = timeout.it_value;
	
	while(1){
		bzero(clientRequest, sizeof(clientRequest));
		bzero(connStatus,sizeof(connStatus));
		bzero(statusLine,sizeof(statusLine));
		bzero(reqLine,sizeof(reqLine));
		bzero(path,sizeof(path));
		bzero(sendData,sizeof(sendData));
		//bzero(countString,sizeof(countString));
		checkPost=0;
		memset((void*)clientRequest, (int)'\0', CLIENT_REQ_SIZE);
		recvBytes = recv(clients[i],clientRequest,CLIENT_REQ_SIZE,0);
		
		if(!strstr(clientRequest,"Connection: Keep-alive"))
			strncpy(connStatus,"Connection: Keep-alive",strlen("Connection: Keep-alive"));
		else
			strncpy(connStatus,"Connection: Close",strlen("Connection: Close"));
		
		bzero(statusLine,sizeof(statusLine));
		if(recvBytes < 0)
			perror("Error: recv() error");
		else if(recvBytes == 0)
			recvBytes = 0;
		else{
			timeout.it_value.tv_sec = TIMEOUT;
			timeout.it_value.tv_usec = 0;
			timeout.it_interval = timeout.it_value;
			
			if(!strstr(clientRequest,"Connection: Keep-alive"))
				strncpy(connStatus,"Connection: Keep-alive",strlen("Connection: Keep-alive"));
			else
				strncpy(connStatus,"Connection: Close",strlen("Connection: Close"));
			
			printf("Client #%d says: %s\n", i,clientRequest);
			
			reqLine[0] = strtok(clientRequest, " \t\n");
			
			if((strncmp(reqLine[0],"GET\0",4)==0) || (strncmp(reqLine[0],"POST\0",5)==0)){
				if(strncmp(reqLine[0],"POST\0",5)==0){
					printf("********POST*********\n");
					checkPost = 1;
				}
				
				reqLine[1] = strtok(NULL, " \t");
				reqLine[2] = strtok(NULL, " \t\n");
					
				if(strncmp(reqLine[2],"HTTP/1.1",8)==0)
					strcpy(version,"HTTP/1.1");
				else
					strcpy(version,"HTTP/1.0");
				
				if(strncmp(reqLine[2],"HTTP/1.1",8)!=0 && strncmp(reqLine[2],"HTTP/1.0",8)!=0){
					strncat(statusLine,version,strlen(version));
					strncat(statusLine," 400 Bad Request",strlen(" 400 Bad Request"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Type:",strlen("Content-Type:"));
					strncat(statusLine,"NONE",strlen("NONE"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
					strncat(statusLine,"NONE",strlen("NONE"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,connStatus,strlen(connStatus));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					strncat(statusLine,"<head><title>400 Bad Request</title></head>",strlen("<head><title>400 Bad Request</title></head>"));
					strncat(statusLine,"<html><body>400 Bad Request: Invalid HTTP-Version</body></html>",strlen("<html><body>400 Bad Request: Invalid HTTP-Version</body></html>"));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					printf("%s\n",statusLine);
					write(clients[i],statusLine,strlen(statusLine));
				}
				else{
					if(strncmp(reqLine[1],"/\0",2)==0)
						reqLine[1] = "/index.html";
					
					stpcpy(path, ROOTDIR);
					strcpy(&path[strlen(ROOTDIR)], reqLine[1]);
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
						
							//POST
							if(checkPost == 1){
								printf("Inside POST IF condition\n");
								sprintf(countString,"%d",COUNT);
								strcat(postFile, countString);
								FILE *fpPostFile = fopen(postFile,"w");
								
								if(fpPostFile!=NULL){
									fputs(clientRequest, fpPostFile);
									fclose(fpPostFile);
								}
								COUNT++;
								
								strncat(statusLine,"POST ",strlen("POST "));
							}
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
							if(checkPost == 1){
								sprintf(postData,"<h1>Post Data</h1><html><body>%s</body></html>\n","Have a nice day!");
								strncat(statusLine,postData,strlen(postData));
								strncat(statusLine,"\r\n",strlen("\r\n"));
							}
							printf("%s\n\n",statusLine);
							send(clients[i],statusLine,strlen(statusLine),0);
							
							while((readBytes = read(fd,sendData,MAXBUFSIZE))>0)
								write(clients[i],sendData,readBytes);
							
							fclose(fp);
							bzero(postData,sizeof(postData));
						}
						//else if(fd == -1) file not found
						else{
							strncat(statusLine,version,strlen(version));
							strncat(statusLine," 404 Not Found",strlen(" 404 Not Found"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Type:",strlen("Content-Type:"));
							strncat(statusLine,"Invalid",strlen("Invalid"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
							strncat(statusLine,"Invalid",strlen("Invalid"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,connStatus,strlen(connStatus));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							strncat(statusLine,"<html><body>404 Not Found: URL does not exist: ",strlen("<html><body>404 Not Found: URL does not exist: "));
							strncat(statusLine,path,strlen(path));
							strncat(statusLine,"</body></html>",strlen("</body></html>"));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							printf("%s\n\n",statusLine);
							write(clients[i],statusLine,strlen(statusLine));
						}
					}//end of if isFileFormat == 1
					
					else{ //if fileFormat is not supported then send 501
						strncat(statusLine,version,strlen(version));
						strncat(statusLine," 501 Not Implemented",strlen(" 501 Not Implemented"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,"Content-Type:",strlen("Content-Type:"));
						strncat(statusLine,"NONE",strlen("NONE"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
						strncat(statusLine,"NONE",strlen("NONE"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,connStatus,strlen(connStatus));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						strncat(statusLine,"<head><title>501 Not Implemented</title></head>",strlen("<head><title>501 Not Implemented</title></head>"));
						strncat(statusLine,"<html><body>501 Not Implemented: Unsupported file format</body></html>",strlen("<html><body>501 Not Implemented: Unsupported file format</body></html>"));
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
				strncat(statusLine,"Content-Type:",strlen("Content-Type:"));
				strncat(statusLine,"NONE",strlen("NONE"));
				strncat(statusLine,"\n",strlen("\n"));
				strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
				strncat(statusLine,"NONE",strlen("NONE"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				strncat(statusLine,"<head><title>501 Not Implemented</title></head>",strlen("<head><title>501 Not Implemented</title></head>"));
				strncat(statusLine,"<html><body>501 Not Implemented: Unsupported method</body></html>",strlen("<html><body>501 Not Implemented: Unsupported method</body></html>"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				//printf("Status Line: %s\n\n",statusLine);
				write(clients[i],statusLine,strlen(statusLine));
			}//end of non GET/POST methods	
		}//end of else recvBytes>0
		
		if(!strstr(clientRequest,"Connection: Keep-alive"))
			setitimer(ITIMER_REAL,&timeout,NULL);
		else{
			//close the socket here
			shutdown(clients[i],SHUT_RDWR);
			close(clients[i]);
			clients[i]=-1;
			exit(0);
		}
		
	}//end of while
	
	//shutdown(clients[i],SHUT_RDWR);
	//close(clients[i]);
	//clients[i]=-1;
	printf("Done with response!\n");
}*/




//**************************************************************************************************************************************************
void respond(int n)
{
    char clientRequest[CLIENT_REQ_SIZE], *reqLine[3], sendData[MAXBUFSIZE], path[CLIENT_REQ_SIZE];
    int recvBytes, fd, readBytes;
    FILE *fp;
    char statusLine[STATUSLINE];
    char connStatus[50];
    clientId = n;
    int checkPost=0;
	char version[8];
	char contentLength[20];
	char postData[POSTMSGSIZE];
    char countString[50];
	char postFile[50] = "postFile";
	signal(SIGALRM, (void(*) (int)) alarmHandler);
	timeout.it_value.tv_sec = TIMEOUT;
	timeout.it_value.tv_usec = 0;
	timeout.it_interval = timeout.it_value;

	while(1){
		checkPost = 0;
		bzero(clientRequest, sizeof(clientRequest));
		bzero(reqLine, sizeof(reqLine));
		bzero(sendData, sizeof(sendData));
		bzero(path, sizeof(path));
		bzero(statusLine, sizeof(statusLine));
		bzero(connStatus, sizeof(connStatus));

		memset( (void*)clientRequest, (int)'\0', CLIENT_REQ_SIZE );
		recvBytes = recv(clients[n], clientRequest, CLIENT_REQ_SIZE, 0);

		if (!strstr(clientRequest,"Connection: Keep-alive"))
			strncpy(connStatus,"Connection: Keep-alive",strlen("Connection: Keep-alive"));
		else    
			strncpy(connStatus, "Connection: Close",strlen("Connection: Close"));

		bzero(statusLine,sizeof(statusLine));
		if (recvBytes < 0)
			perror("Error: recv() error");
		else if (recvBytes == 0)
			recvBytes = 0;
		else{
			timeout.it_value.tv_sec = TIMEOUT;
			timeout.it_value.tv_usec = 0;
			timeout.it_interval = timeout.it_value;
			
			if (!strstr(clientRequest,"Connection: Keep-alive"))
				strncpy(connStatus,"Connection: Keep-alive",strlen("Connection: Keep-alive"));
			else
				strncpy(connStatus,"Connection: Close",strlen("Connection: Close"));

			printf("Client #%d says: %s\n", n,clientRequest);

			reqLine[0] = strtok (clientRequest, " \t\n");
			
			if ((strncmp(reqLine[0],"GET\0",4)==0) || (strncmp(reqLine[0],"POST\0",5)==0)){
				if (strncmp(reqLine[0],"POST\0",5)==0){
					printf("********POST*********\n");
					checkPost = 1;
				}

				reqLine[1] = strtok (NULL, " \t");
				reqLine[2] = strtok (NULL, " \t\n");
				
				if (strncmp(reqLine[2],"HTTP/1.1",8)==0)
					strcpy(version,"HTTP/1.1");
				else
					strcpy(version,"HTTP/1.0");

				if (strncmp(reqLine[2],"HTTP/1.0",8)!=0 && strncmp(reqLine[2],"HTTP/1.1",8)!=0 ){
					strncat(statusLine,version,strlen(version));
					strncat(statusLine," 400 Bad Request",strlen(" 400 Bad Request"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Type:",strlen("Content-type:"));
					strncat(statusLine,"NONE",strlen("NONE"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
					strncat(statusLine,"NONE",strlen("NONE"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,connStatus,strlen(connStatus));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					strncat(statusLine,"<HEAD><TITLE>400 Bad Request Reason</TITLE></HEAD>",strlen("<HEAD><TITLE>400 Bad Request Reason</TITLE></HEAD>"));
					strncat(statusLine,"<html><BODY>>400 Bad Request Reason: Invalid HTTP-Version :",strlen("<BODY>>400 Bad Request Reason: Invalid HTTP-Version :"));
					strncat(statusLine,"HTTP",strlen("HTTP"));
					strncat(statusLine,"</BODY></html>",strlen("</BODY></html>"));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					printf("%s\n",statusLine);
					write(clients[n], statusLine, strlen(statusLine));
				}
				else
				{
					if (strncmp(reqLine[1],"/\0",2)==0)
						reqLine[1] = "/index.html";

					strcpy(path, ROOTDIR);
					strcpy(&path[strlen(ROOTDIR)], reqLine[1]);
					printf("file: %s\n", path);

					int isFileFormat;
					char *ext = strrchr(path,'.');
					printf("Extension: %s\n",ext);
					if (ext == NULL)
						isFileFormat = 0;
					else
						isFileFormat = checkFileFormat(ext);

					if (isFileFormat == 1){
						if ((fd=open(path, O_RDONLY))!=-1){
							fp = fopen(path,"r");;
							char *contentType = getContentType(ext);
							int size = getFileSize(fp);
							sprintf(contentLength,"%d",size);
							
							if(checkPost == 1){
								printf("Inside POST IF condition\n");
								sprintf(countString,"%d",COUNT);
								strcat(postFile, countString);
								FILE *fpPostFile = fopen(postFile,"w");
								
								if(fpPostFile!=NULL){
									fputs(clientRequest, fpPostFile);
									fclose(fpPostFile);
								}
								COUNT++;
								
								strncat(statusLine,"POST ",strlen("POST "));
							}

							strncat(statusLine,version,strlen(version));
							strncat(statusLine," 200 OK",strlen(" 200 OK"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Type:",strlen("Content-type:"));
							strncat(statusLine,contentType,strlen(contentType));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
							strncat(statusLine,contentLength,strlen(contentLength));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,connStatus,strlen(connStatus));
							if (checkPost == 1){
								strncat(statusLine,"\r\n\r\n",strlen("\r\n\r\n"));
								sprintf(postData,"<h1>Post Data</h1><html><body>%s</body></html>\n","Have a nice day!");
								strncat(statusLine,postData,strlen(postData));
								strncat(statusLine,"\r\n",strlen("\r\n"));
							}
							else{
								strncat(statusLine,"\r\n",strlen("\r\n"));
								strncat(statusLine,"\r\n",strlen("\r\n"));
							}
							printf("%s\n",statusLine);
							send(clients[n],statusLine,strlen(statusLine), 0);

							while ((readBytes = read(fd,sendData,MAXBUFSIZE))>0 )
								write (clients[n], sendData, readBytes);

							fclose(fp);
							bzero(postData,sizeof(postData));
						}
					
						else{
							strncat(statusLine,version,strlen(version));
							strncat(statusLine," 404 Not Found",strlen(" 404 Not Found"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Type:",strlen("Content-type:"));
							strncat(statusLine,"Invalid",strlen("Invalid"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
							strncat(statusLine,"Invalid",strlen("Invalid"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,connStatus,strlen(connStatus));                     
							strncat(statusLine,"\r\n",strlen("\r\n"));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							strncat(statusLine,"<html><BODY>404 Not Found: URL does not exist:",strlen("<BODY>404 Not Found: URL does not exist:"));
							strncat(statusLine,path,strlen(path));
							strncat(statusLine,"</BODY></html>",strlen("</BODY></html>"));
							strncat(statusLine,"\r\n",strlen("\r\n"));
							printf("%s\n",statusLine);
							write(clients[n], statusLine, strlen(statusLine));
						}    
					} //end of if isFileFormat == 1

					else //if fileFormat is not supported then send 501
					{
						strncat(statusLine,version,strlen(version));
						strncat(statusLine," 501 Not Implemented",strlen(" 501 Not Implemented"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,"Content-Type:",strlen("Content-type:"));
						strncat(statusLine,"NONE",strlen("NONE"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
						strncat(statusLine,"NONE",strlen("NONE"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,connStatus,strlen(connStatus));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						strncat(statusLine,"<HEAD><TITLE>501 Not Implemented</TITLE></HEAD>",strlen("<HEAD><TITLE>501 Not Implemented</TITLE></HEAD>"));
						strncat(statusLine,"<BODY>501 Not Implemented: File format not supported:",strlen("<BODY>501 Not Implemented: File format not supported:"));
						strncat(statusLine,version,strlen(version));
						strncat(statusLine,"</BODY></html>",strlen("</BODY></html>"));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						write(clients[n], statusLine, strlen(statusLine)); 
					}//end of 501 error
				}//end of 200 OK
			}//end of GET/POST

			//Methods other than GET/POST
			else{
				strncat(statusLine,"HTTP/1.1",strlen("HTTP/1.1"));
				strncat(statusLine,"\n",strlen("\n"));
				strncat(statusLine,"Content-Type:",strlen("Content-type:"));
				strncat(statusLine,"NONE",strlen("NONE"));
				strncat(statusLine,"\n",strlen("\n"));
				strncat(statusLine,"Content-Length:",strlen("Content-Length:"));
				strncat(statusLine,"NONE",strlen("NONE"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				strncat(statusLine,"<HEAD><TITLE>501 Not Implemented</TITLE></HEAD>",strlen("<HEAD><TITLE>501 Not Implemented</TITLE></HEAD>"));
				strncat(statusLine,"<BODY>501 Not Implemented: File format not supported:",strlen("<BODY>501 Not Implemented: File format not supported:"));
				strncat(statusLine,"HTTP/1.1",strlen("HTTP/1.1"));
				strncat(statusLine,"</BODY></html>",strlen("</BODY></html>"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				write(clients[n], statusLine, strlen(statusLine));
			}//end of non GET/POST Methods
		}//end of else recvBytes>0 

		if (!strstr(clientRequest,"Connection: Keep-alive"))
			setitimer(ITIMER_REAL,&timeout,NULL);
		else{
			shutdown (clients[n], SHUT_RDWR);
			close(clients[n]);
			clients[n]=-1;
			exit(0);
		}
	}//end of while
	
    printf("Done with response!\n");
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
				respond(i);
		}
		while(clients[i]!=-1)
			i = (i+1)%MAXCONN;
	}//end of while
	return 0;
}