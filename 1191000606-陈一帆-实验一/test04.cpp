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
        memcpy(httpheader->method, "GET", 3);
        memcpy(httpheader->url, &p[4], strlen(p) - 13);
    }
    else if (p[0] == 'P')
    {
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

boolean ParseDate(char *buffer, char *tempDate)
{
    const char *delim = "\r\n";
    const char *field = "Date";

    char *p = strtok(buffer, delim);
    while (p)
    {
        if (strstr(p, field) != NULL)
        {
            memcpy(tempDate, &p[6], strlen(p) - 6);
            return TRUE;
        }
        p = strtok(NULL, delim);
    }
    return TRUE;
}

void makeNewHTTP(char *buffer, char *value)
{
    const char *field = "Host";
    const char *newfield = "If-Modified-Since: ";
    char temp[MAXSIZE];
    ZeroMemory(temp, MAXSIZE);
    char *pos = strstr(buffer, field);
    int i = 0;
    for (i = 0; i < strlen(pos); i++)
    {
        temp[i] = pos[i];
    }
    *pos = '\0';
    while (*newfield != '\0')
    {
        *pos++ = *newfield++;
    }
    while (*value != '\0')
    {
        *pos++ = *value++;
    }
    *pos++ = '\r';
    *pos++ = '\n';
    for (i = 0; i < strlen(temp); i++)
    {
        *pos++ = temp[i];
    }
}

void makeFilename(char *url, char *filename)
{
    while (*url != '\0')
    {
        if (*url != '/' && *url != ':' && *url != '.')
        {
            *filename++ = *url;
        }
        url++;
    }
    strcat(filename, ".txt");
}

void makeCache(char *buffer, char *url)
{
    char *p, *ptr, num[10], tempBuffer[MAXSIZE + 1];
    const char *delim = "\r\n";
    ZeroMemory(num, 10);
    ZeroMemory(tempBuffer, MAXSIZE + 1);
    memcpy(tempBuffer, buffer, strlen(buffer));
    p = strtok(tempBuffer, delim);
    memcpy(num, &p[9], 3);
    if (strcmp(num, "200") == 0)
    {
        char filename[100] = {0};
        makeFilename(url, filename);
        FILE *out;
        out = fopen(filename, "w");
        fwrite(buffer, sizeof(char), strlen(buffer), out);
        fclose(out);
        printf("-------------����ɹ�----------------\n");
    }
}

BOOL getCache(char *buffer, char *filename)
{
    char *p, *ptr, num[10], tempBuffer[MAXSIZE + 1];
    const char *delim = "\r\n";
    ZeroMemory(num, 10);
    ZeroMemory(tempBuffer, MAXSIZE + 1);
    memcpy(tempBuffer, buffer, strlen(buffer));
    p = strtok(tempBuffer, delim); //????????
    memcpy(num, &p[9], 3);
    if (strcmp(num, "304") == 0)
    {

        printf("-------------������ڣ�ʹ�û���----------------\n");
        ZeroMemory(buffer, strlen(buffer));
        FILE *in = NULL;
        if ((in = fopen(filename, "r")) != NULL)
        {
            fread(buffer, sizeof(char), MAXSIZE, in);
            fclose(in);
        }
        return false;
    }
    else
    {
        printf("-------------������ڣ����Ѹ���----------------\n");
    }
    return true;
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

//�ò���������Ҫ��һ���������͵Ĳ���
unsigned int __stdcall ProxyThread(LPVOID params)
{
    SOCKET acceptSocket = (SOCKET)params;
    char *Buffer = (char *)malloc(sizeof(char) * MAXSIZE);
    ZeroMemory(Buffer, MAXSIZE);

    int receSize = recv(acceptSocket, Buffer, MAXSIZE, 0);

    HttpHeader *httpheader = parseHttp(Buffer, receSize);

    printf("�û������ı���\n%s", Buffer);

    if (strcmp(httpheader->url, INVILID_WEBSITE) == 0)
    {
        printf("-------------Sorry!!!�����վ���ܷ���----------------\n");
        closesocket(acceptSocket);
        delete Buffer;
        return 0;
    }

    if (strstr(httpheader->url, FISHING_WEB_SRC) != NULL)
    {
        printf("-------------�Ѵ�%sת�Ƶ�%s ----------------\n", FISHING_WEB_SRC, FISHING_WEB_DEST);
        memcpy(httpheader->host, FISHING_WEB_HOST, strlen(FISHING_WEB_HOST) + 1);
        memcpy(httpheader->url, FISHING_WEB_DEST, strlen(FISHING_WEB_DEST));
    }

    char *filename = (char *)malloc(sizeof(char) * 100);
    ZeroMemory(filename, 100);
    makeFilename(httpheader->url, filename);

    FILE *in;
    BOOL haveCache = false; //��û�л��棬�����ò��û���
    BOOL needCache = true;  //�費��Ҫ����
    if ((in = fopen(filename, "rb")) != NULL)
    {
        printf("-------------�л���----------------\n");
        char *fileBuffer = (char *)malloc(sizeof(char) * MAXSIZE);
        ZeroMemory(fileBuffer, MAXSIZE);
        fread(fileBuffer, sizeof(char), MAXSIZE, in);
        fclose(in);

        char *date_str = (char *)malloc(sizeof(char) * 30);
        ZeroMemory(date_str, 30);
        ParseDate(fileBuffer, date_str);
        //����Ŀ�ķ�������������Buffer���棬����httpHeader����
        delete fileBuffer;
        makeNewHTTP(Buffer, date_str);

        haveCache = true;
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

    printf("����Ŀ��������ı���\n%s", Buffer);

    delete Buffer;
    char *bigBuffer = (char *)malloc(sizeof(char) * MAXMAXSIZE);

    receSize = recv(serverSocket, bigBuffer, MAXMAXSIZE, 0);

    printf("Ŀ������������ı���\n%s", bigBuffer);

    if (haveCache)
    {
        needCache = getCache(bigBuffer, filename);
    }
    if (needCache)
    {
        makeCache(bigBuffer, httpheader->url);
    }

    send(acceptSocket, bigBuffer, receSize + 1, 0);
    printf("���ظ��û��ı���\n%s", bigBuffer);

    delete bigBuffer;
    closesocket(acceptSocket);
    closesocket(serverSocket);
    _endthread();
    return 0;
}