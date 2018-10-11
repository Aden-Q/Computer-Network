#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h>		//�̺߳�����
#include <windows.h>		//windows api��
#include <string.h>
#include <time.h>
#pragma comment (lib, "ws2_32.lib")		//���� ws2_32.dll�����ú�֮��Ͳ��ù���Ŀ������

#define SERVER_PORT 0614	//ѧ�ź���λ���������˿�
#define PAC_TYPE_LEN 1		//���ݰ������ֶγ���Ϊ1
#define PAC_CONTENT_LEN 3	//���ݰ����ݳ����ֶ�Ϊ3
#define PAC_TYPE 0
#define PAC_LENGTH 1
#define PAC_CONTENT 2

#define PAC_ERROR 0
#define PAC_RIGHT 1

//�������ݰ����
#define REQ_TIME "0"		//ʱ������
#define REQ_NAME "1"		//��������
#define REQ_LIST "2"		//�б�����
#define REQ_MSG "3"		//��Ϣ����
//��Ӧ���ݰ����
#define RES_TIME "4"		//ʱ����Ӧ
#define RES_NAME "5"		//������Ӧ
#define RES_LIST "6"		//�б���Ӧ
#define RES_MSG "7"			//��Ϣ��Ӧ
//ָʾ���ݰ���ʽ
#define COM_MSG "8"		//��Ϣָʾ

#define MAX_NUM 20		//����������20��������

int PAC_RECV(SOCKET *sServer, char *ptr, int length);
int PAC_SEND(SOCKET *sServer, char *pactype);
int PAC_TRANSMESSAGE(SOCKET *sServer, char *ptr, int *dest);

bool exitsignal = false;
int clientcount = 0;		//�Ѵ����߳���
struct clientlist {
	struct sockaddr_in saClient;	//�ͻ��˵�ַ�˿ںŵ���Ϣ
	SOCKET sServer;					//�����Ϊ�ÿͻ��˴������׽���
	int clientnumebr;				//�ͻ��˱��
	bool isalive = false;			//�û����Ƿ���
} saClientlist[MAX_NUM];

//�����̺߳���
unsigned int __stdcall ThreadFun(PVOID pM)
{
	int ret;		//������Ϣ����ֵ
	int nLeft = 0;
	char hlomsg[] = "9Hello";
	char message[1000] = "";
	char pactype[PAC_TYPE_LEN + 1];		//���ݰ������ַ���1λ�����һλ\0��־�ַ�������
	char paclength[PAC_CONTENT_LEN + 1];	//���ݰ����ݳ����ֶ�3λ�����һλ\0��־�ַ�������
	pactype[PAC_TYPE_LEN] = '\0';		//��ǰд���ַ���������
	paclength[PAC_CONTENT_LEN] = '\0';	//��ǰд���ַ���������

	int flag;		//������

	printf("�߳�ID��Ϊ%4d�����߳�˵��Hello World\n", GetCurrentThreadId());

	//���̷߳���һ��Hello��Ϣ���ͻ���
	ret = send(*(SOCKET *)pM, (char *)&hlomsg, sizeof(hlomsg), 0);
	if (ret == SOCKET_ERROR)
	{
		printf("send() failed!\n");
	}

	while (true) {
		if (exitsignal == true)		//������߳��յ������źţ�����ֹ�������̣߳����߳���return��ʽ��ֹ����Ϊ��ȫ
			return -1;
		
		flag = PAC_RECV((SOCKET *)pM, (char *)&pactype, PAC_TYPE_LEN);	  //ָ����Ϣ���ܺ���ȥ���������ַ����ȵ����ݰ������ֶ�

		PAC_SEND((SOCKET *)pM, pactype);
	}

	return 0;
}

