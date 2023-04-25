// c파일cpp로옮기기.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

// ConsoleApplication10.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS


#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <winsock2.h>
#include <windows.h>
#include<mswsock.h>
#include <ws2tcpip.h>
#include<cstdio>
#include<iostream>
#include"C:\Users\asus\OneDrive\바탕 화면\새 폴더 (6)\how_to_use_redis_lib\cpp\RedisCpp-hiredis\src\CRedisConn.h"
#include<hiredis.h>



#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"mswsock.lib")

#define BUF_SIZE 1024
#define READ	3
#define	WRITE	5
#define ACCEPT  7
#define START 1
#define SOCKET_POOL_SIZE 10

//Acceptex 관련
LPFN_ACCEPTEX lpfnAcceptEx = NULL;
GUID GuidAcceptEx = WSAID_ACCEPTEX;
WSAOVERLAPPED olOverlap;
DWORD dwBytes;
char lpOutputBuf[1024];
int outBufLen = 1024;
char buffer[1024];




typedef struct    // socket info
{
	SOCKET hClntSock;
	SOCKADDR_IN clntAdr;

} PER_HANDLE_DATA, * LPPER_HANDLE_DATA;

//소켓에서 데이터를 받은 후에 여기에있는 변수들에 어떻게 값을넣어주는지
typedef struct    // buffer info
{
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];
	SOCKET hClntSock;
	SOCKADDR_IN* clntAdr;
	RedisCpp::CRedisConn con;
	int rwMode;    // READ or WRITE
} PER_IO_DATA, * LPPER_IO_DATA;


#pragma comment(lib , "ws2_32.lib")
BOOL on = TRUE;
DWORD WINAPI EchoThreadMain(LPVOID CompletionPortIO);
void ErrorHandling(const char* message);

