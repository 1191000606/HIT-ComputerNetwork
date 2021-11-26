//#include "stdafx.h" //���� VS ��Ŀ������Ԥ����ͷ�ļ�
#include <stdlib.h>
#include <time.h>
#include <WinSock2.h>
#include <fstream>

#define CMD_LENGTH 128
#define BUFFER_LENGTH 1022 //��������С������̫���� UDP ������ ֡�а�����ӦС�� 1480 �ֽڣ�

//һЩ���ڷ�����״̬��ʹ�õĲ�����Ϊ���ú����ķ������Ϊȫ�ֱ���
#define SEND_WIND_SIZE 5 //���ʹ��ڴ�СΪ 5��SR ��Ӧ���� W1 + W2 <= N��W1 Ϊ���ʹ��ڴ�С��N Ϊ���кŸ�����,����w1=w2
#define RECV_WIND_SIZE 5
#define SEQ_SIZE 20    //���кŵĸ������� 0~19 ���� 20 ��,��������ڴ�С��Ϊ 1����Ϊͣ-��Э��
#define PACKET_SIZE 32 // ���е����ݣ�һ���ܷ�Ϊ���ٸ����ݰ�

//���Ͷ�
BOOL ack[SEQ_SIZE]; //false��ʾû��ȷ��
int currentAck;     //��ǰ��ȷ�ϵ�ack
int currentSeq;     //�����Χ��[0,19],��Ҫ���͵����ݴ������,19��������һ����0
int totalIndex;     //�����Χ��[0,33]����Ҫ���͵��������ܵ��еڼ���,��������33
int ackNums;        //һ���ж��ٸ�Package��ȷ�Ϲ���
int waitCount;

//���ն�
short isRecved[SEQ_SIZE]; //ֵΪ-1��ʾ�����Ż�û�л��棬����0��ʾ�����ű����浽����
int currentRecv;          //���ڵ���ʼ�㣬��δ�յ����ݵ��Ǹ�

void printTips()
{
    printf("You can input listed commands\n");
    printf("|     time to get current time              |\n");
    printf("|     quit to exit client                   |\n");
    //����client��ack��ʧ�ĸ��ʣ�����sever��packet��ʧ�ĸ���
    printf("|     begin [X] to begin and set loss radio |\n");
}

/*
Method:    getCurTime
FullName:  getCurTime
Access:    public
Returns:   void
Qualifier: ��ȡ��ǰϵͳʱ�䣬������� ptime ��
Parameter: char * ptime
*/
void getCurTime(char *ptime)
{
    char buffer[128];
    memset(buffer, 0, sizeof(buffer));
    time_t c_time;
    struct tm *p;
    time(&c_time);
    p = localtime(&c_time);
    sprintf(buffer, "%d/%d/%d %d:%d:%d",
            p->tm_year + 1900,
            p->tm_mon + 1,
            p->tm_mday,
            p->tm_hour,
            p->tm_min,
            p->tm_sec);
    strcpy(ptime, buffer);
}

/*
Method: seqRecvAvailable
FullName: seqRecvAvailable
Access: public
Returns: short
Qualifier: 1��ʾ�ڴ�����,2��ʾ�����Ѿ�������,0��ʾ���ڻ�û��
*/
short seqRecvIsAvailable(int recvSeq)
{
    int maxRecved = (currentRecv + RECV_WIND_SIZE - 1) % SEQ_SIZE; //��߿ɱ�Buffer��Seq

    if (maxRecved > currentRecv)
    {
        if (recvSeq >= currentRecv && recvSeq <= maxRecved)
        {
            return 1;
        }

        if (currentRecv >= RECV_WIND_SIZE)
        {
            if (recvSeq <= currentRecv - 1 && recvSeq >= currentRecv - RECV_WIND_SIZE)
            {
                return 2;
            }
        }
        else
        {
            if (recvSeq <= currentRecv - 1 || recvSeq >= currentRecv + SEQ_SIZE - RECV_WIND_SIZE)
            {
                return 2;
            }
        }
    }
    else
    {
        if (recvSeq >= currentRecv || recvSeq <= maxRecved)
        {
            return 1;
        }
        if (recvSeq <= currentRecv - 1 && recvSeq >= currentRecv - RECV_WIND_SIZE)
        {
            return 2;
        }
    }

    return 0;
}

/*
Method:    seqSendIsAvailable
FullName:  seqSendIsAvailable
Access:    public
Returns:   bool
Qualifier: ���Ͷ˵�ǰ���к� curSeq �Ƿ����
*/
bool seqSendIsAvailable()
{
    int step;
    step = currentSeq - currentAck;
    step = step >= 0 ? step : step + SEQ_SIZE;
    //���к��Ƿ��ڵ�ǰ���ʹ���֮��
    if (step >= SEND_WIND_SIZE)
    {
        return false;
    }

    if (ack[currentSeq] == false) //����Ҫ�����ڵ�δack���Ĳſ���
    {
        return true;
    }

    return false;
}

