#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <process.h> //线程函数库
#include <windows.h> //windows api库
#include <string.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib") //加载 ws2_32.dll，设置好之后就不用管项目属性了

#define SERVER_PORT 0614  //学号后四位当做监听端口
#define PAC_TYPE_LEN 1    //数据包类型字段长度为1
#define PAC_CONTENT_LEN 3 //数据包内容长度字段为3

#define PAC_ERROR 0
#define PAC_RIGHT 1

//请求数据包编号
#define REQ_TIME "0" //时间请求
#define REQ_NAME "1" //名字请求
#define REQ_LIST "2" //列表请求
#define REQ_MSG "3"  //消息请求
//响应数据包编号
#define RES_TIME "4" //时间响应
#define RES_NAME "5" //名字响应
#define RES_LIST "6" //列表响应
#define RES_MSG "7"  //消息响应
//指示数据包格式
#define COM_MSG "8" //消息指示

#define MAX_NUM 20 //最多允许存在20个连接数

int pacRecv(struct clientlist *pM, char *ptr, int length);        //子线程消息接收函数
int pacSend(struct clientlist *pM, char *pactype);                //子线程消息发送函数
int pacTransMessage(struct clientlist *pM, char *ptr, int *dest); //用于消息请求数据包的后半段内容接收
void getTime(char *timestring);                                   //获取服务器系统时间
unsigned int __stdcall exitCheck(PVOID pM);                       //结束信号检测线程
unsigned int __stdcall ThreadFun(PVOID pM);                       //响应客户端线程函数

bool exitsignal = false;		 //全局的进程退出信号
int clientcount = 0;			 //已创建线程数
struct clientlist
{
	struct sockaddr_in saClient; //客户端地址端口号等信息
	SOCKET sServer;              //服务端为该客户端创建的套接字
	int clientnumebr;            //客户端编号
	bool isalive = false;        //用户端是否存活
	bool prosignal = false;      //客户端结构体内部的退出信号，用来退出单个客户端
} saClientlist[MAX_NUM];

//定义线程函数
unsigned int __stdcall ThreadFun(PVOID pM)
{
	struct  clientlist* curClient = (struct clientlist*)pM;
	int ret; //接收消息返回值
	int nLeft = 0;
	char hlomsg[] = "9005Hello";		//9相当于是数据包标志位,005表示内容长度
	char message[1000] = "";
	char pactype[PAC_TYPE_LEN + 1];      //数据包类型字符串1位，最后一位\0标志字符串结束
	char paclength[PAC_CONTENT_LEN + 1]; //数据包内容长度字段3位，最后一位\0标志字符串结束
	pactype[PAC_TYPE_LEN] = '\0';        //提前写好字符串结束符
	paclength[PAC_CONTENT_LEN] = '\0';   //提前写好字符串结束符

	//说一下我要给对方客户端发送Hello消息了
	printf("Server say Hello to client: %s:%d\n", inet_ntoa((curClient->saClient.sin_addr)), ntohs((curClient->saClient).sin_port));
	printf("* * * * * * * * * * * * * * * * * * * * * * * *\n");

	//子线程发送一个Hello消息给客户端
	ret = send(((struct clientlist *)pM)->sServer, (char *)&hlomsg, sizeof(hlomsg), 0);
	if (ret == SOCKET_ERROR)
	{
		printf("Send() failed!\n"); //发送消息失败
	}

	while (true)
	{
		//如果收到当前线程退出的信号或收到全部线程退出信号
		if (curClient->prosignal == true || exitsignal == true)
		{                                       //如果主线程收到结束信号，则终止各个子线程，子线程用return方式终止，最为安全的方式
			curClient->isalive = false;
			closesocket(curClient->sServer);	//关闭连接套接字
			return -1;
		}

		ret = pacRecv(curClient, (char *)&pactype, PAC_TYPE_LEN); //指定消息接受函数去接受两个字符长度的数据包类型字段

		//当pacRecv函数没有检测到远端退出信号的时候再去发送数据包，不然会多发一次
		if (!(curClient->prosignal == true || exitsignal == true))
			pacSend((struct clientlist *)pM, pactype);
	}

	return 0;
}