int main(int argc, char* argv[])
{
	WSADATA	wsaData;
	HANDLE hComPort;
	SYSTEM_INFO sysInfo;
	LPPER_IO_DATA ioInfo;
	LPPER_HANDLE_DATA handleInfo;

	SOCKET hServSock;
	SOCKADDR_IN servAdr;
	int recvBytes, i, flags = 0, result = 0;
	int nextSocketIndex = 0;
	RedisCpp::CRedisConn con;
	SOCKADDR_IN service;
	redisContext* key;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	SOCKADDR_IN clntAdr[SOCKET_POOL_SIZE];



	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	GetSystemInfo(&sysInfo);
	//남아있는 애들한태 IO넘길거니까 남아있는애들 포문으로 돌려주면서 걔네한태 뭐해야할지(EchoThreadMain) 넘겨주고 대기하라해.  
	// 요청오면연락할태니까 핸드폰줄게(hComPort) 이거스레드를 몇개를 줄지.
	for (i = 0; i < sysInfo.dwNumberOfProcessors; i++)
		CreateThread(NULL, 0, EchoThreadMain, hComPort, 0, NULL);



	SOCKET socketPool[SOCKET_POOL_SIZE];
	/*
	RedisCpp::CRedisConn RedisSocketPool[SOCKET_POOL_SIZE];
	for (int i = 0; i < 3; i++) {
		if (!RedisSocketPool[i].connect("127.0.0.1", 6379))
		{
			std::cout << "connect error " << RedisSocketPool[i].getErrorStr() << std::endl;
			return -1;
		}
		else
		{
			std::cout << "connect success !!!" << std::endl;
			key = RedisSocketPool[i].getConText();
			std::cout << "Redis 소켓풀의 소켓 : " << key->fd << std::endl;
		
		}

	}*/


	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	//if (setsockopt(hServSock, SOL_SOCKET, SO_CONDITIONAL_ACCEPT, (char*)&on, sizeof(on)))return -1;
	if (!con.connect("127.0.0.1", 6379))
	{
		std::cout << "connect error " << con.getErrorStr() << std::endl;
		return -1;
	}
	else
	{
		std::cout << "connect success !!!" << std::endl;

	}

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(8080);


	bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr));


	handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
	handleInfo->hClntSock = hServSock;
	handleInfo->clntAdr = servAdr;





	printf(" completion port 결과 : %d", CreateIoCompletionPort((HANDLE)hServSock, hComPort, (ULONG_PTR)handleInfo, 0));



	listen(hServSock, SOMAXCONN);
	printf("일단 리슨하고있어요 8080에서\n");





	int iResult = 0;
	//아래에 있는 accpetex 함수 호출을 위해 필요함
	iResult = WSAIoctl(hServSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&lpfnAcceptEx, sizeof(lpfnAcceptEx),
		&dwBytes, NULL, NULL);
	if (result == SOCKET_ERROR) {
		printf("WSAIoctl failed with error: %d\n", WSAGetLastError());
		closesocket(hServSock);
		WSACleanup();
		return 1;
	}
	int bytesReceived;

	// for 문 시작



	//  socket 풀 생성,
	for (int i = 0; i < 3; i++) {
		socketPool[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);/* WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);*/
		/*	iResult = setsockopt(hServSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
				(char*)&socketPool[i], sizeof(socketPool[i]));
			printf("setsocket opt 설정값 : %d", iResult);*/
			/*int resopt =  setsockopt(socketPool[i], SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)hServSock, sizeof(hServSock));*/
		handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		handleInfo->hClntSock = socketPool[i];

		printf("서버 소켓이 가져갈 소켓 : %d\n", handleInfo->hClntSock);
		printf(" completion port 결과 : %d", CreateIoCompletionPort((HANDLE)socketPool[i], hComPort, (ULONG_PTR)handleInfo, 0));
		printf("CompletionPort에 등록\n");






	};


	

	listen(hServSock, SOMAXCONN);
	printf("server 소켓 : %d\n", hServSock);

	for (int i = 0; i < 3; i++) {


		char buf[3];
		memset(buf, 0, sizeof(buf));

		ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
		memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
		ioInfo->wsaBuf.len = BUF_SIZE;
		ioInfo->wsaBuf.buf = ioInfo->buffer;
		ioInfo->hClntSock = socketPool[i];

		//acceptex
		ioInfo->rwMode = 10;
		lpfnAcceptEx(hServSock, socketPool[i], &(ioInfo->wsaBuf), 0,
			sizeof(SOCKADDR_IN) + 16,
			sizeof(SOCKADDR_IN) + 16,
			&dwBytes, &(ioInfo->overlapped));





	}
	// for 문 끝






	printf("서버 8080port에서 listen 중");


	while (1)
	{


	}
	return 0;
}

