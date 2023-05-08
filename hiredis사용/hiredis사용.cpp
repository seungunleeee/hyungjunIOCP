// hiredis사용.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//
// 
// 

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
// DisconnectEx처리 후 다시 AcceptEX()를 사용해서 소켓 재사용 가능하게 하기 주의 IOPENDING 체크
// 메모리 풀을 일단 구현해서 되는것 확인후 하나라도 적용시키고 생색내기 
// redis 그룹화 회사 

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <winsock2.h>
#include <windows.h>
#include<mswsock.h>
#include <ws2tcpip.h>
#include<cstdio>
#include<iostream>
//#include"C:\Users\asus\OneDrive\바탕 화면\새 폴더 (6)\how_to_use_redis_lib\cpp\RedisCpp-hiredis\src\CRedisConn.h"
#include<hiredis/hiredis.h>
#include <vector>
#include <sstream>
#include <list>
#include<algorithm>


#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"mswsock.lib")

#define BUF_SIZE 1024
#define READ	3
#define	WRITE	5
#define ACCEPT  7
#define START 1
#define SOCKET_POOL_SIZE 10
//라이더, 관리자 요청 파싱용
#define USERID 0
#define LOCATION 1
#define CLIENTPORTINFO 2
#define CLIENT_TCP_PORTINFO 3 
#define COMPANYINFO 3
#define CONDITION 3
//레디스에서 가져온 리턴값 파싱용
#define REDIS_IPnPORT_INFO 0
#define REDIS_LOCATION 1
#define REDIS_COMAPNY 2
// REDIS_IPnPORT_INFO에서 파싱된 값들
#define REDIS_IP 0
#define REDIS_TCPPORT 1
#define REDIS_UDPPORT 2




//Acceptex 관련
LPFN_ACCEPTEX lpfnAcceptEx = NULL;
GUID GuidAcceptEx = WSAID_ACCEPTEX;
WSAOVERLAPPED olOverlap;
DWORD dwBytes;
char lpOutputBuf[1024];
int outBufLen = 1024;
char buffer[1024];

struct ThreadParams {
	HANDLE hComPort;
	int index;
};


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
std::vector<std::string> split(std::string str, char Delimiter);
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

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	GetSystemInfo(&sysInfo);
	
	std::vector<ThreadParams> threadParams(sysInfo.dwNumberOfProcessors);
	std::cout << "내컴퓨터의 코어수 : " << sysInfo.dwNumberOfProcessors << std::endl;

	hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	
	
	HANDLE threadId1;
	for (i = 0; i < sysInfo.dwNumberOfProcessors; i++) {
		threadParams[i].hComPort = hComPort;
		threadParams[i].index = i;
		threadId1=	CreateThread(NULL, 0, EchoThreadMain, &threadParams[i], 0, NULL);
		std::cout << "Created thread 1 with ID: " << int(threadId1) << std::endl;
	}


	SOCKET socketPool[SOCKET_POOL_SIZE];



	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);


	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(8080);


	bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr));


	handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
	handleInfo->hClntSock = hServSock;
	handleInfo->clntAdr = servAdr;




	HANDLE hPort = CreateIoCompletionPort((HANDLE)hServSock, hComPort, (ULONG_PTR)handleInfo, 0);
	printf(" completion port 결과 : %d", hPort);



	int listenResult = listen(hServSock, SOMAXCONN);
	printf("일단 Listen 결과 %d\n", listenResult);

	int allaroundtheword = 0;



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


	// QOS 적용하기. 
	//  socket 풀 생성,
	for (int i = 0; i < 10; i++) {
		socketPool[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);/* WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);*/
		/*	iResult = setsockopt(hServSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
				(char*)&socketPool[i], sizeof(socketPool[i]));
			printf("setsocket opt 설정값 : %d", iResult);*/
			/*int resopt =  setsockopt(socketPool[i], SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)hServSock, sizeof(hServSock));*/
		handleInfo = (LPPER_HANDLE_DATA)malloc(sizeof(PER_HANDLE_DATA));
		handleInfo->hClntSock = socketPool[i];

		printf("서버 소켓이 가져갈 소켓 : %d\n", handleInfo->hClntSock);
		HANDLE childSocketResult = CreateIoCompletionPort((HANDLE)socketPool[i], hComPort, (ULONG_PTR)handleInfo, 0);
		printf(" completion port 결과 : %d", childSocketResult);







	};
	printf("CompletionPort에 등록 끝\n");



	listen(hServSock, SOMAXCONN);
	printf("server 소켓 : %d\n", hServSock);

	for (int i = 0; i < 10; i++) {


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
	int once = 0;

	while (1)
	{
		if (allaroundtheword != 0) {
			if(once==0)
			{
				std::cout << "main 함수 allaroundtheword 값 변경 : " << allaroundtheword << std::endl;
				once++;
			}

		}

	}
	return 0;
}