//消息接收函数，第一个参数是连接套接字，第二个参数是字符指针，第三个参数指示要接收的消息类型，第四个参数表示待接收消息的长度
int pacRecv(struct clientlist *pM, char *ptr, int length)
{
	int nLeft = length; //读取类型字符串，只需要一个字符长度
	int ret;            //消息返回值
	int rflag;          //标志返回值

	while (nLeft > 0)
	{
		//接收数据
		ret = recv(pM->sServer, ptr, nLeft, 0);
		if (ret == SOCKET_ERROR || ret == 0)
		{
			pM->prosignal = true; //通知线程退出
			printf("Client Number:%d has closed the connection!\n", pM->clientnumebr);
			rflag = PAC_ERROR;
			break;
		}

		nLeft -= ret;
		ptr += ret;
	}

	if (!nLeft)            //如果流式套接字传送数据已经完成
		rflag = PAC_RIGHT; //指示正确返回，类型字符串被读到了message里面
	return rflag;
}

//消息发送函数，根据请求包的类型作出相应的动作
int pacSend(struct clientlist *pM, char *pactype)
{
	int rflag;       //指示发送是否成功
	int ret;         //测试返回值
	int length = 0;  //响应数据包长度字段也要发送出去，默认长度为0
	char slength[5]; //记录长度的字符串,长度为5个字节
	char message[1000] = { 0 };
	char newmsg[100] = { 0 };
	char fmessage[1050] = { 0 };
	int dest;
	char timestring[25] = { 0 }; //获取时间字段信息

	if (strcmp(pactype, REQ_TIME) == 0) //如果是请求时间的数据包，系统调用获取时间
	{
		getTime(timestring);
		strcat(message, RES_TIME);        //标志时间相应数据包
		length = strlen(timestring);      //内容长度
		sprintf(slength, "%03d", length); //长度转为三位字符串，不足则前面补0
		strcat(message, slength);
		strcat(message, timestring); //连接完成，得到封装好的数据包message
		
		printf("Server time: %s\n", timestring);
		printf("Client Number %d requestes system time on server!\n", pM->clientnumebr); //server上打印说哪个客户端请求了服务器上的系统时间
		ret = send(pM->sServer, (char *)&message, strlen(message), 0);                   //发送主机时间的数据包
		printf("* * * * * * * * * * * * * * * * * * * * * * * *\n");
	}
	else if (strcmp(pactype, REQ_NAME) == 0) //如果是请求服务器名字的数据包，系统调用获取名字
	{
		char hostname[100] = { 0 };
		if (gethostname(hostname, sizeof(hostname)) < 0)
		{
			//错误并返回
		}
		printf("Hostname on server:%s\n", hostname);
		length = strlen(hostname);                                                    //求hostname的长度
		sprintf(slength, "%03d", length);                                             //将长度转成三位字符串，不足前面补0
		strcat(message, RES_NAME);                                                    //添加响应数据包标志位
		strcat(message, slength);                                                     //添加响应数据包长度字段
		strcat(message, hostname);                                                    //添加响应数据包内容字段
		printf("Client Number %d requestes hostname on server!\n", pM->clientnumebr); //server上打印哪个客户端请求了服务器主机名
		printf("* * * * * * * * * * * * * * * * * * * * * * * *\n");
		ret = send(pM->sServer, (char *)&message, strlen(message), 0);                //发送主机名字的数据包
	}
	else if (strcmp(pactype, REQ_LIST) == 0)
	{                               //如果是请求连接列表的数据包，列出所有的连接服务器列表
		int ccount = 0;				//活着的主机数
		strcat(fmessage, RES_LIST); //标志列表响应数据包
		for (int i = 0; i < MAX_NUM; i++)
		{
			if (saClientlist[i].isalive == true)
			{ //如果是存活的客户端，打包客户端信息
				if (ccount == 0) {	//如果是第一个要打印的
					sprintf(newmsg, "%d-%s-%d", saClientlist[i].clientnumebr, inet_ntoa(saClientlist[i].saClient.sin_addr), ntohs(saClientlist
						[i].saClient.sin_port));
					strcat(message, newmsg);
				}
				else {	//如果前面已经有了一些字段
					sprintf(newmsg, "_%d-%s-%d", saClientlist[i].clientnumebr, inet_ntoa(saClientlist[i].saClient.sin_addr), ntohs(saClientlist
						[i].saClient.sin_port));
					strcat(message, newmsg);
				}
				ccount++;
			}
		}
		sprintf(slength, "%03d", strlen(message));
		strcat(fmessage, slength);
		strcat(fmessage, message);
		printf("Client Number %d requestes clientlist on server!\n", pM->clientnumebr); //server上打印说哪个客户端请求了连接服务器列表
		printf("* * * * * * * * * * * * * * * * * * * * * * * *\n");
		ret = send(pM->sServer, (char *)&fmessage, strlen(fmessage), 0);
	}
	else if (strcmp(pactype, REQ_MSG) == 0) //如果是消息转发请求的数据包，需要长度字段和内容，继续读取后续字段
	{
		getTime(timestring);
		length = strlen(timestring);                                        //内容长度
		pacTransMessage(pM, (char *)&message, &dest);                       //继续从sServer套接字接收消息，并存放到message里面并发送
		strcat(fmessage, COM_MSG);                                          //标志为指示数据包
		sprintf(slength, "%03d", strlen(message) + 4 + strlen(timestring)); //3位打印消息长度
		strcat(fmessage, slength);                                          //字符串连接，补上长度项
		sprintf(newmsg, "%02d", pM->clientnumebr);                          //源客户端的编号
		strcat(fmessage, newmsg);                                           //加上这个字节
		strcat(fmessage, "_");                                              //用下划线分隔
		strcat(fmessage, timestring);                                       //时间字段
		strcat(fmessage, "_");
		strcat(fmessage, message); //补上内容，补全数据包
		printf("Client number: %d say %s to client number: %d\n", pM->clientnumebr, message, dest);	//服务器上打印一下发送了什么消息

		if (saClientlist[dest].isalive == true)
		{
			ret = send(saClientlist[dest].sServer, (char *)&fmessage, strlen(fmessage), 0); //发送数据包到目的主机
			if (ret == SOCKET_ERROR)
				printf("Send failed!\n");
			sprintf(message, "%s", "7013Send Succeed!");
			ret = send(pM->sServer, (char *)&message, strlen(message), 0); //返回消息给客户端表示发送成功
			if (ret == SOCKET_ERROR)
				printf("Send failed!\n");
		}
		else
		{
			sprintf(message, "%s", "7012Send Failed!");
			ret = send(pM->sServer, (char *)&message, strlen(message), 0); //返回消息给客户端表示发送失败
		}
	}
	else //如果啥都不是，可能是数据包格式错误
		;
	rflag = 0;
	return rflag;
}

