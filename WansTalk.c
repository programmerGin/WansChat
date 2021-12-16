
/// Linux, 교수님이 성함의 한글자 완을 따서 Wan's chat 이라고 이름 지었습니다.
// -pthread 
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <pthread.h>

#define MAX_BUFF 100
#define MAX_MESSAGE_LEN 256
#define NAME_MAX_SIZE 20

// Message 구조체를 정의하여 메세지 데이타의 기본단위로 만듬
typedef struct Message {  
	int user_id;
        char name[NAME_MAX_SIZE];	
	char str[MAX_MESSAGE_LEN];
}Message;

void *sendThread();
void *recvThread(void *data);
void *sendThreadClient();
int isFull();
int isEmpty();
int enqueue(Message item);
Message* dequeue();

int sock_main, sock_client[20],user_name[20];
Message *msgbuff;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int front = -1;
int rear = -1;

int main()
{
	int mode;
	int count = 0;
	int th_id;
	Message buff;
	pthread_t th_send;

	struct sockaddr_in addr;

	printf("1. Server, 2. Client: ");
	scanf("%d", &mode);
	getchar();

	if (mode == 1) { //Server 서버 선택 
		pthread_t th_recv[20]; 
		// 쳇 유저  몇명까지 수용할 것인지, 나는 20명으로 제한을 두겠음.
		
		msgbuff = (Message *)malloc(sizeof(Message) * MAX_BUFF);
		//메세지 버퍼 malloc으로 받음
		
		// Create Send Thread 보내는 쓰래드를 먼저 생성
		// 밑에 sendThread 함수에 자세히 설명
		th_id = pthread_create(&th_send, NULL, sendThread, 0);

		if (th_id < 0) { // 전송 쓰래드 구현 성공 여부 확인.
			printf("Send Thread Creation Failed\n");
			exit(1);
		}

		// 전송 쓰래드 구현 성공시 
		// 36007 adress 지정 및 포트 오픈 
		addr.sin_family = AF_INET;
		addr.sin_port = htons(36007);
		addr.sin_addr.s_addr = INADDR_ANY;

		// IPv4 TCP [1]소켓생성(create) 
		if ((sock_main = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			printf("Socket Open Failed\n");
			exit(1);
		}

		// [ 바인딩[2]결합 ] 서버에서 클라이언트 연결을 허용 하려면 시스템 내의 네트워크 IP 주소와 포트번호를 생성한 소켓에 바인딩되어야 함.
		if (bind(sock_main, (struct sockaddr*)&addr, sizeof(addr)) == -1) { 					     //Sockaddr 구조는 주소 패밀리, IP 주소 및 포트 번호에 대 한 정보를 저장 
			printf("Bind Failed\n");
			exit(2);
		}

		//[3]주시(listen)
		// listen 연결시 필요한 IP/PORT로 연결을 할때 받아주는 구실을 함
		if (listen(sock_main, 5) == -1) {
			printf("Listen Failed\n");
			exit(3);
		}

		//[4]받아들여(accept) 
		while (1) { 
			// accept and create client thread
			// 유저들이 접속할 때 마다 각 유저의 정보를 접속순으로 
			// sock_client[]배열에 저장함.
			if ((sock_client[count] = accept(sock_main, NULL, NULL)) == -1) {
				printf("Accept Failed\n");
				continue;
			}
			else { // 유저 20명 제한 설정으로 해놨으니
				if (count < 20) { //[5]송수신(send/recv)
					
					

					int idx = count;
// 서버는 새로운 유저가 들어오면 메세지를 받을 수 있는  리시브쓰래드를 만들어 놓고 다음 접속자를 기다림 
					 
					th_id = pthread_create(&th_recv[count], NULL, recvThread, (void *)&idx);
		    // 자신이 몇번째 유저인지 확인 
					if (th_id < 0) {
						printf("Receive Thread #%d Creation Failed\n", count + 1);
						exit(1);
					}

					count++;
				}
			}
		}
	}
	else { // 메뉴에서  Client 클라이언트 선택 

			
	 	 // socket 생성 및 주소 지정  127.0.0.1 36007
		addr.sin_family = AF_INET;
		addr.sin_port = htons(36007);
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");

		// 소켓 생성  확인  IPv4 TCP
		if ((sock_main = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			printf("Socket Open Failed\n");
			exit(1);
		}

		// Connect 정해진 주소로 서버와  연결 시도 
		if (connect(sock_main, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
			printf("Connect Failed\n");
			exit(4);
		}

		// Client Send Thread 정상적으로 연결 되었을 떄 쓰래드 하나 만들어서 보내는 쓰래드구현 
		th_id = pthread_create(&th_send, NULL, sendThreadClient, 0);

		if (th_id < 0) {
			printf("Send Thread Creation Failed\n");
			exit(1);
		}


		while (1) { 

			// 메인에서 메세지 받는것 처리  
			memset(&buff, 0, sizeof(buff));

			if (recv(sock_main, (char*)&buff, sizeof(buff), 0) > 0) {
				printf("FROM  %s: %s\n", buff.name, buff.str);
			}// 버퍼 통해서 유저아이디와 메세지 내용 받음 
			else {
				printf("Disconnected\n");
				exit(5);
			}
		}
	}

	return 0;
}


void *sendThread() { //Queue에 값이 들어오면 계속 실행됨 
	Message *tmp;

	printf("Send Thread Start\n");

	while (1) {
	// 유저가 보내는 메세지를 서버는  Queue에 담은 후 Dequeue를 이용해 담은 메세지 구조체에 저장된 정보를 가져와서 자신빼고 다른 유저들에게 전부 메세지안 문자열을 보낸다.
		if ((tmp = dequeue()) != NULL) {
			for (int i = 0; i < 20; i++) {//동시채팅 가능한 유저인원 지정한 만큼 for 문을 돌려주어 전체 유저에게 메세지를 보낼 수 있게함.
				if (i != tmp->user_id) { // 다만 자신의 메세지는 자신에게 보내지 않는다.
					send(sock_client[i], (char*)tmp, sizeof(Message), 0);
				}
			}
		}

		usleep(1000); // microsec 단위로1000/1000000 = 0.001 초 대기  
	}
}

void *recvThread(void *data) {
	Message buff;
	int thread_id = *((int*)data);

	printf("Receive Thread %d Start\n", thread_id);
	// 밑으로  enqueue 내용
	memset(&buff, 0, sizeof(Message));
	
	// 스래드 아이디를 이용해  sock_client 배열에서 정보를 가져와서 받은 내용을 버퍼에 넣음  
	while (recv(sock_client[thread_id], (char*)&buff, sizeof(buff), 0) > 0) {
		buff.user_id = thread_id; //메세지 하나 만들어서 
		if (enqueue(buff) == -1) { // 받은 내용을 큐에 밀어 넣음 
			printf("Messag Buffer Full\n"); //만약 buffer가 꽉차서  송신이 안되면 에러 알림.
		}
	}
}

void *sendThreadClient() {
	Message tmp; // Message 구조체를 정의하여 메세지 데이타의 기본단위로 만듬
	int count;

		printf("\n");

		printf("메세지 사용법:  [자신의 이름] 입력후 ENTER, 메세지 입력 ENTER   \n  ");


	while (1) {
		// �޽����� �Է� ���� �� ����
		memset(&tmp, 0, sizeof(tmp));
		//scanf("%[^\n]s", tmp.str);
		fgets(tmp.name,NAME_MAX_SIZE,stdin);
		tmp.name[strlen(tmp.name) -1] = '\0';

 
		fgets(tmp.str, MAX_MESSAGE_LEN, stdin);//사용자로부터 글자 받아서 
		tmp.str[strlen(tmp.str) - 1] = '\0'; //메세지 문자열
		tmp.user_id = -1; //유저아이디 
		count = send(sock_main, (char *)&tmp, sizeof(Message), 0);
	}
}

int isFull() {
	if ((front == rear + 1) || (front == 0 && rear == MAX_BUFF - 1)) {
		return 1;
	}
	return 0;
}

int isEmpty() {
	if (front == -1) {
		return 1;
	}
	return 0;
}
// 큐에 메세지 삽입 
int enqueue(Message item) {

	if (isFull()) {
		return -1;
	}
	else {
		pthread_mutex_lock(&mutex);//큐에서 메세지 삽입 하는 동안 다른 일이 방해하여 엉뚱한게 같이 삽입 되지  못하게 상호배제 함. 
		if (front == -1) {
			front = 0;
		}
		rear = (rear + 1) % MAX_BUFF;
		msgbuff[rear].user_id = item.user_id;
		strcpy(msgbuff[rear].name, item.name);
		strcpy(msgbuff[rear].str, item.str);
		pthread_mutex_unlock(&mutex);
		//위에서 상호배제 잠궈놨던것 락 풀음 
	}
	return 0;
}
//큐에서 메세지 꺼냄 
Message* dequeue() {
	Message *item;

	if (isEmpty()) {
		return NULL;
	}
	else {
		// 정보를 읽어올 때 엉뚱한게 같이 읽어오지 못하게 상호배제를 함.
		pthread_mutex_lock(&mutex);
		//위에서 상호배제 잠궈놨던것 락 풀음 
		item = &msgbuff[front];

		if (front == rear) {
			front = -1;
			rear = -1;
		}
		else {
			front = (front + 1) % MAX_BUFF;
		}
		pthread_mutex_unlock(&mutex);
		return item;
	}

}