DWORD WINAPI EchoThreadMain(LPVOID lpParam)
{
	ThreadParams* params = static_cast<ThreadParams*>(lpParam);


	HANDLE hComPort = params->hComPort;
	int AssingedIndex = params->index;
	SOCKET sock;
	DWORD bytesTrans;
	LPPER_HANDLE_DATA handleInfo;
	LPPER_IO_DATA ioInfo;  //LPOVERLAPPED로 나중에바꿔야함 
	DWORD flags = 0;
	DWORD dwBytes;
	RedisCpp::CRedisConn con;
	//클라이언트 IP주소 얻기위한 변수 
	SOCKADDR_IN* tempUserInfo = 0;

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
	std::string result;
	std::vector<std::string> ParsedResult;
	int pushConditionNew = 0;
	int pushConditionChange = 0;


	uint64_t ret;
	std::list <std::string>::iterator iter;


	while (1)
	{



		GetQueuedCompletionStatus(hComPort, &bytesTrans,
			(PULONG_PTR)&handleInfo, (LPOVERLAPPED*)&ioInfo, INFINITE);
		printf("쓰레드  내부 GetQueuedCompletionStatus 후, push 할정보 저장할 배정받은 index : %d \n",AssingedIndex);
		sock = handleInfo->hClntSock;
		printf("IO 발생소켓  : %d\n", sock);




		if (!con.connect("127.0.0.1", 6379))
		{
			std::cout << "connect error " << con.getErrorStr() << std::endl;
			return -1;
		}
		else
		{
			std::cout << "connect success !!!" << std::endl;

		}


		// 요청 처리하는 분기문
		if (ioInfo->rwMode == READ)
		{

			printf("rwMode == Read 분기 시작");
			puts("message received!");

			if (bytesTrans == 0)    // EOF 전송 시
			{
				// 여기서 DisconnectEx() 함수를 이용하여 socket 재활용 처리해줘야함.
				// I/O pending 확인할것. 
				printf("EOF 전송받음!");
				closesocket(sock);
				free(handleInfo); free(ioInfo);
				continue;
			}



			std::cout << " 처음에 IO로 들어온값 : " << ioInfo->buffer << std::endl;
			//라이더 요청 처리
			if (strstr(ioInfo->buffer, "Rider") != NULL)
			{
				std::string RedisValue;
				printf("Rider임\n");
				std::string temp = ioInfo->buffer;
				std::cout << "Rider 위치정보 : " << temp << std::endl;
				temp.erase(0, temp.find(',') + 1);
				if (strstr(temp.c_str(), "Enrollment") != NULL) {
					temp.erase(0, temp.find(',') + 1);
					std::cout << "라이더의 Enrollment 요청 왔음 분기 성공" << std::endl;
					std::cout << "parsing 후 값 : " << temp << std::endl;
					ParsedResult = split(temp, '-');

					for (int i = 0; i < ParsedResult.size(); i++) {
						std::cout << "라이더의 요청 파싱결과 : " << ParsedResult[i] << std::endl;
					}
					std::vector<std::string> ParsedLocation = split(ParsedResult[LOCATION], '_');
					for (int i = 0; i < ParsedLocation.size(); i++) {
						std::cout << "라이더의 위치정보 파싱결과 : " << ParsedLocation[i] << std::endl;
					}
			/*		std::vector<std::string> ParsedCompanyInfo = split(ParsedResult[COMPANYINFO], '_');
					for (int i = 0; i < ParsedCompanyInfo.size(); i++) {
						std::cout << "라이더의 회사 정보 파싱결과 : " << ParsedCompanyInfo[i] << std::endl;
					}*/

					char ipstr[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &(ioInfo->clntAdr->sin_addr), ipstr, sizeof(ipstr));
					std::string tcpAddress = ipstr;
					std::string IPnPORT;
					IPnPORT.append(tcpAddress);
					IPnPORT.append("_");
					IPnPORT.append(std::to_string(ntohs(ioInfo->clntAdr->sin_port)));
					IPnPORT.append("_");
					IPnPORT.append(ParsedResult[CLIENTPORTINFO]);

					std::cout << "string 으로 바꾼 IP_TCPPORT_UDPPORT 정보" << IPnPORT << std::endl;
					
					RedisValue.append(IPnPORT);
					RedisValue.append("|");
					RedisValue.append(ParsedResult[LOCATION]);
					//RedisValue.append("|");
					//RedisValue.append(ParsedResult[COMPANYINFO]);
					std::cout << "Rider정보 최종적으로 Redis에 저장할 값 : " << RedisValue << std::endl;
					uint32_t ret32;



					//라이더 정보 조회 
					std::string oldRiderINFO; 
					
					con.get(ParsedResult[USERID], oldRiderINFO);
					std::cout << "레디스에서 쿼리한 이전 라이더 정보  : " << oldRiderINFO << std::endl;
					std::vector<std::string> ParsdOldResult = split(oldRiderINFO, '|');
					std::cout << "현재 라이더 정보 : " << RedisValue << std::endl;
					if(oldRiderINFO!="") {
						std::cout << "비교할 라이더의 현재정보 위치: " << ParsedResult[LOCATION] << "라이더의 이전 정보 위치 : " << ParsdOldResult[LOCATION] << std::endl;
						std::cout << " 라이더의 현재 위치 정보와 이전  위치 정보 비교" << ParsdOldResult[REDIS_LOCATION].compare(ParsedResult[LOCATION]) << std::endl;
						//std::cout << "비교할 라이더의 현재 근무 회사정보 : " << ParsedResult[COMPANYINFO] << "라이더의 이전 근무 회사정보 : " << ParsdOldResult[REDIS_COMAPNY] << std::endl;
						//std::cout << " 라이더의 이전 근무 회사 와 현재 근무회사 정보 비교" << ParsdOldResult[REDIS_COMAPNY].compare(ParsedResult[COMPANYINFO]) << std::endl;
					}
					// 라이더 정보가 없을 경우
					else {
						pushConditionNew = 1;
						std::cout << "처음 접속한 라이더, 레디스 에 정보저장" << std::endl;
						std::string	REDISKEY = "Rider";
						con.set(ParsedResult[USERID], RedisValue, ret32);
						std::cout << "set 결과 : " << ret32 << std::endl;
						for (int i = 0; i < ParsedLocation.size(); i++) {
							REDISKEY.append(ParsedLocation[i]);



							// RIDER 위치 정보 저장
							con.hset(REDISKEY, ParsedResult[USERID], RedisValue, ret32);
							std::cout << "레디스에 hset 성공여부 : " << ret32 << std::endl;
							std::cout << "Rider KEY에 저장한 필드 값  : " << REDISKEY << std::endl;
							if (i != ParsedLocation.size() - 1) {
								REDISKEY.append("_");
							}
						}
						//REDISKEY = "Rider";
						//for (int i = 0; i < ParsedCompanyInfo.size(); i++) {
						//	REDISKEY.append(ParsedCompanyInfo[i]);



						//	// RIDER 회사 정보 저장
						//	con.hset(REDISKEY, ParsedResult[USERID], RedisValue, ret32);
						//	std::cout << "레디스에 hset 성공여부 : " << ret32 << std::endl;
						//	std::cout << "Rider KEY에 저장한 필드 값  : " << REDISKEY << std::endl;
						//	if (i != ParsedLocation.size() - 1) {
						//		REDISKEY.append("_");
						//	}
						//}
						//std::cout << "레디스 KEY  : " << REDISKEY << std::endl;
					}
					
					// 라이더의 정보가 이전과 다를 경우 기존의 정보들을 삭제하고 새롭게 자료들을 저장합니다.
					if ((oldRiderINFO != "") && ((ParsdOldResult[REDIS_LOCATION].compare(ParsedResult[LOCATION]) != 0))) //같으면 0 아님 -1
					{
						pushConditionChange = 1;
						std::cout << "기존에 접속한 라이더 , 변경된 정보 레디스에 저장 시작" << std::endl;
						// 기존의 정보 레디스에서 삭제 
						std::string REDISKEY = "Rider";
						std::vector<std::string> oldInfoToDelete = split(ParsdOldResult[REDIS_LOCATION], '_');
						for (int i = 0; i < oldInfoToDelete.size(); i++) {
							std::cout << "ParsdOldResult 위치정보 Parse 결과  : " << oldInfoToDelete[i] << std::endl;
						}
						for (int i = 0; i < oldInfoToDelete.size(); i++) {
						
							if (i == 0) {
								REDISKEY.append(oldInfoToDelete[i]);
							}
							else {
								REDISKEY.append("_");
								REDISKEY.append(oldInfoToDelete[i]);
							}
							std::cout << "레디스에서 삭제할 자료의 필드 : " << REDISKEY << std::endl;
							con.hdel(REDISKEY, ParsedResult[USERID] ,ret32 );
							std::cout << "HDEL 결과 : " << ret32 << std::endl;
						}

						//REDISKEY = "Rider";
						//oldInfoToDelete = split(ParsdOldResult[REDIS_COMAPNY], '_');
						//for (int i = 0; i < oldInfoToDelete.size(); i++) {
						//	std::cout << "ParsdOldResult 회사정보 Parse 결과  : " << oldInfoToDelete[i] << std::endl;
						//}
						//for (int i = 0; i < oldInfoToDelete.size(); i++) {

						//	if (i == 0) {
						//		REDISKEY.append(oldInfoToDelete[i]);
						//	}
						//	else {
						//		REDISKEY.append("_");
						//		REDISKEY.append(oldInfoToDelete[i]);
						//	}
						//	std::cout << "레디스에서 삭제할 자료의 필드 : " << REDISKEY << std::endl;
						//	con.hdel(REDISKEY, ParsedResult[USERID], ret32);
						//	std::cout << "HDEL 결과 : " << ret32 << std::endl;
						//}
					
						


						// 기존의 정보 레디스에서 삭제  끝


						// 새로운 정보 레디스에 저장
						
						REDISKEY = "Rider";
						// RIDER  정보 저장
						con.set(ParsedResult[USERID], RedisValue, ret32);
						std::cout << "set 결과 : " << ret32 << std::endl;
						// RIDER  정보  위치별로 저장
						for (int i = 0; i < ParsedLocation.size(); i++) {
							REDISKEY.append(ParsedLocation[i]);



						
							con.hset(REDISKEY, ParsedResult[USERID], RedisValue, ret32);
							std::cout << "레디스에 hset 성공여부 : " << ret32 << std::endl;
							std::cout << "Rider KEY에 저장한 필드 값  : " << REDISKEY << std::endl;
							if (i != ParsedLocation.size() - 1) {
								REDISKEY.append("_");
							}
						}
						std::cout << "레디스 KEY  : " << REDISKEY << std::endl;


						// *회사별 저장하는 코드 나중에 
						//REDISKEY = "Rider";
					
						//// RIDER  정보  회사별로 저장
						//for (int i = 0; i < ParsedCompanyInfo.size(); i++) {
						//	REDISKEY.append(ParsedCompanyInfo[i]);



						//	// RIDER 정보 저장
						//	con.hset(REDISKEY, ParsedResult[USERID], RedisValue, ret32);
						//	std::cout << "레디스에 hset 성공여부 : " << ret32 << std::endl;
						//	std::cout << "Rider KEY에 저장한 필드 값  : " << REDISKEY << std::endl;
						//	if (i != ParsedLocation.size() - 1) {
						//		REDISKEY.append("_");
						//	}
						//}
						//std::cout << "레디스 KEY  : " << REDISKEY << std::endl;









						std::cout << "기존에 접속한 라이더 , 변경된 정보 레디스에 저장 끝" << std::endl;
						// 새로운 정보 레디스에 저장 끝
					}
					else{
						//이 else 문없에도 됨 걍 확인용
						std::cout << "기존에 접속한 라이더 , 변경된 정보 없음" << std::endl;
					}


					//라이더에게 전달할 관리자 정보   코드 시작
					std::string redisInstruction = "HVALS  Administrator";
					
					redisInstruction.append(ParsedResult[LOCATION]);
					std::cout << "라이더에게 줄 관리자 쿼리 문 : " << redisInstruction << std::endl;


					redisReply* reply = con.redisCmd(redisInstruction.c_str());
					/*std::vector<std::string> queryAnswer;*/
					std::string queryAnswer;
					if (reply->type == REDIS_REPLY_ARRAY) {
						 for (size_t i = 0; i < reply->elements; ++i) {
							std::cout << "위치기준 HVAL실행 Hash value: " << reply->element[i]->str << std::endl;
							/*queryAnswer.push_back(reply->element[i]->str);*/
							if (i != 0) {
								queryAnswer.append(" , ");
								queryAnswer.append(reply->element[i]->str);
							}
							else {
								queryAnswer.append(reply->element[i]->str);
							}
						}
					 } 

					// 나중에 set으로 바꾸기
					
					
					// 회사정보 기준으로 위치를 전송해줘야 하는 관리자들 쿼리
					/*redisInstruction = "HVALS  Administrator";*/
					//redisInstruction.append(ParsedResult[COMPANYINFO]);
					// reply = con.redisCmd(redisInstruction.c_str());
					// if (reply->type == REDIS_REPLY_ARRAY) {
					//	 for (size_t i = 0; i < reply->elements; ++i) {
					//		 std::cout << "회사기준 HVAL실행 Hash value: " << reply->element[i]->str << std::endl;
					//		 /*queryAnswer.push_back(reply->element[i]->str);*/
					//		 if (queryAnswer.size() != 0) {
					//			 queryAnswer.append(" , ");
					//			 queryAnswer.append(reply->element[i]->str);
					//		 }
					//		 else {
					//			 queryAnswer.append(reply->element[i]->str);
					//		 }
					//	 }
					//	 freeReplyObject(reply);
					// }
					//std::cout << "레디스에서 조회한 라이더가 위치를 전송할 관리자들의 정보 : " << queryAnswer << std::endl;
					//라이더에게 전달할 관리자 정보 코드 끝


					/*std::cout << "레디스에서 관리자 쿼리 결과 : " << redisResult << std::endl;*/


				/*	
				*	std::string ADMIN_INFO; 
					std::cout << "레디스에서 관리자 쿼리 결과 : " << redisResult << std::endl;
					con.get(redisResult, ADMIN_INFO);
					std::cout << "라이더에게 보낼 관리자 정보 : " << ADMIN_INFO << std::endl;*/




					ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
					memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
					memset(&(ioInfo->buffer), 0, sizeof(ioInfo->buffer));
					ioInfo->wsaBuf.len = BUF_SIZE;
					ioInfo->wsaBuf.buf = ioInfo->buffer;
					ioInfo->buffer[BUF_SIZE - 1] = '\0';
					ioInfo->rwMode = WRITE;
					ioInfo->hClntSock = sock;
					ioInfo->clntAdr = tempUserInfo;
					sprintf(ioInfo->buffer, "%s", queryAnswer.c_str());





					/*	sprintf(ioInfo->buffer, ADMIN_INFO.c_str());*/
					WSASend(sock, &(ioInfo->wsaBuf),
						1, NULL, 0, &(ioInfo->overlapped), NULL);

					// 일단 여기서 Push 진행
					if (pushConditionNew || pushConditionChange) {
						std::cout << "변경되서 PUSH진행 예정 "<< std::endl;



						if (reply->type == REDIS_REPLY_ARRAY) {
							for (size_t i = 0; i < reply->elements; ++i) {
								std::cout << "위치기준 Push할 관리자 정보 : " << reply->element[i]->str << std::endl;
								std::vector<std::string> AdminInfo = split(reply->element[i]->str, '|');
								std::vector<std::string> PushInfo = split(AdminInfo[0], '_');
								
								
								int result;
								SOCKET socket1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
								if (socket1 == INVALID_SOCKET) {
									std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
									WSACleanup();
									return 1;
								}

								sockaddr_in server_addr;
								server_addr.sin_family = AF_INET;
								server_addr.sin_port = htons(std::stoi(PushInfo[1]));
								inet_pton(AF_INET, PushInfo[0].c_str(), &server_addr.sin_addr);
								// 관리자 서버소켓에 연결 요청
								result = connect(socket1, (sockaddr*)&server_addr, sizeof(server_addr));
								if (result == SOCKET_ERROR) {
									std::cerr << "Connect failed: " << WSAGetLastError() << std::endl;
									closesocket(socket1);
									continue;
									
								}
								std::string message = reply->element[i]->str;
								result = send(socket1, message.c_str(), message.length(), 0);
								if (result == SOCKET_ERROR) {
									std::cerr << "Send failed: " << WSAGetLastError() << std::endl;
									closesocket(socket1);
									WSACleanup();
									return 1;
								}

								const int bufSize = 512;
								char buf[bufSize];
								int bytesReceived = recv(socket1, buf, bufSize, 0);
								if (bytesReceived == SOCKET_ERROR) {
									std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
									closesocket(socket1);
									WSACleanup();
									return 1;
								}
								std::string receivedData(buf, bytesReceived);
								std::cout << "Received from server: " << receivedData << std::endl;
								
				
								closesocket(socket1);


							}
						}


				
					
					}




					freeReplyObject(reply);


					ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
					memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
					memset(&(ioInfo->buffer), 0, sizeof(ioInfo->buffer));
					ioInfo->wsaBuf.len = BUF_SIZE;
					ioInfo->wsaBuf.buf = ioInfo->buffer;
					ioInfo->buffer[BUF_SIZE - 1] = '\0';
					ioInfo->rwMode = READ;
					ioInfo->hClntSock = sock;
					ioInfo->clntAdr = tempUserInfo;

					WSARecv(sock, &(ioInfo->wsaBuf),
						1, NULL, &flags, &(ioInfo->overlapped), NULL);
					std::cout << "----------------------------------------------라이더 요청처리 끝--------------------------------------------------" <<std::endl;
					continue;
				}



			}


			//관리자 요청 처리
			else if (strstr(ioInfo->buffer, "Administrator") != NULL) {
				std::string RedisValue;
				std::string temp = ioInfo->buffer;
				tempUserInfo = ioInfo->clntAdr;
				uint32_t ret32;
				temp.erase(0, temp.find(',') + 1);

				printf("Administrator 임\n");
				printf("Administrator 위치정보 : %s\n", temp.c_str());


				//요청분기
				if (strstr(temp.c_str(), "Enrollment") != NULL) {
					std::cout << "관리자의 Enrollment 요청  분기 성공" << std::endl;
					temp.erase(0, temp.find(',') + 1);
					std::cout << temp << std::endl;
					ParsedResult = split(temp, '-');

					for (int i = 0; i < ParsedResult.size(); i++) {
						std::cout << "관리자 요청 파싱결과 : " << ParsedResult[i] << std::endl;
					}

				}
			

				char ipstr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(ioInfo->clntAdr->sin_addr), ipstr, sizeof(ipstr));
				std::string ADMIN_IP = ipstr;
				std::string IPnPORT;
				// IP,TCP_PORT번호 , UDP_PORT번호 저장
				IPnPORT.append(ADMIN_IP);
				IPnPORT.append("_");
				IPnPORT.append(ParsedResult[CLIENT_TCP_PORTINFO]);
				IPnPORT.append("_");
				IPnPORT.append(ParsedResult[CLIENTPORTINFO]);

				RedisValue.append(IPnPORT);
				RedisValue.append("|");
				RedisValue.append(ParsedResult[LOCATION]);
				std::string oldAdminValue;
				con.get(ParsedResult[USERID], oldAdminValue);
				std::string REDISKEY = "Administrator";
				// 기존 조건 저장되있는 걸 삭제
				std::vector<std::string> oldParsedAdminCondition = split(oldAdminValue, '|');
				/*for (int i = 0; i < oldParsedAdminCondition.size(); i++) {*/
					std::cout << " 이전 조건들 : " << oldParsedAdminCondition[1] << std::endl;
					REDISKEY.append(oldParsedAdminCondition[1]);
					con.hdel(REDISKEY, ParsedResult[USERID]  , ret32);
		/*			REDISKEY = "Administrator";
				}*/
				// 기존 조건들 삭제 끝 -> 이거 문제될수 도 제대로 삭제되는지 확인필요


				REDISKEY = "Administrator";
				// Administartor 정보저장
				printf("Administartor 정보  저장할값 : %s\n", RedisValue.c_str());
			
				std::cout << " 이전 Administrator 정보들 : " << oldAdminValue << std::endl;
				con.set(ParsedResult[USERID], RedisValue, ret32);
				/*if(std::stoi(ParsedResult[CONDITION])==1)
				{*/
					
					REDISKEY.append(ParsedResult[LOCATION]);
					std::cout << "관리자 정보 저장 키 : " << REDISKEY << std::endl;
					con.hset(REDISKEY, ParsedResult[USERID], RedisValue, ret32); // 에러 처리 필요 , 
			/*	}*/
				// 조건 연산 일단 보류
				/*else {
					std::vector <std::string> contions = split(ParsedResult[LOCATION], '+');

					for (int i = 0; i < contions.size(); i++) {
							REDISKEY.append(contions[i]);
							std::cout << "레디스에 저장하는 키 " << REDISKEY << " 필드 " << ParsedResult[USERID] << " 벨류 " << RedisValue << std::endl;
							con.hset(REDISKEY, ParsedResult[USERID], RedisValue, ret32);
							REDISKEY = "Administrator";
					}*/

				/*}*/
			// Administartor 정보저장 끝
				
				

				std::string riderInfo;
				redisReply* reply;
				// 조건이 하나인 경우
			/*	if(std::stoi(ParsedResult[CONDITION])==1)
				{*/
					std::string query = "HVALS Rider";
					 reply = con.redisCmd(query.append(ParsedResult[LOCATION]).c_str());
				
					if (reply->type == REDIS_REPLY_ARRAY) {
						for (size_t i = 0; i < reply->elements; ++i) {
							std::cout << " HVAL 관리자에게 전달할 라이더 정보 조회 실행 결과  Hash value: " << reply->element[i]->str << std::endl;
							if (i != 0) {
								riderInfo.append(" , ");
								riderInfo.append(reply->element[i]->str);
							}
							else {
								riderInfo.append(reply->element[i]->str);
							}
						}
					}

					std::cout << "관리자에게 보내줄 라이더정보 쿼리 결과 : " << riderInfo << std::endl;
					freeReplyObject(reply);
			/*	}*/
				// 조건이 두개 이상인 경우 교집합연산   ,, or연산도고려해야함
				//else {
				//
				//	std::vector <std::string> contions = split(ParsedResult[LOCATION], '+');
				//	std::vector<std::vector<int>> v(contions.size());
				//	int iterForElement = 0;
				//	REDISKEY = "HKEYS   Rider";
				//	int length=0;
				//	for (int i = 0; i < contions.size(); i++) {
				//		REDISKEY.append(contions[i]);
				//		
				//		reply = con.redisCmd(REDISKEY.c_str());
				//		if (length < reply->elements) {
				//			length = reply->elements;
				//		}
				//		
				//		for (size_t j = 0; j < reply->elements; ++j) {
				//			std::cout << "Hash key "<< REDISKEY <<" :  "<< reply->element[j]->str << " int로 변환 후 출력 : " << std::stoi(reply->element[j]->str) << std::endl;
				//		
				//			v[i].push_back(std::stoi(reply->element[j]->str));
				//		}
				//		REDISKEY = "HKEYS   Rider";
				//		freeReplyObject(reply);
				//	}
				//	std::vector<int> buff(length);
				//	for (int i = 0; i < contions.size(); i++) {
				//		std::sort(v[i].begin(), v[i].end());
				//	}
				//	std::vector<int> intersectionKey;
				//	std::vector<int> current_intersection = v.front();
				//	for (size_t i = 1; i < v.size(); ++i) {
				//		std::vector<int> next_intersection;
				//		std::set_intersection(current_intersection.begin(), current_intersection.end(),
				//			v[i].begin(), v[i].end(), std::back_inserter(next_intersection));
				//		current_intersection = std::move(next_intersection);
				//		intersectionKey = current_intersection;
				//	}
				//	std::cout << "교집합 연산 이 후 출력 " << std::endl;
				//	std::string mget = "mget ";
				//	for (int i = 0; i < intersectionKey.size(); i++) {
				//		std::cout << "교집합 연산 후 원소 :" << intersectionKey[i] << std::endl;
				//		mget.append(std::to_string(intersectionKey[i]));
				//		mget.append(" ");
				//	}
				//	reply = con.redisCmd(mget.c_str());
				//	if (reply && reply->type == REDIS_REPLY_ARRAY) {
				//		for (size_t i = 0; i < reply->elements; ++i) {
				//			std::cout <<" 조건 검색한 유저정보들 : " << ": " << reply->element[i]->str << std::endl;
				//			if (i == 0) {
				//				riderInfo.append(reply->element[i]->str);
				//			}
				//			else {
				//				riderInfo.append(" , ");
				//				riderInfo.append(reply->element[i]->str);
				//			}
				//		}
				//	}
				//	else {
				//		std::cerr << "Error: MGET command failed.  " << std::endl;
				//	}


				//	//reply = (redisReply*)redisCommand(context, "HKEYS myhash");
				//	//if (reply->type == REDIS_REPLY_ARRAY) {
				//	//	for (size_t i = 0; i < reply->elements; ++i) {
				//	//		std::cout << "Hash key: " << reply->element[i]->str << std::endl;
				//	//	}
				//	//}
				//	//freeReplyObject(reply);
				//
				//
				//}






				ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
				memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
				memset(&(ioInfo->buffer), 0, sizeof(ioInfo->buffer));
				ioInfo->wsaBuf.len = BUF_SIZE;
				ioInfo->wsaBuf.buf = ioInfo->buffer;

				ioInfo->buffer[BUF_SIZE - 1] = '\0';
				ioInfo->rwMode = WRITE;
				ioInfo->hClntSock = sock;
				ioInfo->clntAdr = tempUserInfo;






				sprintf(ioInfo->buffer,"%s", riderInfo.c_str());
				WSASend(sock, &(ioInfo->wsaBuf),
					1, NULL, 0, &(ioInfo->overlapped), NULL);


				ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
				memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
				memset(&(ioInfo->buffer), 0, sizeof(ioInfo->buffer));
				ioInfo->wsaBuf.len = BUF_SIZE;
				ioInfo->wsaBuf.buf = ioInfo->buffer;
				ioInfo->rwMode = READ;
				ioInfo->buffer[BUF_SIZE - 1] = '\0';

				ioInfo->hClntSock = sock;
				ioInfo->clntAdr = tempUserInfo;


				WSARecv(sock, &(ioInfo->wsaBuf),
					1, NULL, &flags, &(ioInfo->overlapped), NULL);

				std::cout << "----------------------------------------------관리자 요청처리 끝--------------------------------------------------" << std::endl;
				continue;

			}


			char ipstr[INET_ADDRSTRLEN];
			std::string HgetResult;
			tempUserInfo = ioInfo->clntAdr;
			inet_ntop(AF_INET, &(ioInfo->clntAdr->sin_addr), ipstr, sizeof(ipstr));

			/*	con.hget("Rider", ipstr, HgetResult);
				printf("레디스에서 가져온 값 : 내 TCP프로토콜 IP 번호 : %s  내 포트 번호 : %s\n", ipstr, HgetResult.c_str());
				printf("reveived message (http요청시 ioInfo->buffer 뭐가오는지확인용  : %s ", ioInfo->wsaBuf.buf);*/

				//alive 체크하기 
				// 시간값 체크 시간 동기화 서버 시간이 맞는지도 몰름

			ioInfo = (LPPER_IO_DATA)malloc(sizeof(PER_IO_DATA));
			memset(&(ioInfo->overlapped), 0, sizeof(OVERLAPPED));
			memset(&(ioInfo->buffer), 0, sizeof(ioInfo->buffer));
			ioInfo->wsaBuf.len = BUF_SIZE;
			ioInfo->wsaBuf.buf = ioInfo->buffer;
			ioInfo->buffer[BUF_SIZE - 1] = '\0';
			ioInfo->rwMode = WRITE;
			ioInfo->hClntSock = sock;
			ioInfo->clntAdr = tempUserInfo;
			printf("문자열길이 : %ld", bytesTrans);
			//이거 wsaSend쓰도록바꿔야함

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
		// 응답 처리하는 분기문
		else if (ioInfo->rwMode == WRITE)
		{
			puts("message sent!");

			memset(&(ioInfo->wsaBuf), 0, sizeof(ioInfo->wsaBuf));
			/*closesocket(sock);*/
			free(ioInfo);
			continue;
		}
		// AcceptEx()요청시 처리하는 분기문. (AcceptEx분기 후 
		else {

			std::cout << "-----------------------------------------------------요청처리시작--------------------------------------------------" << std::endl;
			printf("첫요청 소캣 :%d\n", handleInfo->hClntSock);
			printf("앞으로 요청받을 소켓: %d\n", ioInfo->hClntSock);






			std::cout << "setsockopt 결과" << setsockopt(ioInfo->hClntSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&handleInfo->hClntSock, sizeof(SOCKET)) << std::endl;
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

				uint32_t ret = 0;
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



std::vector<std::string> split(std::string str, char Delimiter) {
	std::istringstream iss(str);             // istringstream에 str을 담는다.
	std::string buffer;                      // 구분자를 기준으로 절삭된 문자열이 담겨지는 버퍼

	std::vector<std::string> result;

	// istringstream은 istream을 상속받으므로 getline을 사용할 수 있다.
	while (getline(iss, buffer, Delimiter)) {
		result.push_back(buffer);               // 절삭된 문자열을 vector에 저장
	}

	return result;
};


























// 
//#include <iostream>
//#include <hiredis/hiredis.h>
//
//int main() {
//    // Redis 서버에 연결
//    redisContext* context = redisConnect("127.0.0.1", 6379);
//    if (context == NULL || context->err) {
//        if (context) {
//            std::cout << "Error: " << context->errstr << std::endl;
//            redisFree(context);
//
//
//
//
//
//           
//        }
//        else {
//            std::cout << "Can't allocate redis context." << std::endl;
//        }
//        return 1;
//    }
//
//    // SET 예제
//    std::cout << "예제 실행 " << std::endl;
//        // Redis 명령 실행: SADD
//        redisReply* reply = (redisReply*)redisCommand(context, "SADD myset value1");
//        freeReplyObject(reply);
//
//        // Redis 명령 실행: SADD
//        reply = (redisReply*)redisCommand(context, "SADD myset value2");
//        freeReplyObject(reply);
//
//        // Redis 명령 실행: SMEMBERS
//        reply = (redisReply*)redisCommand(context, "SMEMBERS myset");
//        if (reply->type == REDIS_REPLY_ARRAY) {
//            for (size_t i = 0; i < reply->elements; ++i) {
//                std::cout << "Set member: " << reply->element[i]->str << std::endl;
//            }
//        }
//        freeReplyObject(reply);
//    
//
//    // LIST 예제
//    
//        // Redis 명령 실행: LPUSH
//         reply = (redisReply*)redisCommand(context, "LPUSH mylist value1");
//        freeReplyObject(reply);
//
//        // Redis 명령 실행: LPUSH
//        reply = (redisReply*)redisCommand(context, "LPUSH mylist value2");
//        freeReplyObject(reply);
//
//        // Redis 명령 실행: LRANGE
//        reply = (redisReply*)redisCommand(context, "LRANGE mylist 0 -1");
//        if (reply->type == REDIS_REPLY_ARRAY) {
//            for (size_t i = 0; i < reply->elements; ++i) {
//                std::cout << "List element: " << reply->element[i]->str << std::endl;
//            }
//        }
//        freeReplyObject(reply);
//    
//
//    // HASH 예제
//    
//        // Redis 명령 실행: HSET
//         reply = (redisReply*)redisCommand(context, "HSET myhash field1 value1");
//        freeReplyObject(reply);
//
//        // Redis 명령 실행: HSET
//        reply = (redisReply*)redisCommand(context, "HSET myhash field2 value2");
//        freeReplyObject(reply);
//
//        // Redis 명령 실행: HGETALL
//        reply = (redisReply*)redisCommand(context, "HGETALL myhash");
//        if (reply->type == REDIS_REPLY_ARRAY) {
//            for (size_t i = 0; i < reply->elements; i += 2) {
//                std::cout << "Hash field: " << reply->element[i]->str
//                    << ", value: " << reply->element[i + 1]->str << std::endl;
//            }
//        }
//        freeReplyObject(reply);
//    
//
//
//
//
//
//
//
//
//
//
//
//
//
//         reply = (redisReply*)redisCommand(context, "SET mynumber 10");
//        freeReplyObject(reply);
//
//        // Redis 명령 실행: INCR
//        reply = (redisReply*)redisCommand(context, "INCR mynumber");
//        std::cout << "INCR result: " << reply->integer << std::endl;
//        freeReplyObject(reply);
//
//
//
//
//        // Redis 명령 실행: HMSET
//       reply = (redisReply*)redisCommand(context, "HMSET myhash field1 value1 field2 value2");
//        freeReplyObject(reply);
//
//        // Redis 명령 실행: HMGET
//        reply = (redisReply*)redisCommand(context, "HMGET myhash field1 field2");
//        if (reply->type == REDIS_REPLY_ARRAY) {
//            for (size_t i = 0; i < reply->elements; ++i) {
//                std::cout << "Hash value: " << reply->element[i]->str << std::endl;
//            }
//        }
//        freeReplyObject(reply);
//
//
//        // Redis 명령 실행: HVALS
//         reply = (redisReply*)redisCommand(context, "HVALS myhash");
//        if (reply->type == REDIS_REPLY_ARRAY) {
//            for (size_t i = 0; i < reply->elements; ++i) {
//                std::cout << " HVAL실행 Hash value: " << reply->element[i]->str << std::endl;
//            }
//        }
//        freeReplyObject(reply);
//
//
//
//        // Redis 명령 실행: HDEL
//     reply = (redisReply*)redisCommand(context, "HDEL myhash field1");
//        std::cout << "HDEL result: " << (reply->integer == 1 ? "success" : "failed") << std::endl;
//        freeReplyObject(reply);
//
//
//        reply = (redisReply*)redisCommand(context, "HGETALL myhash");
//        if (reply->type == REDIS_REPLY_ARRAY) {
//            for (size_t i = 0; i < reply->elements; i += 2) {
//                std::cout << "Hash field: " << reply->element[i]->str
//                    << ", value: " << reply->element[i + 1]->str << std::endl;
//            }
//        }
//        freeReplyObject(reply);
//
//
// /*       2. `INCR` 명령어를 사용하여 숫자 값을 증가시키고 결과를 출력합니다.
//            3. `HMSET`과 `HMGET` 명령어를 사용하여 해시에 여러 필드를 설정하고 값을 가져옵니다.
//            4. `HKEYS` 명령어를 사용하여 해시의 모든 키를 출력합니다.
//            5. `HVALS` 명령어를 사용하여 해시의 모든 값을 출력합니다.
//            6. `HDEL` 명령어를 사용하여 해시의 필드를 삭제하고, 삭제 후 해시의 모든 필드와 값을 출력합니다.*/
//
//    // 연결 종료
//    redisFree(context);
//
//    return 0;
//}