//读取转发请求数据包的后半段内容
int pacTransMessage(struct clientlist *pM, char *ptr, int *dest)
{
	int nLeft = PAC_CONTENT_LEN + 2; //长度字段三位加上两位表示目的地址
	int ret;
	int rflag;
	char sig[6] = { 0 }; //读取信号信息
	char *sigptr = sig;

	//开启第一轮循环，接收后面五位，分别是：前三位表示数据包长度，后两位表示目的地址
	while (nLeft > 0)
	{
		//接收数据
		ret = recv(pM->sServer, sigptr, nLeft, 0);
		//远端断开连接的信号
		if (ret == SOCKET_ERROR || ret == 0)
		{
			pM->prosignal = true;
			//printf("Recv failed!\n");
			printf("Client Number:%d has closed the connection!\n", pM->clientnumebr);
			rflag = PAC_ERROR;
			break;
		}

		nLeft -= ret;
		sigptr += ret;
	}

	*dest = atoi(sig + 3); //取目的客户端编号
	sig[3] = '\0';
	nLeft = atoi(sig);

	//开启第二轮循环，读取待转发消息的内容
	while (nLeft > 0)
	{
		//接收数据
		ret = recv(pM->sServer, ptr, nLeft, 0);
		if (ret == SOCKET_ERROR)
		{
			pM->prosignal = true;
			printf("Client Number:%d has closed the connection!\n", pM->clientnumebr);
			rflag = PAC_ERROR;
			break;
		}

		if (ret == 0)
		{
			pM->prosignal = true;
			printf("Client Number:%d has closed the connection!\n", pM->clientnumebr);
			rflag = PAC_ERROR;
		}
		nLeft -= ret;
		ptr += ret;
	}

	if (!nLeft)            //如果流式套接字传送数据已经完成
		rflag = PAC_RIGHT; //指示正确返回，类型字符串被读到了message里面

	return rflag;
}

//获取系统时间，传递指针，写入字符串内容
void getTime(char *timestring)
{
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
	strcat(timestring, "-"); //各个时间单位之间用一个'-'来分隔
	strcat(timestring, month);
	strcat(timestring, "-");
	strcat(timestring, day);
	strcat(timestring, "-");
	strcat(timestring, hour);
	strcat(timestring, "-");
	strcat(timestring, minute);
	strcat(timestring, "-");
	strcat(timestring, second);
	return;
}