/*
Method:    timeoutHandler
FullName:  timeoutHandler
Access:    public
Returns:   void
Qualifier: ��ʱ�ش�������
*/
void timeoutHandler()
{
    printf("There is a time out\n");

    if (currentSeq > currentAck)
    {
        totalIndex -= (currentSeq - currentAck);
    }
    else if (currentSeq < currentAck)
    {
        totalIndex -= (currentSeq - currentAck + 20);
    }
    currentSeq = currentAck;
    printf("currentSeq->%d, currentAck->%d, totalIndex->%d\n", currentSeq, currentAck, totalIndex);
}

/*
Method:    ackHandler
FullName:  ackHandler
Access:    public
Returns:   void
Qualifier: �յ� ack��ȡ����֡�ĵ�һ���ֽ�
���ڷ�������ʱ����һ���ֽڣ����кţ�Ϊ 0��ASCII��ʱ����ʧ�ܣ���˼�һ �ˣ��˴���Ҫ��һ��ԭ
Parameter: char c
*/
void ackHandler(char c)
{
    unsigned char index = (unsigned char)c - 1; //���кż�һ

    if (currentAck <= currentSeq && (currentAck <= index && index < currentSeq))
    {
        printf("When window=[%d,%d] received ack%d\n", currentAck, (currentAck+SEND_WIND_SIZE-1)%SEQ_SIZE, index);
        ack[index] = true;
    }
    else if (currentSeq < currentAck && (index >= currentAck || index < currentSeq))
    {
        printf("When window=[%d,%d] received ack%d\n", currentAck, (currentAck+SEND_WIND_SIZE-1)%SEQ_SIZE, index);
        ack[index] = true;
    }

    while (true)
    {
        if (ack[currentAck])
        {
            waitCount = 0;
            ack[currentAck] = false;
            currentAck++;
            currentAck %= SEQ_SIZE;
            currentSeq++;
            currentSeq %= SEQ_SIZE;
            totalIndex++;
            ackNums++;
        }
        else
        {
            break;
        }
    }
}

/*
Method: lossInLossRatio
FullName: lossInLossRatio
Access: public
Returns: BOOL
Qualifier: ���ݶ�ʧ���������һ�����֣��ж��Ƿ�ʧ,��ʧ�򷵻�TRUE�����򷵻� FALSE
Parameter: float lossRatio [0,1]
*/
BOOL lossInLossRatio(float lossRatio)
{
    int lossBound = (int)(lossRatio * 100);
    int r = rand() % 101;
    if (r <= lossBound)
    {
        return TRUE;
    }
    return FALSE;
}

