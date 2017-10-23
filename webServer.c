//NETSYS PROGRAMMING ASSIGNMENT 2
//Chaitra Ramachandra

//libraries
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

//constants
#define MAXCONN 1000
#define MAXBUFSIZE 1024
#define STATUSLINE 2000
#define CLIENT_REQ_SIZE 99999

//Global variables
char *ROOTDIR;
char PORT[10];
char TIMEOUT_STRING[10];
int TIMEOUT;
int clientId;
int listenfd, clients[MAXCONN];
struct itimerval timeout;

//-------------------------------------TIMEOUT------------------------------------
//extra credit part
void clientTimeout(){  
	printf("Timing Out!\n");
	printf("Closing client #%d\n", clientId);
	shutdown(clients[clientId], SHUT_RDWR);
	close(clients[clientId]);
	clients[clientId]=-1; //reset the clientId
   	exit(0);
}


//----------------------------------GET FILESIZE---------------------------------
int getFileSize(FILE *fp){
	fseek(fp, 0, SEEK_END);
    return(ftell(fp));
}


//-------------------------CHECK FILE FORMAT---------------------------------
//This functions checks for the extention type requested by the client and returns 1 if it is supported, else  returns 0
int checkFileFormat(char *ext){
    FILE *fp;
	char formats[20][100];
    char wsBuffer[MAXBUFSIZE];
	
	//get current directory
	char *wsConfigFilePath = getenv("PWD");
	
    int fileSupported = 0;
	int wsConfigFileSize = 0; 
	int i = 0;
    
	if(wsConfigFilePath != NULL)
    	printf("ws.conf path: %s\n", wsConfigFilePath);

	fp=fopen(wsConfigFilePath,"r");
	wsConfigFileSize = getFileSize(fp);
	fseek(fp, 0, SEEK_SET);

    while(fgets(wsBuffer,wsConfigFileSize,fp)!=NULL) {
        strcpy(formats[i],wsBuffer);
        i++;
    }

	//string compare for supported file types
    int j=0;
    for(j=0;j<i+1;j++){
        if(strncmp(formats[j],ext,3)==0)
            fileSupported = 1;
    }

    fclose(fp);

	printf("File supported: %d\n", fileSupported);
    return fileSupported;
}