DWORD WINAPI EchoThreadMain(LPVOID pComPort)
{
	HANDLE hComPort = (HANDLE)pComPort;
	SOCKET sock;
	DWORD bytesTrans;
	LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;  //LPOVERLAPPED로 나중에바꿔야함 
	DWORD flags = 0;
	DWORD dwBytes;
	RedisCpp::CRedisConn con;
	//클라이언트 IP주소 얻기위한 변수 
	SOCKADDR_IN* tempUserInfo;
	
	int port; 

	sockaddr* localSockaddr;
	int localSockaddrLength;
	sockaddr* remoteSockaddr;
	int remoteSockaddrLength;

	//get요청받기위한 변수들
	char buf[2048];
	char method[100];
	char ct[100];
	char filename[100];
	struct sockaddr_in* addr_in = 0;






	while (1)
	{
		// 가끔 다시실행하면 콘솔창에 암것도안뜸; 끄고 다시실행하면되는데 왜그러냐 ㄹㅇ


		GetQueuedCompletionStatus(hComPort, &bytesTrans,
			(PULONG_PTR)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);
		printf("쓰레드  내부 GetQueuedCompletionStatus 후 \n");
		sock = handleInfo->hClntSock;
		printf("IO 발생소켓  : %d\n", sock);
		printf("mode : %d\n", ioInfo->rwMode);



		if (!con.connect("127.0.0.1", 6379))
		{
			std::cout << "connect error " << con.getErrorStr() << std::endl;
			return -1;
		}
		else
		{
			std::cout << "connect success !!!" << std::endl;

		}


		
		if (ioInfo->rwMode == READ)
		{
	
			printf("rwMode == Read 분기 시작");
			puts("message received!");
		/*	printf("전송받은 파일디스크럽터 : %d", handleInfo->hClntSock);*/




			if (bytesTrans == 0)    // EOF 전송 시
			{

				printf("EOF 전송받음!");
				closesocket(sock);
				free(handleInfo); free(ioInfo);
				continue;
			}
			/*	recv(sock, buf, 2048, 0);*/
				/*	printf("버퍼문 처음왔을때 %s : ", buf);*/
			if (strstr(ioInfo->buffer, "Rider") != NULL)
			{
				printf("Rider임");
				printf("Rider 위치정보 : %s", ioInfo->buffer);
				std::string temp = ioInfo->buffer;
				std::cout << temp << std::endl;
			}
			else if (strstr(ioInfo->buffer, "Monitor") != NULL) {
				printf("Monitor임");
				printf("Monitor 위치정보 : %s\n", ioInfo->buffer);

			}
			std::string redisResult="";
		
			char ipstr[INET_ADDRSTRLEN];
			std::string HgetResult;
			tempUserInfo = ioInfo->clntAdr;
			inet_ntop(AF_INET, &(ioInfo->clntAdr->sin_addr), ipstr, sizeof(ipstr));
		
			con.hget("Rider", ipstr, HgetResult);
			printf("레디스에서 가져온 값 : 내 IP 번호 : %s  내 포트 번호 : %s\n", ipstr, HgetResult.c_str());
			
	
			

			printf("reveived message (http요청시 ioInfo->buffer 뭐가오는지확인용  : %s ", ioInfo->wsaBuf.buf);
		
			
			
			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			memset(&(ioInfo->buffer), 0, sizeof(ioInfo->buffer));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;
			ioInfo->buffer[BUF_SIZE - 1] = '\0';
			ioInfo->rwMode = WRITE;
			ioInfo->hClntSock = sock;
			ioInfo->clntAdr = tempUserInfo;
			printf("문자열길이 : %ld", bytesTrans);
			//이거 wsaSend쓰도록바꿔야함
		//	send(sock, "{ \"IP\" : \"15.165.22.113\" , \"PORT\" : 3000 } \0", strlen("{ \"IP\" : \"15.165.22.113\" , \"PORT\" : 3000 } \0"), 0);
			/*memset(&(ioInfo->wsaBuf), 0, sizeof(ioInfo->wsaBuf));*/
			sprintf(ioInfo->buffer, "{ \"IP\" : \"15.165.22.113\" , \"PORT\" : 3000 } ");
			WSASend(sock, &(ioInfo->wsaBuf),
				1, NULL, 0, &(ioInfo->overlapped), NULL);


			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			/*memset(&(ioInfo->wsaBuf), 0, sizeof(ioInfo->wsaBuf));*/
			memset(&(ioInfo->buffer), 0, sizeof(ioInfo->buffer));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;
			ioInfo->buffer[BUF_SIZE - 1] = '\0';
			ioInfo->hClntSock = sock;
			ioInfo->clntAdr = tempUserInfo;
			WSARecv(sock, &(ioInfo->wsaBuf),
				1, NULL, &flags, &(ioInfo->overlapped), NULL);
		}
	
		else if (ioInfo->rwMode == WRITE)
		{
			puts("message sent!");
			
			memset(&(ioInfo->wsaBuf), 0, sizeof(ioInfo->wsaBuf));
			/*closesocket(sock);*/
			free(ioInfo);
		}

		else {
			printf("첫요청 소캣 :%d\n", handleInfo->hClntSock);
			printf("앞으로 요청받을 소켓: %d\n", ioInfo->hClntSock);





			/*iResult = WSAIoctl(hServSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&GuidAcceptEx, sizeof(GuidAcceptEx),
				&lpfnAcceptEx, sizeof(lpfnAcceptEx),
				&dwBytes, NULL, NULL);
			if (result == SOCKET_ERROR) {
				printf("WSAIoctl failed with error: %d\n", WSAGetLastError());
				closesocket(hServSock);
				WSACleanup();
				return 1;
			}*/

			/*	handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
				handleInfo->hClntSock = ioInfo->hClntSock;*/

			std::cout << "결과" <<  setsockopt(ioInfo->hClntSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&handleInfo->hClntSock, sizeof(SOCKET)) << std::endl;
			sock = ioInfo->hClntSock;



			LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = NULL;
			GUID GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
			WSAOVERLAPPED olOverlap;
			DWORD dwBytes;
			char lpOutputBuf[1024];
			int outBufLen = 1024;
			char buffer[1024];

			int iResult = WSAIoctl(ioInfo->hClntSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
				&GuidGetAcceptExSockaddrs, sizeof(GuidGetAcceptExSockaddrs),
				&lpfnGetAcceptExSockaddrs, sizeof(lpfnGetAcceptExSockaddrs),
				&dwBytes, NULL, NULL);

			if (iResult == SOCKET_ERROR) {
				printf("WSAIoctl failed with error: %d\n", WSAGetLastError());
				closesocket(ioInfo->hClntSock);
				WSACleanup();
				return 1;
			}

			lpfnGetAcceptExSockaddrs(
				&(ioInfo->wsaBuf),
				0,
				sizeof(SOCKADDR_IN) + 16,
				sizeof(SOCKADDR_IN) + 16,
				&localSockaddr,
				&localSockaddrLength,
				&remoteSockaddr,
				&remoteSockaddrLength
			);

			// 클라이언트 주소추출
			if (remoteSockaddr->sa_family == AF_INET) {
				//IPv4 address
				/*addr_in = (struct sockaddr_in*)remoteSockaddr;*/
				addr_in = (SOCKADDR_IN*)remoteSockaddr;
				char ipstr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(addr_in->sin_addr), ipstr, sizeof(ipstr));
				printf("IPv4 Remote address: %s:%d\n", ipstr, ntohs(addr_in->sin_port));

				uint32_t ret=0;
				/*ioInfo->con.connect("127.0.0.1", 6379);*/


				
				con.hset("Rider", ipstr, std::to_string(ntohs(addr_in->sin_port)), ret);
				printf("con set 끝났음 일단");
			}
			else if (remoteSockaddr->sa_family == AF_INET6) {
				//IPv6 address
				struct sockaddr_in6* addr_in6 = (struct sockaddr_in6*)remoteSockaddr;
				char ipstr[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, &(addr_in6->sin6_addr), ipstr, sizeof(ipstr));
				printf(" IPv6 Remote address: [%s]:%d\n", ipstr, ntohs(addr_in6->sin6_port));
			}
			else {
				//Unknown address family
				printf("Unknown address family\n");
			}
			//클라이언트 주소추출 끝
			
			/*std::cout << getpeername(handleInfo->hClntSock, (struct sockaddr*)&clintAdr, &clintAdrSize) << std::endl;*/
			/*std::cout << "에러이유 " <<   WSAGetLastError() << std::endl;
			std::cout << "getpeername 결과 : " << clintAdrSize << std::endl;*/

		
		


			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->rwMode = READ;
			ioInfo->hClntSock = sock;
			ioInfo->clntAdr = addr_in;
			printf("STart 끝 WSArecv 다시 요청 서버 소켓의 accpetex 끝!\n");

			//  클라이언트 IP주소 , PORT 추출 끝
			


			//앞으로 통신할 소켓(미리 할당한 소켓풀의 소켓중 하나) 등록
			handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
			handleInfo->hClntSock = sock;
			

		
		
		;
			
			CreateIoCompletionPort((HANDLE)(ioInfo->hClntSock), hComPort, (ULONG_PTR)handleInfo, 0);
			WSARecv(ioInfo->hClntSock, &(ioInfo->wsaBuf),
				1, NULL, &flags, &(ioInfo->overlapped), NULL);
			
		}
	}
	return 0;
}

void ErrorHandling(const char* message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}


// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁: 
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.


// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁: 
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