//������
int main(int argc, char *argv[])
{
    //�����׽��ֿ⣨���룩
    WORD wVersionRequested;
    WSADATA wsaData;
    //�׽��ּ���ʱ������ʾ
    int err;
    //�汾 2.2
    wVersionRequested = MAKEWORD(2, 2);
    //���� dll �ļ� Scoket ��
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0)
    {
        //�Ҳ��� winsock.dll
        printf("WSAStartup failed with error: %d\n", err);
        return -1;
    }
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        printf("Could not find a usable version of Winsock.dll\n");
        WSACleanup();
        return -1;
    }

    while (true)
    {
        printf("Please choose to be sender or receiver or quit\n");
        printf("eg.sender 127.0.0.1 8000 or receiver 127.0.0.1 8000\n");

        char cmdBuffer[CMD_LENGTH];
        ZeroMemory(cmdBuffer, sizeof(cmdBuffer));

        char mode[10];
        ZeroMemory(mode, 10);
        char ip[20];
        ZeroMemory(ip, 20);
        int port;
        gets(cmdBuffer);
        int ret = sscanf(cmdBuffer, "%s %s %d", &mode, &ip, &port);

        if (!strcmp(mode, "receiver"))
        {
            printf("I am a receiver(client) and sender(server) is on %s:%d\n", ip, port);

            SOCKET mySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            SOCKADDR_IN addrServer;
            addrServer.sin_addr.S_un.S_addr = inet_addr(ip);
            addrServer.sin_family = AF_INET;
            addrServer.sin_port = htons(port);

            printTips();

            while (true)
            {
                ZeroMemory(cmdBuffer, sizeof(cmdBuffer));
                gets(cmdBuffer);

                char command[10];
                ZeroMemory(command, 10);
                float lossRadio;
                ret = sscanf(cmdBuffer, "%s %f", &command, &lossRadio);

                if (!strcmp(command, "begin"))
                {
                    char *buffer = (char *)malloc((BUFFER_LENGTH + 2) * sizeof(char));
                    ZeroMemory(buffer, BUFFER_LENGTH + 2);
                    sendto(mySocket, "hello", strlen("hello") + 1, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
                    int len = sizeof(SOCKADDR);

                    char *recvBuffer = (char *)malloc(BUFFER_LENGTH * RECV_WIND_SIZE * sizeof(char));
                    ZeroMemory(recvBuffer, BUFFER_LENGTH * RECV_WIND_SIZE * sizeof(char));
                    currentRecv = 0; //���ڵ���ʼ�㣬��δ�յ�����
                    for (int i = 0; i < SEQ_SIZE; i++)
                    {
                        isRecved[i] = -1;
                    }

                    //����recvBuffer�п��Դ����ݵ�������С��ţ���[0~RECV_WIND_SIZE-1]��ȡֵ
                    int availableStart = 0;
                    int availableEnd = RECV_WIND_SIZE - 1;

                    std::ofstream os("rece.txt");

                    srand((unsigned)time(NULL)); //�ж϶�������ʱ����
                    while (true)
                    {
                        ZeroMemory(buffer, BUFFER_LENGTH + 2);
                        int recvsize = recvfrom(mySocket, buffer, BUFFER_LENGTH + 2, 0, (SOCKADDR *)&addrServer, &len);

                        if (!strcmp(buffer, "All data is OK"))
                        {
                            os.close();
                            printf("All data is received\n");
                            break;
                        }

                        unsigned short seq = (unsigned short)buffer[1];
                        BOOL isReturnAck = false;

                        int ret = seqRecvIsAvailable(seq);
                        if (ret == 1)
                        {
                            printf("Recv packet%d,currentRecv=%d\n", seq, currentRecv);
                            if (isRecved[seq] == -1)
                            {
                                if (seq != currentRecv)
                                {
                                    isRecved[seq] = availableStart;
                                    memcpy(&recvBuffer[availableStart * BUFFER_LENGTH], &buffer[2], BUFFER_LENGTH);
                                    availableStart++;
                                    availableStart %= RECV_WIND_SIZE;
                                }
                                else
                                {
                                    os << &buffer[2], BUFFER_LENGTH;
                                    while (true)
                                    {
                                        currentRecv++;
                                        currentRecv %= SEQ_SIZE;

                                        if (isRecved[currentRecv] != -1)
                                        {
                                            os << &recvBuffer[BUFFER_LENGTH * isRecved[currentRecv]], BUFFER_LENGTH;
                                            isRecved[currentRecv] = -1;
                                            availableEnd++;
                                            availableEnd %= RECV_WIND_SIZE;
                                        }
                                        else
                                        {
                                            break;
                                        }
                                    }
                                }
                            }

                            isReturnAck = true;
                        }
                        else if (ret == 2)
                        {
                            printf("Recv packet%d,currentRecv=%d,has delievered\n", seq, currentRecv);
                            isReturnAck = true;
                        }
                        else
                        {
                            printf("Recv packet%d,currentRecv=%d,outside window\n", seq, currentRecv);
                        }

                        if (isReturnAck)
                        {
                            char ack[2];

                            ack[0] = seq + 1; //��Ϊ��0�������ģ���ʱ���յ�ʱ��Ҫ��һ
                            ack[1] = '\0';

                            int isloss = lossInLossRatio(lossRadio);
                            if (isloss)
                            {
                                printf("Ack%d loss\n", ack[0] - 1);
                            }
                            else
                            {
                                sendto(mySocket, ack, 2, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
                                printf("return ack%d to sender(server)\n", ack[0] - 1);
                            }
                            Sleep(100);
                        }
                    }

                    delete buffer;
                    delete recvBuffer;
                }
                else if (!strcmp(command, "quit"))
                {
                    closesocket(mySocket);
                    printf("You have exit from sender mode\n");
                    break;
                }
                else if (!strcmp(command, "time"))
                {
                    char *buffer = (char *)malloc(sizeof(char) * BUFFER_LENGTH);
                    ZeroMemory(buffer, BUFFER_LENGTH);
                    int len = sizeof(SOCKADDR);
                    sendto(mySocket, "time", strlen("time") + 1, 0, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
                    recvfrom(mySocket, buffer, BUFFER_LENGTH, 0, (SOCKADDR *)&addrServer, &len);
                    printf("time from sender(server):%s\n", buffer);
                    delete buffer;
                }
                else
                {
                    printf("Sorry, I can't understand this command\n");
                }
            }
        }
        else if (!strcmp(mode, "sender"))
        {
            printf("I am a sender(server) and run on %s:%d\n", ip, port);
            SOCKET mySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            int iMode = 1;
            ioctlsocket(mySocket, FIONBIO, (u_long FAR *)&iMode);

            SOCKADDR_IN addrServer;
            addrServer.sin_addr.S_un.S_addr = inet_addr(ip);
            addrServer.sin_family = AF_INET;
            addrServer.sin_port = htons(port);

            err = bind(mySocket, (SOCKADDR *)&addrServer, sizeof(SOCKADDR));
            if (err)
            {
                err = GetLastError();
                printf("Could not bind server socket on %s:%d", ip, port);
                continue;
            }

            std::ifstream icin;
            icin.open("test.txt");
            char allData[BUFFER_LENGTH * PACKET_SIZE]; //һ�ֽ������UDP���ݶ�ͷ������Ϊ���ݶε�һ���ֽ���0�Ļ��ᱨ���ڶ����ֽ�����š�
            ZeroMemory(allData, sizeof(allData));
            icin.read(allData, BUFFER_LENGTH * PACKET_SIZE);
            icin.close();
            printf("Success to read data in %d packages\n", sizeof(allData) / BUFFER_LENGTH); //��һ�н�����debug

            float loss_radio = 0.2;
            printf("Please set the loss radio of sent packet:");
            scanf("%f", &loss_radio);

            SOCKADDR_IN addrClient; //�������ڴ洢�ͻ��ĵ�ַ
            int length = sizeof(SOCKADDR);

            while (true)
            {
                char *requestBuffer = (char *)malloc(sizeof(char) * 120);
                int recvSize = recvfrom(mySocket, requestBuffer, 120, 0, (SOCKADDR *)&addrClient, &length);
                if (recvSize < 0)
                {
                    Sleep(200);
                    continue;
                }

                printf("recvive request from client:%s\n", requestBuffer);

                if (!strcmp(requestBuffer, "time"))
                {
                    char *buffer = (char *)malloc(sizeof(char) * BUFFER_LENGTH);
                    ZeroMemory(buffer, BUFFER_LENGTH);
                    getCurTime(buffer);
                    sendto(mySocket, buffer, BUFFER_LENGTH, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                    delete buffer;
                }
                else if (!strcmp(requestBuffer, "hello"))
                {   
                    currentAck = 0;
                    currentSeq = 0;
                    totalIndex = 0;
                    ackNums = 0;

                    printf("Begin to send data\n");
                    waitCount = 0; //��ʱ��ʼ

                    for (int i = 0; i < SEQ_SIZE; i++)
                    {
                        ack[i] = false;
                    }

                    int flag = 0;
                    srand((unsigned)time(NULL)); //�ж϶�������ʱ����
                    while (true)
                    {
                        if (seqSendIsAvailable() && totalIndex < PACKET_SIZE)
                        {
                            char *buffer = (char *)malloc(sizeof(char) * (BUFFER_LENGTH + 2));
                            buffer[0] = (char)255;
                            buffer[1] = (char)currentSeq;
                            memcpy(&buffer[2], allData + BUFFER_LENGTH * totalIndex, BUFFER_LENGTH);

                            printf("Send packet%d\n", currentSeq);
                            if (flag == 0)
                            {
                                flag = 1;
                                sendto(mySocket, buffer, BUFFER_LENGTH + 2, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                            }
                            else
                            {
                                if (lossInLossRatio(loss_radio))
                                {
                                    printf("Packet%d loss\n", currentSeq);
                                }
                                else
                                {
                                    sendto(mySocket, buffer, BUFFER_LENGTH + 2, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                                }
                            }

                            currentSeq++;
                            currentSeq %= SEQ_SIZE;
                            totalIndex++;
                            delete (buffer);
                            Sleep(100);
                        }

                        char *recvBuffer = (char *)malloc(sizeof(char) * 100);
                        ZeroMemory(recvBuffer, 100);
                        int len = sizeof(SOCKADDR);
                        recvSize = recvfrom(mySocket, recvBuffer, 100, 0, (SOCKADDR *)&addrClient, &len);
                        if (recvSize < 0)
                        {
                            waitCount++;
                            if (waitCount > 20)
                            {
                                timeoutHandler();

                                waitCount = 0;
                            }
                        }
                        else
                        {
                            ackHandler(recvBuffer[0]);

                            if (ackNums == PACKET_SIZE)
                            {
                                printf("All data is OK\n");
                                sendto(mySocket, "All data is OK", strlen("All data is Ok") + 1, 0, (SOCKADDR *)&addrClient, sizeof(SOCKADDR));
                                break;
                            }
                        }
                        Sleep(100);
                    }
                }

                printf("Do you want to continue?[Y/N]");
                char ifQuit[10];
                scanf("%s", ifQuit);

                if (ifQuit[0] == 'N')
                {
                    closesocket(mySocket);
                    printf("You have exit from sender mode\n");
                    break;
                }
            }
        }
        else if (!strcmp(mode, "quit"))
        {
            printf("This program is over\n");
            break;
        }
        else
        {
            printf("Sorry, I can't understand this command\n");
        }
    }

    WSACleanup();
    return 0;
}