//��Ϣ���պ�������һ�������������׽��֣��ڶ����������ַ�ָ�룬����������ָʾҪ���յ���Ϣ���ͣ����ĸ�������ʾ��������Ϣ�ĳ���
int PAC_RECV(SOCKET *sServer, char *ptr, int length) {
	int nLeft = length;		//��ȡ�����ַ�����ֻ��Ҫһ���ַ�����
	int ret;				//��Ϣ����ֵ
	int rflag;				//��־����ֵ

	while (nLeft > 0) {
		//��������
		ret = recv(*sServer, ptr, nLeft, 0);
		if (ret == SOCKET_ERROR)
		{
			printf("%d nLeft recv failed!kkkkkk\n", nLeft);
			rflag = PAC_ERROR;
			break;
		}

		if (ret == 0)
		{
			printf("client has closed the connection!\n");
			rflag = PAC_ERROR;
		}
		//printf("ret = %d, nLeft = %d\n", ret, nLeft);				//����ָʾ���
		nLeft -= ret;
		ptr += ret;
	}

	if (!nLeft)		//�����ʽ�׽��ִ��������Ѿ����
		rflag = PAC_RIGHT;		//ָʾ��ȷ���أ������ַ�����������message����
	return rflag;
}

//��Ϣ���ͺ��������������������������Ӧ�Ķ���
int PAC_SEND(SOCKET *sServer, char *pactype)
{
	int rflag;		//ָʾ�����Ƿ�ɹ�
	int ret;		//���Է���ֵ
	int length = 0;		//��Ӧ���ݰ������ֶ�ҲҪ���ͳ�ȥ��Ĭ�ϳ���Ϊ0
	char slength[5];	//��¼���ȵ��ַ���,����Ϊ5���ֽ�
	char message[1000] = { 0 };
	char newmsg[100] = { 0 };
	char fmessage[1050] = { 0 };
	int dest;
	if (strcmp(pactype, REQ_TIME) == 0) {			//���������ʱ������ݰ���ϵͳ���û�ȡʱ��
		char timestring[25] = {0};
		time_t now;
		struct tm *tm_now;
		time(&now);
		tm_now = localtime(&now);
		char year[5], month[3], day[3], hour[3], minute[3], second[3];
		itoa(tm_now->tm_year + 1900, year, 10);
		itoa(tm_now->tm_mon + 1, month, 10);
		itoa(tm_now->tm_mday, day, 10);
		itoa(tm_now->tm_hour, hour, 10);
		itoa(tm_now->tm_min, minute, 10);
		itoa(tm_now->tm_sec, second, 10);
		strcat(timestring, year);
		strcat(timestring, "-");		//����ʱ�䵥λ֮����һ��'-'���ָ�
		strcat(timestring, month);
		strcat(timestring, "-");
		strcat(timestring, day);
		strcat(timestring, "-");
		strcat(timestring, hour);
		strcat(timestring, "-");
		strcat(timestring, minute);
		strcat(timestring, "-");
		strcat(timestring, second);
		strcat(message, RES_TIME);		//��־ʱ����Ӧ���ݰ�
		length = strlen(timestring);	//���ݳ���
		sprintf(slength, "%03d", length);	//����תΪ��λ�ַ�����������ǰ�油0
		strcat(message, slength);
		strcat(message, timestring);	//������ɣ��õ���װ�õ����ݰ�message
		printf("��ǰʱ��Ϊ%s\n", message, strlen(message));
		ret = send(*sServer, (char *)&message, strlen(message), 0);	//��������ʱ������ݰ�
	}
	else if (strcmp(pactype, REQ_NAME) == 0) {		//�����������������ֵ����ݰ���ϵͳ���û�ȡ����
		char hostname[100] = { 0 };
		if (gethostname(hostname, sizeof(hostname)) < 0) {
			//���󲢷���
		}
		printf("hostname:%s\n", hostname);
		length = strlen(hostname);		//��hostname�ĳ���
		sprintf(slength, "%03d", length);//������ת����λ�ַ���������ǰ�油0
		strcat(message, RES_NAME);		//�����Ӧ���ݰ���־λ
		strcat(message, slength);		//�����Ӧ���ݰ������ֶ�
		strcat(message, hostname);		//�����Ӧ���ݰ������ֶ�
		ret = send(*sServer, (char *)&message, strlen(message), 0);	//�����������ֵ����ݰ�
	}
	else if (strcmp(pactype, REQ_LIST) == 0) {		//��������������б�����ݰ����г����е����ӷ������б�
		strcat(fmessage, RES_LIST);		//��־�б���Ӧ���ݰ�
		for (int i = 0; i < MAX_NUM; i++) {
			if (saClientlist[i].isalive == true) {		//����Ǵ��Ŀͻ��ˣ�����ͻ�����Ϣ
				sprintf(message, "%d", saClientlist[i].clientnumebr);
				strcat(message, "-");
				sprintf(newmsg, "%s", inet_ntoa(saClientlist[i].saClient.sin_addr));
				strcat(message, newmsg);
				strcat(message, "-");
				sprintf(newmsg, "%d\n", ntohs(saClientlist[i].saClient.sin_port));
				strcat(message, newmsg);
			}
		}
		sprintf(slength, "%03d", strlen(message));
		strcat(fmessage, slength);
		strcat(fmessage, message);
		ret = send(*sServer, (char *)&fmessage, strlen(fmessage), 0);
	}
	else if (strcmp(pactype, REQ_MSG) == 0) {		//�������Ϣת����������ݰ�����Ҫ�����ֶκ����ݣ�������ȡ�����ֶ�
		PAC_TRANSMESSAGE(sServer, (char *)&message, &dest);		//������sServer�׽��ֽ�����Ϣ������ŵ�message���棬���淢�ʹ�message,dest�б���Ŀ�Ŀͻ��˵ı��
		strcat(fmessage, COM_MSG);		//��־Ϊָʾ���ݰ�
		sprintf(slength, "%03d", strlen(message));		//3λ��ӡ�����㲹0
		strcat(fmessage, slength);		//�ַ������ӣ����ϳ�����
		sprintf(newmsg, "02d", dest);	//Ŀ��ͻ��˵ı��
		strcat(fmessage, newmsg);		//��������ֽ�
		strcat(fmessage, message);		//�������ݣ���ȫ���ݰ�

		if (saClientlist[dest].isalive == true) {
			ret = send(saClientlist[dest].sServer, (char *)&fmessage, strlen(fmessage), 0);		//�������ݰ���Ŀ������
			sprintf(message, "%s", "7014Send Succeed!");
			ret = send(*sServer, (char *)&message, strlen(message), 0);		//������Ϣ���ͻ��˱�ʾ���ͳɹ�
		}
		else {
			sprintf(message, "%s", "7013Send Failed!");
			ret = send(*sServer, (char *)&message, strlen(message), 0);		//������Ϣ���ͻ��˱�ʾ����ʧ��
		}
			
	}
	else {			//���ɶ�����ǣ����������ݰ���ʽ����

	}

	rflag = 0;
	return rflag;
}