//---------------------------------GET CONTENT TYPE-----------------------------
//This function will check the extention and return the ContentType
//Reference: https://stackoverflow.com/questions/28631767/sending-images-over-http-to-browser-in-c
char *getContentType(char *ext){
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



//-------------------------------------READ WSCONFIG------------------------------
//This function reads the ws.conf file for Port number, Document root directory and Timeout value
void readWSConfigFile(){
    FILE *fp;
	char *wsConfFilePath = getenv("PWD");
    char wsConfFileBuffer[MAXBUFSIZE];
    char *token;

	strncat(wsConfFilePath,"/ws.conf", 8);
    printf("wsconf file path: %s\n", wsConfFilePath);

    if(fp = fopen(wsConfFilePath, "r")){
		int wsConfFileSize = getFileSize(fp);
		fseek(fp, 0, SEEK_SET);
		
		while(fgets(wsConfFileBuffer, wsConfFileSize, fp)!=NULL){
			if(strncmp(wsConfFileBuffer,"Listen",6)==0){
				token = strtok(wsConfFileBuffer," \t\n");
				token = strtok(NULL, " \t\n");
				strcpy(PORT, token);
				printf("Port Number: %s\n",PORT);
				bzero(wsConfFileBuffer,sizeof(wsConfFileBuffer));
				int portNum = atoi(PORT);
				if(portNum < 1024){
					perror("Error: Invalid PORT Number");
					exit(1);
				}
			}
			if(strncmp(wsConfFileBuffer,"DocumentRoot",12)==0){
				token = strtok(wsConfFileBuffer," \t\n");
				token = strtok(NULL, " \t\n");
				ROOTDIR=(char*)malloc(100);
				strcpy(ROOTDIR, token);
				printf("Root Directory: %s\n",ROOTDIR);
				bzero(wsConfFileBuffer,sizeof(wsConfFileBuffer));
			}
			
			//extra credit part
			if(strncmp(wsConfFileBuffer,"Timeout",7)==0){
				token = strtok(wsConfFileBuffer," \t\n");
				token = strtok(NULL, " \t\n");
				strcpy(TIMEOUT_STRING, token);
				printf("Default timeout value: %s\n",TIMEOUT_STRING);
				TIMEOUT = atoi(TIMEOUT_STRING);
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




//-----------------------------------START WEB SERVER------------------------------
//Reference: https://msdn.microsoft.com/en-us/library/windows/desktop/ms737530(v=vs.85).aspx
void startServer(){
	struct addrinfo *result;
    struct addrinfo *ptr;
    struct addrinfo hints;
    memset(&hints,0,sizeof(hints));
	
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
	//Reference: https://github.com/angrave/SystemProgramming/wiki/Networking,-Part-2:-Using-getaddrinfo
    if(getaddrinfo(NULL, PORT, &hints, &result) != 0){
        perror("Error: getaddrinfo()");
		exit(1);
    }
    
    for (ptr=result; ptr!=NULL; ptr=ptr->ai_next){
        if ((listenfd = socket(ptr->ai_family, ptr->ai_socktype, 0)) == -1)
            continue;
        if (bind(listenfd, ptr->ai_addr, ptr->ai_addrlen) == 0)
			break;
    }

    if (ptr==NULL){
        perror("Error: socket() or bind()");
		exit(1);
    }

    freeaddrinfo(result);

    if (listen(listenfd, MAXCONN)!=0){
        perror("Error: listen()");
		exit(1);
    }
}




//-----------------------GENERATE HTTP RESPONSE----------------------------
void getHTTPResponse(int i)
{
    clientId = i; //global value
	char clientRequest[CLIENT_REQ_SIZE];
	char *requestData[3];
	char sendData[MAXBUFSIZE];
	char path[CLIENT_REQ_SIZE];
    char statusLine[STATUSLINE];
    char connStatus[50];
	char version[8];
	
	int recvBytes;
	int fd;
	int readBytes;
    
	FILE *fp;
    
    int checkPost=0;
    
	//timeout part
	signal(SIGALRM,(void(*)(int))clientTimeout);
	timeout.it_value.tv_sec = TIMEOUT;
	timeout.it_value.tv_usec = 0;
	timeout.it_interval = timeout.it_value;

	while(1){
		//zero all the values so that next client can use it
		bzero(clientRequest,sizeof(clientRequest));
		bzero(requestData,sizeof(requestData));
		bzero(sendData,sizeof(sendData));
		bzero(path,sizeof(path));
		bzero(statusLine,sizeof(statusLine));
		bzero(connStatus,sizeof(connStatus));
		checkPost = 0;
		
		memset((void*)clientRequest,(int)'\0',CLIENT_REQ_SIZE);
		recvBytes = recv(clients[i],clientRequest,CLIENT_REQ_SIZE,0);
		
		//check for connection status in the clientRequest
		if(!strstr(clientRequest,"Connection: keep-alive"))
			strncpy(connStatus,"Connection: keep-alive",strlen("Connection: keep-alive"));
		else    
			strncpy(connStatus, "Connection: Close",strlen("Connection: Close"));

		bzero(statusLine,sizeof(statusLine));
		
		if(recvBytes < 0)
			perror("Error: recv() error");
		else if(recvBytes==0)
			recvBytes = 0;
		else{
			//timeout needs to be reset for another request
			timeout.it_value.tv_sec = TIMEOUT;
			timeout.it_value.tv_usec = 0;
			timeout.it_interval = timeout.it_value;
			
			if(!strstr(clientRequest,"Connection: keep-alive"))
				strncpy(connStatus,"Connection: keep-alive",strlen("Connection: keep-alive"));
			else
				strncpy(connStatus,"Connection: Close",strlen("Connection: Close"));

			printf("Client #%d says: %s\n", i,clientRequest);

			requestData[0] = strtok (clientRequest, " \t\n");
			
			if ((strncmp(requestData[0],"GET\0",4)==0) || (strncmp(requestData[0],"POST\0",5)==0)){
				if(strncmp(requestData[0],"POST\0",5)==0){
					printf("********POST*********\n");
					checkPost = 1;
				}
				
				//tokenize to get HTTP/1.1 or HTTP/1.0
				requestData[1] = strtok(NULL, " \t");
				requestData[2] = strtok(NULL, " \t\n");
				
				if(strncmp(requestData[2],"HTTP/1.1",8) == 0)
					strcpy(version, "HTTP/1.1");
				else
					strcpy(version, "HTTP/1.0");
				
				//Error 400 Bad Request
				if(strncmp(requestData[2],"HTTP/1.0",8)!=0 && strncmp(requestData[2],"HTTP/1.1",8)!=0){
					strncat(statusLine,version,strlen(version));
					strncat(statusLine," 400 Bad Request",strlen(" 400 Bad Request"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Type:None",strlen("Content-Type:None"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,"Content-Length:None",strlen("Content-Length:None"));
					strncat(statusLine,"\n",strlen("\n"));
					strncat(statusLine,connStatus,strlen(connStatus));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					strncat(statusLine,
						"<!DOCTYPE html><head><title>400 Bad Request Reason</title></head><body><h1>400 Bad Request: Invalid HTTP Version</h1></body></html>",
						strlen("<!DOCTYPE html><head><title>400 Bad Request Reason</title></head><body><h1>400 Bad Request: Invalid HTTP Version</h1></body></html>"));
					strncat(statusLine,"\r\n",strlen("\r\n"));
					printf("%s\n",statusLine);
					write(clients[i],statusLine,strlen(statusLine));
				} //end of 400 error
				
				else{ //if version_header is correct
					if (strncmp(requestData[1],"/\0",2)==0)
						requestData[1] = "/index.html";

					strcpy(path, ROOTDIR);
					strcpy(&path[strlen(ROOTDIR)], requestData[1]);
					//printf("file: %s\n", path);

					int isFileFormat;
					char *ext = strrchr(path,'.');
					if (ext == NULL)
						isFileFormat = 0;
					else
						isFileFormat = checkFileFormat(ext);
					
					char contentLength[20];
					
					if(isFileFormat==1){
						if((fd=open(path, O_RDONLY))!=-1){
							fp = fopen(path,"r");
							char *contentType = getContentType(ext);
							int size = getFileSize(fp);
							fseek(fp, 0, SEEK_SET);
							sprintf(contentLength,"%d",size);
							
							//For POST requests
							char postData[CLIENT_REQ_SIZE];
							if(checkPost==1){
								printf("Inside POST condition\n");
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
							strncat(statusLine,"\r\n\r\n",strlen("\r\n\r\n"));
							
							//For POST requests
							if(checkPost==1){
								sprintf(postData,
								"<!DOCTYPE html><head><title>Post Data</title></head><body><h1>%s</h1></body></html>\n","Have a nice day!");
								strncat(statusLine,postData,strlen(postData));
								strncat(statusLine,"\r\n",strlen("\r\n"));
							}
							printf("%s\n",statusLine);
							send(clients[i],statusLine,strlen(statusLine),0);

							while((readBytes = read(fd,sendData,MAXBUFSIZE))>0)
								write(clients[i],sendData,readBytes);

							fclose(fp);
							bzero(postData,sizeof(postData));
						}
					
						//Error 404 Not Found
						else{  //if fd == -1 404 error
							strncat(statusLine,version,strlen(version));
							strncat(statusLine," 404 Not Found",strlen(" 404 Not Found"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Type:Invalid",strlen("Content-type:Invalid"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,"Content-Length:Invalid",strlen("Content-Length:Invalid"));
							strncat(statusLine,"\n",strlen("\n"));
							strncat(statusLine,connStatus,strlen(connStatus));
							strncat(statusLine,"\r\n\r\n",strlen("\r\n\r\n"));
							strncat(statusLine,
							"<!DOCTYPE html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><br/><p>URL does not exist:",
							strlen("<!DOCTYPE html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><br/><p>URL does not exist:"));
							strncat(statusLine,path,strlen(path));
							strncat(statusLine,"</p></body></html>",strlen("</p></body></html>"));
							strncat(statusLine,"\r\n\r\n",strlen("\r\n\r\n"));
							printf("%s\n",statusLine);
							write(clients[i], statusLine, strlen(statusLine));								
						}//end of 404
					}//end of if isFileFormat == 1

					//Error 501 Unsupported File format
					else{ //if fileFormat is not supported then send 501
						strncat(statusLine,version,strlen(version));
						strncat(statusLine," 501 Not Implemented",strlen(" 501 Not Implemented"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,"Content-Type:None",strlen("Content-type:"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,"Content-Length:None",strlen("Content-Length:"));
						strncat(statusLine,"\n",strlen("\n"));
						strncat(statusLine,connStatus,strlen(connStatus));
						strncat(statusLine,"\r\n\r\n",strlen("\r\n\r\n"));
						strncat(statusLine,
						"<!DOCTYPE html><head><title>501 Not Implemented</title></head><body><h1>501 Not Implemented</h1><p>File format not supported: ",
						strlen("<!DOCTYPE html><head><title>501 Not Implemented</title></head><body><h1>501 Not Implemented</h1><p>File format not supported: "));
						strncat(statusLine,ext,strlen(ext));
						strncat(statusLine,"</p></body></html>",strlen("</p></body></html>"));
						strncat(statusLine,"\r\n",strlen("\r\n"));
						write(clients[i], statusLine, strlen(statusLine)); 
					}//end of 501
				}//end of 200 OK
			}//end of GET/POST

			//Error 501 Methods other than GET/POST
			else{
				strncat(statusLine,"HTTP/1.1",strlen("HTTP/1.1"));
				strncat(statusLine,"\n",strlen("\n"));
				strncat(statusLine,"Content-Type:None",strlen("Content-type:"));
				strncat(statusLine,"\n",strlen("\n"));
				strncat(statusLine,"Content-Length:None",strlen("Content-Length:"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				strncat(statusLine,
				"<!DOCTYPE html><head><title>501 Not Implemented</title></head><body><h1>501 Not Implemented</h1><p>Method not supported: ",
				strlen("<!DOCTYPE html><head><title>501 Not Implemented</title></head><body><h1>501 Not Implemented</h1><p>Method not supported</p></body></html>"));
				strncat(statusLine,"\r\n",strlen("\r\n"));
				write(clients[i], statusLine, strlen(statusLine));
			}//end of 501 for unsupported Methods
		}

		//check for keep-alive
		if (!strstr(clientRequest,"Connection: keep-alive"))
			setitimer(ITIMER_REAL,&timeout,NULL);
		else{
			shutdown(clients[i], SHUT_RDWR);
			close(clients[i]);
			clients[i]=-1;
			exit(0);
		}
	}//end while

    printf("My Ghod! Done with response!\n");
}






//-----------------------------------MAIN----------------------------------
int main(){
	//To create socket - Ref: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740496(v=vs.85).aspx
	struct sockaddr_in clientServer;
	int clientServerSize;
	int i;
    
	//Initialize array of clients to -1 because ClientID starts from #0
	for(i=0;i<MAXCONN;i++)
		clients[i]=-1;

	//read the ws.conf file
    readWSConfigFile();
	
	//start the Web Server
    startServer();

    i=0;
    while (1)
    {
        clientServerSize = sizeof(clientServer);
		
		//start accepting connections
        clients[i] = accept(listenfd, (struct sockaddr *)&clientServer, &clientServerSize);
		
        if (clients[i]<0)
            perror("Error: accept()");
        else
        {
			//for multiple connections
            if (fork()==0)
				//this function generates the http response
                getHTTPResponse(i);
        }

        while (clients[i]!=-1) 
            i = (i+1)%MAXCONN;
    }//end of while
    return 0;
}