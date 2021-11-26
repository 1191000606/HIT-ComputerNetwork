/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include <vector>
using std::vector;
#include <iostream>
using std::cout;

// system support
extern void fwd_LocalRcv(char *pBuffer, int length);

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char *pBuffer, int type);

extern unsigned int getIpv4Address( );

// implemented by students

struct routeTableItem
{
	unsigned int destIP;     //Ŀ��IP
	unsigned int mask;       // ����
	unsigned int masklen;    // ���볤��
	unsigned int nexthop;    // ��һ��
};

vector<routeTableItem> m_table; 

void stud_Route_Init()
{
	m_table.clear();
	return;
}

void stud_route_add(stud_route_msg *proute)
{
	routeTableItem newTableItem;
	newTableItem.masklen = ntohl(proute->masklen);  //��һ���޷��ų��������������ֽ�˳��ת��Ϊ�����ֽ�˳��
	newTableItem.mask = (1<<31)>>(ntohl(proute->masklen)-1); //֪�����볤�Ⱥ��������һ������������
	newTableItem.destIP = ntohl(proute->dest);
	newTableItem.nexthop = ntohl(proute->nexthop);
	m_table.push_back(newTableItem);
	return;
}

int stud_fwd_deal(char *pBuffer, int length)
{

	int TTL = (int)pBuffer[8];  //�洢TTL
	int headerChecksum = ntohl(*(unsigned short*)(pBuffer+10)); 
	int DestIP = ntohl(*(unsigned int*)(pBuffer+16));
      int headsum = pBuffer[0] & 0xf; 

	if(DestIP == getIpv4Address()) //�жϷ����ַ�뱾����ַ�Ƿ���ͬ
	{
		fwd_LocalRcv(pBuffer, length); //�� IP �����Ͻ������ϲ�Э�� 
		return 0;
	}

	if(TTL <= 0) //TTL �ж� С��0 ����ת�� ���� IP ����
	{
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR); //���� IP ����
		return 1;
	}

	//����ƥ��λ
	bool Match = false;
	unsigned int longestMatchLen = 0;//���ƥ�䳤��
	int bestMatch = 0;
	// �ж������Ƿ�ƥ��
	for(int i = 0; i < m_table.size(); i ++)//ÿ��·�������ȥƥ��
	{
		if(m_table[i].masklen > longestMatchLen && m_table[i].destIP == (DestIP & m_table[i].mask)) //���ǰ׺ƥ��,���ĸ�·������Ŀ��IP��ַƥ���ǰ׺�
		{
			bestMatch = i;
			Match = true;
			longestMatchLen = m_table[i].masklen;
		}
	}

	if(Match) //ƥ��ɹ�
	{
		char *buffer = new char[length];
            memcpy(buffer,pBuffer,length);
            buffer[8]--; //TTL - 1
		int sum = 0;
            unsigned short int localCheckSum = 0;
            for(int j = 1; j < 2 * headsum +1; j ++)
            {
                if (j != 6){ //����У��͵�ʱ��У����ֶ�Ҫ����0��
                    sum = sum + (buffer[(j-1)*2]<<8)+(buffer[(j-1)*2+1]);
                    sum %= 65535; 
                }
            }
            //���¼���У���
           	localCheckSum = htons(~(unsigned short int)sum);
		memcpy(buffer+10, &localCheckSum, sizeof(unsigned short));
		// ������һ��Э��
		fwd_SendtoLower(buffer, length, m_table[bestMatch].nexthop);
		return 0;
	}
	else //ƥ��ʧ��
	{
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE); //���� IP ����
		return 1;
	}
	return 1;
}

