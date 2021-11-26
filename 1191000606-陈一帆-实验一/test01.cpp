#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#define MAXSIZE 65535

typedef struct HttpHeader
{
    char method[4];
    char url[1024];
    char host[1024];
    char cookie[1024 * 10];
} HttpHeader;

HttpHeader *parseHttp(char *Buffer, int size)
{
    char HttpHeadBuffer[size + 1];
    memcpy(HttpHeadBuffer, Buffer, size);
    const char *delim = "\r\n";
    char *p = strtok(HttpHeadBuffer, delim);

    HttpHeader *httpheader = (HttpHeader *)malloc(sizeof(HttpHeader));
    ZeroMemory(httpheader, sizeof(HttpHeader));

    if (p[0] == 'G')
    {
        //GET 方式
        memcpy(httpheader->method, "GET", 3);
        memcpy(httpheader->url, &p[4], strlen(p) - 13);
    }
    else if (p[0] == 'P')
    {
        //POST 方式
        memcpy(httpheader->method, "POST", 4);
        memcpy(httpheader->url, &p[5], strlen(p) - 14);
    }
    p = strtok(NULL, delim);
    while (p)
    {
        switch (p[0])
        {
        case 'H': //Host
            memcpy(httpheader->host, &p[6], strlen(p) - 6);
            break;
        case 'C': //Cookie
            if (strlen(p) > 8)
            {
                char header[8];
                ZeroMemory(header, sizeof(header));
                memcpy(header, p, 6);
                if (!strcmp(header, "Cookie"))
                {
                    memcpy(httpheader->cookie, &p[8], strlen(p) - 8);
                }
            }
            break;
        default:
            break;
        }
        p = strtok(NULL, delim);
    }
    return httpheader;
}

int main()
{
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD(2, 2);
    WSAStartup(wVersionRequested, &wsaData);

    SOCKET ProxyServer = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in ProxyServerAddr;
    ProxyServerAddr.sin_family = AF_INET;
    ProxyServerAddr.sin_port = htons(10240);
    ProxyServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    bind(ProxyServer, (SOCKADDR *)&ProxyServerAddr, sizeof(SOCKADDR));

    listen(ProxyServer, SOMAXCONN);

    SOCKET acceptSocket = accept(ProxyServer, NULL, NULL);
    char *Buffer = (char *)malloc(sizeof(char) * MAXSIZE);
    ZeroMemory(Buffer, MAXSIZE);

    int receSize = recv(acceptSocket, Buffer, MAXSIZE, 0);

    printf("%s", Buffer);

    HttpHeader *httpheader = parseHttp(Buffer, receSize);

    printf("method:%s\n", httpheader->method);
    printf("url:%s\n", httpheader->url);
    printf("host:%s\n", httpheader->host);
    printf("cookie:%s\n", httpheader->cookie);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(80);
    HOSTENT *hostent = gethostbyname(httpheader->host);

    in_addr Inaddr = *((in_addr *)*hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    connect(serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
    send(serverSocket, Buffer, strlen(Buffer) + 1, 0);
    receSize = recv(serverSocket, Buffer, MAXSIZE, 0);

    send(acceptSocket, Buffer, sizeof(Buffer), 0);

    delete Buffer;
    closesocket(acceptSocket);
    closesocket(serverSocket);
    return 0;
}
