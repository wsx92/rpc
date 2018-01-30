#include<netinet/in.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<sys/sendfile.h>
#include<sys/stat.h>
#include<unistd.h>
#include<sys/types.h>
#include<fcntl.h>
#include<openssl/ssl.h>
#include<openssl/err.h>
#include<gflags/gflags.h>
#include<string>
#include<sstream>
#include<vector>
#include<sys/types.h>
#include<sys/wait.h>

using namespace std;

DEFINE_int32(port, 8080, "port");

vector<string> split(string str)
{
	string delim = ".";
	vector<string> result;
	size_t last = 0;
	size_t index = str.find_first_of(delim, last);
	while(index != string::npos)
	{
		result.push_back(str.substr(last, index - last));
		last = index + 1;
		index = str.find_first_of(delim, last);
	}
	if(index - last > 0)
	{
		result.push_back(str.substr(last, index - last));
	}
	return result;
}

string contenttype(const string& str)
{
	string result;
	if(str == "png")
	{
		return "image/png";
	}
	else if(str == "html" || str == "htm")
	{
		return "text/html";
	}
	else
	{
		return "application/octet-stream";
	}
}

string encodeurl(const string& str)
{
	char num[] = "0123456789ABCDEF";
	string result;
	for(size_t i = 0; i < str.size(); ++i)
	{
		unsigned char c = str[i];
		if(isascii(c))
		{
			if(c == ' ')
			{
				result += "%20";
			}
			else
			{
				result += c;
			}
		}
		else
		{
			unsigned char cc = str[i];
			result += "%";
			result += num[cc/16];
			result += num[cc%16];
		}
	}
	return result;
}

string decodeurl(const string& str)
{
	string result;
	int hex = 0;
	for(size_t i = 0; i < str.size(); ++i)
	{
		if(str[i] == '+')
		{
			result += ' ';
		}
		else if(str[i] == '%')
		{
			if(isxdigit(str[i+1]) && isxdigit(str[i+2]))
			{
				string sub = str.substr(i+1, 2);
				hex = strtol(sub.c_str(), 0, 16);
				result += char(hex);
				i += 2;
			}
			else
			{
				result += '%';
			}
		}
		else
		{
			result += str[i];
		}
	}
	return result;

}

void response_file(int connfd, int size, int status)
{
	char buf[128];
	sprintf(buf, "HTTP/1.1 %d OK\r\nConnection:Close\r\nContent-Type:text/html;charset=utf-8\r\ncontent-length:%d\r\n\r\n", status, size);
	write(connfd, buf, strlen(buf));
	printf("response_file\n%s", buf);
}

void response_file(SSL* ssl, int size, int status, string filename)
{
	char buf[128];
	//sprintf(buf, "HTTP/1.1 %d OK\r\nConnection:Close\r\nContent-Type:text/html;charset=utf-8\r\ncontent-length:%d\r\n\r\n", status, size);
	//SSL_write(ssl, buf, strlen(buf));
	//printf("response_file\n%s", buf);
	vector<string> vstr = split(filename);
	stringstream ss;
	ss << size;
	string str = "HTTP/1.1 200 OK\r\nConnection:Close\r\nContent-Type:" + contenttype(vstr[vstr.size() - 1]) +"\r\ncontent-length:" + ss.str() +"\r\n\r\n";
	SSL_write(ssl, str.c_str(), str.size());
	printf("response_file\n%s", str.c_str());
}

void response_get(int connfd, string& name)
{
	char file[100];
	strcpy(file, "/root/index.html");
	struct stat filestat;
	int ret = stat(file, &filestat);
	if(ret == 0 && !S_ISDIR(filestat.st_mode))
	{
		int filefd = open(file, O_RDONLY);
		response_file(connfd, filestat.st_size, 200);
		sendfile(connfd, filefd, 0, filestat.st_size);
	}
}

void response_get(SSL* ssl, string& name)
{
	string dir = "/root";
	dir += name;
	struct stat filestat;
	int ret = stat(dir.c_str(), &filestat);
	if(ret == 0 && !S_ISDIR(filestat.st_mode))
	{
		int filefd = open(dir.c_str(), O_RDONLY);
		//char buff[] = "<html>\n<head>\n<title>index</title>\n</head>\n<body>hello</body>\n</html>\n";
		response_file(ssl, filestat.st_size, 200, name);
		
		char buff[2048];
		int size;
		while((size = read(filefd, buff, sizeof(buff))) != 0)
		{
			SSL_write(ssl, buff, size);
		}
		//string result = encode(str);
		//response_file(ssl, filestat.st_size, 200);
		//sendfile(connfd, filefd, 0, filestat.st_size);
	}
}

