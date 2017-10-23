#Contents
There is one folder:
-	webServer

webServer has the following contents:
- webServer.c
- Makefile
- www (document root folder)
- ws.conf (the server config file)

-------------------------------------------------------------------------------------

#How to run the code:
To run: type make on command line from webServer folder
To clean the object file: run make clean on command line from webServer folder

-------------------------------------------------------------------------------------

#What has been implemented
- Web server that supports multiple eclient connections
- clients have clientID
- fork() is used to create multiple client requests
- TCP is implemented
- addrinfo struct is used for host addresses

-------------------------------------------------------------------------------------
#Extra credit
- Timeout is implemented for clients. Default timeout value is 10s and it can be configred through ws.conf file
- POST is implemented by creating a form in index.html which renders the page post.html

-------------------------------------------------------------------------------------

#How it works
- reads the ws config file for port number, document root and timeout value
- starts the Web Server
- Accepts connections from clients and calls getHTTPResponse() function
- getHTTPResponse() function parses the clientRequest and generates the response html for the browser to display
- It also checks for supported  file extensions
- Errors 400, 404, 501 are implemented