int PAC_TRANSMESSAGE(SOCKET *sServer, char *ptr, int *dest) {
	int nLeft = PAC_CONTENT_LEN + 2;		//�����ֶ���λ������λ��ʾĿ�ĵ�ַ
	int ret;
	int rflag;
	char sig[6] = {0};		//��ȡ�ź���Ϣ
	char *sigptr = sig;

	//������һ��ѭ�������պ�����λ���ֱ��ǣ�ǰ��λ��ʾ���ݰ����ȣ�����λ��ʾĿ�ĵ�ַ
	while (nLeft > 0) {
		//��������
		ret = recv(*sServer, sigptr, nLeft, 0);
		if (ret == SOCKET_ERROR)
		{
			printf("recv failed!\n");
			rflag = PAC_ERROR;
			break;
		}

		if (ret == 0)
		{
			printf("client has closed the connection!\n");
			rflag = PAC_ERROR;
		}
		nLeft -= ret;
		sigptr += ret;
	}

//	if (!nLeft)		//�����ʽ�׽��ִ��������Ѿ����

	*dest = atoi(sig + 3);		//ȡĿ�Ŀͻ��˱��
	sig[3] = '\0';
	nLeft = atoi(sig);

	//�����ڶ���ѭ������ȡ��ת����Ϣ������
	while (nLeft > 0) {
		//��������
		ret = recv(*sServer, ptr, nLeft, 0);
		if (ret == SOCKET_ERROR)
		{
			printf("recv failed!\n");
			rflag = PAC_ERROR;
			break;
		}

		if (ret == 0)
		{
			printf("client has closed the connection!\n");
			rflag = PAC_ERROR;
		}
		nLeft -= ret;
		ptr += ret;
	}

	if (!nLeft)		//�����ʽ�׽��ִ��������Ѿ����
		rflag = PAC_RIGHT;		//ָʾ��ȷ���أ������ַ�����������message����

	return rflag;
}