//检测退出信号的线程，如果该线程检测到退出信号，通知各个线程结束以及主进程退出
unsigned int __stdcall exitCheck(PVOID pM)
{
	char flag;
	while (true)
	{
		scanf_s("%c", &flag);
		if (flag == 'q')
		{
			exitsignal = true; //通知全部线程结束
			return 0;
		}
	}
	return 0;
}

void main()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret, length;                       //ret接收各个函数返回值
	SOCKET sListen, sServer;               //侦听套接字和连接套接字，连接套接字要用侦听套接字来创建
	struct sockaddr_in saServer, saClient; //服务器和客户端的地址信息
	char *ptr;                             //参数传递用
	const int THREAD_NUM = MAX_NUM;        //最多允许连接的客户端数
	HANDLE handle[THREAD_NUM];             //返回句柄数组
	HANDLE exitprocess;                    //检测退出信号的线程
	int clientcount = 0;
	SOCKET clientlist[THREAD_NUM];             //允许多少个线程就允许开多少个socket
	struct sockaddr_in clientaddr[THREAD_NUM]; //记录客户端地址信息，允许开多少个线程就允许存在多少个客户端

	//WinSock初始化
	wVersionRequested = MAKEWORD(2, 2); //希望使用的WinSock DLL版本
	ret = WSAStartup(wVersionRequested, &wsaData);
	if (ret != 0)
	{
		printf("WSAStartup() failed!\n");
		return;
	}

	//确定WinSock DLL支持版本2.2
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		WSACleanup();
		printf("Invalid WinSock Version!\n");
		return;
	}

	//创建socket，使用TCP协议，并验证是否有效，待修改
	sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListen == INVALID_SOCKET)
	{
		WSACleanup();
		printf("socket() failed!\n");
		return;
	}

	//构建本地信息
	saServer.sin_family = AF_INET;                     //地址家族
	saServer.sin_port = htons(SERVER_PORT);            //转换成网络字节序
	saServer.sin_addr.S_un.S_addr = htonl(INADDR_ANY); //指示任意地址

	//绑定
	ret = bind(sListen, (struct sockaddr *)&saServer, sizeof(saServer));
	if (ret == SOCKET_ERROR)
	{
		printf("bind() failed! code:%d\n", WSAGetLastError());
		closesocket(sListen);
		WSACleanup();
		return;
	}

	//侦听连接请求
	ret = listen(sListen, MAX_NUM); //连接等待队列长度为20，可以往大了设置
	if (ret == SOCKET_ERROR)
	{
		printf("listen() failed! code:%d\n", WSAGetLastError());
		closesocket(sListen);
		WSACleanup();
		return;
	}

	//提示语句
	printf("* * * * * * * * * * * * * * * * * * * * * * * *\n");
	printf("Waiting for client connecting!\n");
	printf("tips : Ctrl+c to quit!\n");
	printf("* * * * * * * * * * * * * * * * * * * * * * * *\n");

	length = sizeof(saClient);

	//阻塞等待客户端连接
	while (true)
	{
		//如果总的线程退出信号为true，则跳出循环而不是阻塞在等待连接的地方
		if (exitsignal == true)
			break;
		sServer = accept(sListen, (struct sockaddr *)&saClient, &length);
		if (sServer == INVALID_SOCKET)
		{
			printf("accept() failed! code:%d Please connect again!\n", WSAGetLastError());
			continue; //如果连接失败，则进入下一轮循环等待
		}
		printf("Accepted Client: %s:%d\n", inet_ntoa(saClient.sin_addr), ntohs(saClient.sin_port)); //如果连接成功，打印连接成功的客户端信息
		saClientlist[clientcount].sServer = sServer;
		saClientlist[clientcount].clientnumebr = clientcount;                                                  //client编号
		saClientlist[clientcount].isalive = true;                                                              //标志为存活的client
		saClientlist[clientcount].saClient = saClient;                                                         //存放client的地址结构信息
		handle[clientcount] = (HANDLE)_beginthreadex(NULL, 0, ThreadFun, &saClientlist[clientcount], 0, NULL); //线程句柄
		clientcount++;                                                                                         //客户端计数加1
	}

	//主线程要等待其余线程执行完才能退出，调用此函数等待其余线程执行完
	WaitForMultipleObjects(THREAD_NUM, handle, TRUE, INFINITE); //避免在线程未终止的情况下终止掉线程所在进程

	closesocket(sListen); //关闭监听套接字
	closesocket(sServer); //关闭连接套接字
	WSACleanup();         //关闭Winsock DLL
}