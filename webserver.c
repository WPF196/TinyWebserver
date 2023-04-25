#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 1024
#define SMALL_BUF 100

void* request_handler(void* arg);
void send_data(FILE* fp, char* ct, char* file_name);
char* content_type(char* file);
void send_error(FILE* fp);
void error_handling(char* message);

int main(int argc, char *argv[]){
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_size;
	char buf[BUF_SIZE];
	pthread_t t_id;

	if(argc!=2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	
	serv_sock=socket(PF_INET, SOCK_STREAM, 0);
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr))==-1)
		error_handling("bind() error");
	if(listen(serv_sock, 20)==-1)
		error_handling("listen() error");

	while(1){
		clnt_adr_size=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_size);
		
		printf("Connection Request : %s:%d\n",
			inet_ntoa(clnt_adr.sin_addr), ntohs(clnt_adr.sin_port));

		// 使用多进程，实现并发处理多个客户端
		pthread_create(&t_id, NULL, request_handler, &clnt_sock);
		pthread_detach(t_id);
	}

	close(serv_sock);
	return 0;
}

void* request_handler(void* arg){
	int clnt_sock = *((int*)arg);
	char req_line[SMALL_BUF];
	FILE* clnt_read;
	FILE* clnt_write;

	char method[10];		//存客户端的请求方式
	char file_name[30];		//存储文件名称
	char ct[15];			//存储文件类型

	clnt_read = fdopen(clnt_sock, "r");
	clnt_write = fdopen(dup(clnt_sock), "w");	//生成通信套接字副本，实现I/O分离
	fgets(req_line, SMALL_BUF, clnt_read);		//从通信套接字中获取浏览器客户端的请求数据(主要是请求行)
	
	if(strstr(req_line, "HTTP/") == NULL){		//如果请求数据不是HTTP，直接返回错误信息
		send_error(clnt_write);					//向写指针发送错误信息
		fclose(clnt_read);
		fclose(clnt_write);
		return NULL;
	}

	strcpy(method, strtok(req_line, " /"));		//获取请求方式，如“GET”
	strcpy(file_name, strtok(NULL, " /"));		//获取文件名，如“index.html”
	strcpy(ct, content_type(file_name));		//获取文件类型，如“text/html”

	if(strcmp(method, "GET") != 0){				//如果不是GET，则返回错误
		send_error(clnt_write);
		fclose(clnt_read);
		fclose(clnt_write);
		return NULL; 
	}

	fclose(clnt_read);
	send_data(clnt_write, ct, file_name);		//将文件file_name发送到file写指针	
	//fclose(clnt_write);
}

// 将文件file_name发送给文件指针fp（写指针）
void send_data(FILE* fp, char* ct, char* file_name){
	char protocol[]="HTTP/1.0 200 OK\r\n";		//状态行
	char server[]="Server:Linux Web Server \r\n";
	char cnt_len[]="Content-length:2048\r\n";
	char cnt_type[SMALL_BUF];
	char buf[BUF_SIZE];
	FILE* send_file;
	
	sprintf(cnt_type, "Content-type:%s\r\n\r\n", ct);
	
    send_file = fopen(file_name, "r");			//打开目标文件
	if(send_file == NULL){						//不存在则报错返回
		send_error(fp);
		return;
	}

    // 返回http响应头部（状态行 + 消息头），浏览器将根据响应头部进行操作，如页面的渲染等
	fputs(protocol, fp);
	fputs(server, fp);
	fputs(cnt_len, fp);
	fputs(cnt_type, fp);

    // 将目标文件内容发到文件写指针 fp
	while(fgets(buf, BUF_SIZE, send_file) != NULL) {
		fputs(buf, fp);
		fflush(fp);
	}
	fflush(fp);
	fclose(fp);
}

// 获取并返回file的文件类型
char* content_type(char* file){
	char extension[SMALL_BUF];
	char file_name[SMALL_BUF];
	
	strcpy(file_name, file);
	strtok(file_name, ".");
	strcpy(extension, strtok(NULL, "."));

	if(!strcmp(extension, "html") || !strcmp(extension, "htm"))
		return "text/html";
	else 
		return "text/plain";
}

// 向文件指针fp发送错误信息
void send_error(FILE* fp){
	char protocol[] = "HTTP/1.0 400 Bad Request \r\n";
	char server[] = "Server:Linux Web Server \r\n";
	char cnt_len[] = "Content-length:2048 \r\n";
	char cnt_type[] = "Content-type:text/html \r\n\r\n";
	char content[]="<html><head><title>NETWORK</title></head>"
	       "<body><font size=+5><br> ERROR!"
		   "</font></body></html>";

	fputs(protocol, fp);
	fputs(server, fp);
	fputs(cnt_len, fp);
	fputs(cnt_type, fp);
	fflush(fp);
}

// 处理错误信息
void error_handling(char* message){
	fputs(message, stderr);
	fputc('\n', stderr);
}
