#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#define MAXSIZE 65535
#define MAXMAXSIZE 65535000

typedef struct HttpHeader
{
    char method[4];
    char url[1024];
    char host[1024];
    char cookie[1024 * 10];
} HttpHeader;

#define INVILID_WEBSITE "http://www.4399.com/"
#define FISHING_WEB_SRC "http://today.hit.edu.cn/"
#define FISHING_WEB_DEST "http://jwts.hit.edu.cn/"
#define FISHING_WEB_HOST "jwts.hit.edu.cn"
SOCKET ProxyServer;
unsigned int __stdcall ProxyThread(LPVOID param);

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

    ProxyServer = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in ProxyServerAddr;
    ProxyServerAddr.sin_family = AF_INET;
    ProxyServerAddr.sin_port = htons(10240);
    ProxyServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    bind(ProxyServer, (SOCKADDR *)&ProxyServerAddr, sizeof(SOCKADDR));
    listen(ProxyServer, SOMAXCONN);

    while (true)
    {
        SOCKET acceptSocket = accept(ProxyServer, NULL, NULL);
        HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)acceptSocket, 0, 0);
        CloseHandle(hThread);
        Sleep(200);
    }

    closesocket(ProxyServer);
    WSACleanup();
    return 0;
}

//用不到参数都要加一个这样类型的参数
unsigned int __stdcall ProxyThread(LPVOID params)
{
    SOCKET acceptSocket = (SOCKET)params;
    char *Buffer = (char *)malloc(sizeof(char) * MAXSIZE);
    ZeroMemory(Buffer, MAXSIZE);

    int receSize = recv(acceptSocket, Buffer, MAXSIZE, 0);

    HttpHeader *httpheader = parseHttp(Buffer, receSize);

    if (strcmp(httpheader->url, INVILID_WEBSITE) == 0)
    {
        printf("\n=====================================\n\n");
        printf("-------------Sorry!!!这个网站不能访问----------------\n");
        closesocket(acceptSocket);
        delete Buffer;
        return 0;
    }

    if (strstr(httpheader->url, FISHING_WEB_SRC) != NULL)
    {
        printf("\n=====================================\n\n");
        printf("-------------已从%s转移到%s ----------------\n", FISHING_WEB_SRC, FISHING_WEB_DEST);
        memcpy(httpheader->host, FISHING_WEB_HOST, strlen(FISHING_WEB_HOST) + 1);
        memcpy(httpheader->url, FISHING_WEB_DEST, strlen(FISHING_WEB_DEST));
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(80);
    HOSTENT *hostent = gethostbyname(httpheader->host);

    in_addr Inaddr = *((in_addr *)*hostent->h_addr_list);
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    connect(serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr));
    send(serverSocket, Buffer, strlen(Buffer) + 1, 0);

    delete Buffer;
    char *bigBuffer = (char *)malloc(sizeof(char) * MAXMAXSIZE);

    receSize = recv(serverSocket, bigBuffer, MAXMAXSIZE, 0);

    printf("%s", bigBuffer);
    send(acceptSocket, bigBuffer, receSize + 1, 0);

    delete bigBuffer;
    closesocket(acceptSocket);
    closesocket(serverSocket);
    _endthread();
    return 0;
}