void main()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret, length;		//ret���ո�����������ֵ
	SOCKET sListen, sServer;	//�����׽��ֺ������׽��֣������׽���Ҫ�������׽���������
	struct sockaddr_in saServer, saClient;		//�������Ϳͻ��˵ĵ�ַ��Ϣ
	char *ptr;	//����������
	const int THREAD_NUM = MAX_NUM;		//����������ӵĿͻ�����
	HANDLE handle[THREAD_NUM];		//���ؾ������
	int clientcount = 0;
	SOCKET clientlist[THREAD_NUM];		//������ٸ��߳̾��������ٸ�socket
	struct sockaddr_in clientaddr[THREAD_NUM];		//��¼�ͻ��˵�ַ��Ϣ���������ٸ��߳̾�������ڶ��ٸ��ͻ���

	//WinSock��ʼ��
	wVersionRequested = MAKEWORD(2, 2);	//ϣ��ʹ�õ�WinSock DLL�汾
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0)
	{
		printf("WSAStartup() failed!\n");
		return;
	}

	//ȷ��WinSock DLL֧�ְ汾2.2
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		printf("Invalid WinSock Version!\n");
		return;
	}

	//����socket��ʹ��TCPЭ�飬����֤�Ƿ���Ч�����޸�
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET)
	{
		WSACleanup();
		printf("socket() failed!\n");
		return;
	}

	//����������Ϣ
	saServer.sin_family = AF_INET;	//��ַ����
	saServer.sin_port = htons(SERVER_PORT);	//ת���������ֽ���
	saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY);	//ָʾ�����ַ

	//��
	ret = bind(sListen, (struct sockaddr*)&saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR)
	{
		printf("bind() failed! code:%d\n", WSAGetLastError());
		closesocket(sListen);
		WSACleanup();
		return;
	}

	//������������
	ret = listen(sListen, 20);		//���ӵȴ����г���Ϊ5����������������
	if (ret == SOCKET_ERROR)
	{
		printf("listen() failed! code:%d\n", WSAGetLastError());
		closesocket(sListen);
		WSACleanup();
		return;
	}

	//��ʾ���
	printf("Waiting for client connecting!\n");
	printf("tips : Ctrl+c to quit!\n");

	length = sizeof(saClient);
	
	//�����ȴ��ͻ�������
	while (true) {
		sServer = accept(sListen, (struct sockaddr *)&saClient, &length);
		if (sServer == INVALID_SOCKET) {
			printf("accept() failed! code:%d Please connect again!\n", WSAGetLastError());
			continue;		//�������ʧ�ܣ��������һ��ѭ���ȴ�
		}
		printf("Accepted Client: %s:%d\n", inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port));		//������ӳɹ�����ӡ���ӳɹ��Ŀͻ�����Ϣ
		handle[clientcount] = (HANDLE)_beginthreadex(NULL, 0, ThreadFun, &sServer, 0, NULL);		//�߳̾��
		saClientlist[clientcount].sServer = sServer;
		saClientlist[clientcount].clientnumebr = clientcount;	//client���
		saClientlist[clientcount].isalive = true;			//��־Ϊ����client
		saClientlist[clientcount].saClient = saClient;		//���client�ĵ�ַ�ṹ��Ϣ
		clientcount++;										//�ͻ��˼�����1
	}

	//���߳�Ҫ�ȴ������߳�ִ��������˳������ô˺����ȴ������߳�ִ����
	WaitForMultipleObjects(THREAD_NUM, handle, TRUE, INFINITE);		//�������߳�δ��ֹ���������ֹ���߳����ڽ���

	closesocket(sListen);		//�رռ����׽���
	closesocket(sServer);		//�ر������׽���
	WSACleanup();				//�ر�Winsock DLL
}