void response(int connfd)
{
	char buffer[4096];
	int size = read(connfd, buffer, sizeof(buffer) - 1);

	printf("content:\n%s", buffer);
	if(size > 0)
	{
		char method[5];
		char filename[50];

		int i = 0, j = 0;
		while(buffer[j] != ' ' && buffer[j] != '\0')
		{
			method[i++] = buffer[j++];
		}
		++j;
		method[i] = '\0';
		i = 0;
		while(buffer[j] != ' ' && buffer[j] != '\0')
		{
			filename[i++] = buffer[j++];
		}
		filename[i] = '\0';
		string name_str(filename);
		if(strcasecmp(method, "get") == 0)
		{
			response_get(connfd, name_str);
		}
	}

}

void response_post(SSL* ssl, string& name)
{
	string dir = "/root";
	dir += name;

	int fds_i[2];
	int fds_o[2];
	if(pipe(fds_i)) return;
	if(pipe(fds_o)) return;


	if(fork() == 0)
	{
		if(fork() == 0)
		{
			close(fds_i[1]);
			close(fds_o[0]);
			close(0);
			close(1);
			dup2(fds_i[0], 0);
			dup2(fds_o[1], 1);
			execl(dir.c_str(), dir.c_str(), NULL);
			fprintf(stderr,"execl error\n");
			_exit(0);

		}
		else
		{
			_exit(0);
		}
	}
	close(fds_i[0]);
	close(fds_o[1]);
	wait(NULL);
	

	string str = "HTTP/1.1 200 OK\r\nConnection:Close\r\nContent-Type:text/html\r\n\r\n";
	SSL_write(ssl, str.c_str(), str.size());
	//FILE* file = fdopen(fds_o[0], "r");
	char buf[2048];
	//while(fgets(buf, sizeof(buf), file) != NULL)
	//{
	//	printf("action:\nsize:%d\n%s\n", strlen(buf), buf);
	//	SSL_write(ssl, buf, strlen(buf));
	//}
	int size;
	while((size = read(fds_o[0], buf, sizeof(buf))) != 0)
	{
		printf("action:\nsize:%d\n%s\n", size, buf);
		SSL_write(ssl, buf, size);
	}
}

void response(SSL* ssl)
{
	char buffer[4096];
	int size = SSL_read(ssl, buffer, sizeof(buffer) - 1);

	printf("content:\n%s\n", buffer);
	if(size > 0)
	{
		char method[5];
		char filename[50];

		int i = 0, j = 0;
		while(buffer[j] != ' ' && buffer[j] != '\0')
		{
			method[i++] = buffer[j++];
		}
		++j;
		method[i] = '\0';
		i = 0;
		while(buffer[j] != ' ' && buffer[j] != '\0')
		{
			filename[i++] = buffer[j++];
		}
		filename[i] = '\0';
		string name_str(filename);
		if(strcasecmp(method, "get") == 0)
		{
			response_get(ssl, name_str);
		}
		else if(strcasecmp(method, "post") == 0)
		{
			response_post(ssl, name_str);
		}
	}

}

int main(int argc, char* argv[])
{
	gflags::ParseCommandLineFlags(&argc, &argv, true);

	struct sockaddr_in server_addr, client;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(FLAGS_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0)
	{
		return -1;
	}

	int ret = bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

	if(ret < 0)
	{
		return -1;
	}

	ret = listen(sockfd, 10);

	if(ret < 0)
	{
		return -1;
	}

	SSL_library_init();
	SSL_load_error_strings();
	SSL_CTX* ctx = SSL_CTX_new(TLSv1_2_server_method());
	
	if(!SSL_CTX_use_certificate_file(ctx, "sslservercert.pem", SSL_FILETYPE_PEM))
	{
		return -1;
	}
	if(!SSL_CTX_use_PrivateKey_file(ctx, "sslserverkey.pem", SSL_FILETYPE_PEM))
	{
		return -1;
	}
	if(!SSL_CTX_check_private_key(ctx))
	{
		return -1;
	}

	while(1)
	{
		socklen_t len = sizeof(client);
		int connfd = accept(sockfd, (struct sockaddr*)&client, &len);
		//printf("server: got connection\n");

		if(connfd == -1)
		{
			printf("accpet error\n");
			continue;
		}
		SSL* ssl = SSL_new(ctx);
		SSL_set_fd(ssl, connfd);
		if(SSL_accept(ssl) == -1)
		{
			printf("ssl_accept error\n");
			continue;
		}

		response(ssl);
		//response(connfd);
		SSL_shutdown(ssl);
		SSL_free(ssl);
		close(connfd);
	}		
}
