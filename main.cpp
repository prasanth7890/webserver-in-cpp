#include <iostream>
#include <winsock2.h> // lib for socket programming in windows
#include <ws2tcpip.h>
#include <vector>
#include <fstream>
#include <signal.h>
#include "ThreadPool.h"

using namespace std;

#define DEFAULTPORT 6969
#define DEFAULTBUFFER 1000
#define DEFAULT_RESPONSE 5000

int parseRequest(char request[], int size);
vector<string> split(string s, char d);
bool isPathPresent(string path);
void handleRequest(SOCKET AccpetSocket);

bool stop = false;
SOCKET globalListenSocket = INVALID_SOCKET; 


//The WSAStartup function is used to start or initialise winsock library.
// It takes 2 parameters ; the first one is the version we want to load and second one is a WSADATA structure
// which will hold additional information after winsock has been loaded.
int init() {
    WSADATA wsaData; 
    int result = WSAStartup(MAKEWORD(2,2), &wsaData); 
    if(result != 0) {
        cout << "WSAStartup failed: " << result <<endl;
        WSACleanup();
        return 1;
    }
    cout << "WSAStartup Initialized..." << endl;


    // creating socket 
    SOCKET s = socket(AF_INET,SOCK_STREAM,0);
    if(s == INVALID_SOCKET) {
        cout << "Socket creation failed: " << WSAGetLastError() << " \n";
        WSACleanup();
        return 1;
    }
    cout << "Socket created... \n";

    // binding the socket
    sockaddr_in server;

    hostent* localHost = gethostbyname("");
    char* localIP = inet_ntoa(*(struct in_addr *)*localHost->h_addr_list);
    
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(DEFAULTPORT);

    
    if(bind(s, (SOCKADDR*) &server, sizeof(server)) == SOCKET_ERROR) {
        cout << "Socket binding failed : \n";
        closesocket(s);
        WSACleanup();
        return 1;
    }
    cout << "Socket binded at port " << DEFAULTPORT << "\n";

    // listen for incoming requests
    if(listen(s, 4) == SOCKET_ERROR) {
        cout << "socket failed to listen : \n";
        closesocket(s);
        WSACleanup();
        return 1;
    }
    cout << "Socket is listening for incoming connections... \n";

    globalListenSocket = s;

    ThreadPool pool(4);    

    while(!stop) {
        SOCKET AcceptSocket = accept(s, NULL, NULL);
        if (AcceptSocket != INVALID_SOCKET) {
           pool.addTask([AcceptSocket] {handleRequest(AcceptSocket);});
        }
        else {
            if(stop) break;

            cout <<"Accept failed with Error - " << WSAGetLastError() << "\n";
            closesocket(s);
            break;
        }
    }

    cout << "waiting for worker threads to finish... \n";
    pool.shutDown();

    WSACleanup();
    cout << "Server stopped!" << endl;
}

void sighandler(int sig) {
    if(sig == SIGINT) {
        cout << "stop the program! \n";
        stop = true;
        
        if(globalListenSocket != INVALID_SOCKET)  {
            closesocket(globalListenSocket);
            globalListenSocket = INVALID_SOCKET;
        }
    }
}

int main()
{
    signal(SIGINT, sighandler);
    cout << "Starting web server...\n";
    cout << "Main thread ID: " << std::this_thread::get_id() << endl;
    init();
}

int parseRequest(char request[], int size) {
    // returns 0 if everything is good, else returns the appropriate integer code
    string req = request;
    vector<string> v = split(req, '\n');

    //finding if its a get request or not
    vector<string> reqFirstLine = split(v[0], ' ');
    if(reqFirstLine[0].compare("GET")) {
        cout << "Error - Not a get request " << reqFirstLine[0] << "\n";
        return 1; 
    }

    // if the requsted path found
    if(!isPathPresent(reqFirstLine[1])) {
        return 2;
    }

    return 0;    
}


vector<string> split(string req, char d) {
    vector<string> output;
    int i=0;
    string temp = "";
    while(i<req.size()) {
        if(req[i] == d) {
            output.push_back(temp);
            temp = "";
        }
        temp = temp + req[i];
        i++;
    }
    return output;
}

bool isPathPresent(string path) {
    // if the requested resource is present, then it returns true else return false.
    if(path.compare("/")) {
        return true;
    }

    cout << "Error - Requested Page not Found! \n";
    return false;
}

void handleRequest(SOCKET AcceptSocket) {
    cout << "Handling request on Thread ID: " << std::this_thread::get_id() << endl;
    cout << "connection established! \n";

    // receiving the request from client and storing it in buffer
    char buffer[DEFAULTBUFFER] = {0}; 
    int res = recv(AcceptSocket, buffer, DEFAULTBUFFER, 0);

    if(res == SOCKET_ERROR) {
        cout << "receiving failed \n";
        closesocket(AcceptSocket);
        return;
    }
    else if(res == 0) {
        cout << "connection closed. \n";
        closesocket(AcceptSocket);
        return;
    }
    else if(res > 0) {
        cout << "Bytes Recevied : " << res << "\n";
    }

    cout << "----------- Http Request: -----------\n" << buffer << "\n";

    // parsing the request
    int output = parseRequest(buffer, res);
    if(output != 0) {
        cout << "something went wrong in parsingRequest" << endl;
        closesocket(AcceptSocket);
        return;
    }

    ifstream ReadFile("home\\index.html");
    if (!ReadFile.is_open()) {
        cout << "Error: Could not open the file!" << endl;
        closesocket(AcceptSocket);
        return;
    }

    char ch;
    string response = ""; 

    while(ReadFile.get(ch)) {
        response.push_back(ch);
    }
    ReadFile.close();

    // adding headers to the response
    string httpResponse = "HTTP/1.1 200 OK\r\n";
    httpResponse += "Content-Type: text/html\r\n"; 
    httpResponse += "Content-Length: " + to_string(response.length()) + "\r\n";
    httpResponse += "Connection: close\r\n\r\n";
    httpResponse += response;

    // sending response to client
    if(send(AcceptSocket, httpResponse.c_str(), httpResponse.length(), 0) == SOCKET_ERROR) {
        cout << "sending failed \n";
        closesocket(AcceptSocket);
        return;
    } 

    cout << "----------- Http Response: -----------\n" << httpResponse << "\n";
    
    closesocket(AcceptSocket);
}