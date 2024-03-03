#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <iostream>
#include <cassert>
#include <thread>
#include <algorithm>

typedef unsigned long long ull;

int Recv(int sockfd);

int Send(int sockfd);


int main(int argc, char* argv[])
{

	if (argc != 3)
	{
		assert("Usage: server ip_address, port" && false);
	}

	struct sockaddr_in serv_addr;
	bzero((char*)&serv_addr, sizeof(struct sockaddr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("Socket error");
		return EXIT_FAILURE;
	}

	/*if (bind(sockfd, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(struct sockaddr*)) < 0)
	{
		perror("Bind error");
		return EXIT_FAILURE;
	}*/

/*
	struct addrinfo hints, *p, *res;
	memset(&hints, 0, sizeof(struct addrinfo)); // must have
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int getaddrinfo_error;
	if ( (getaddrinfo_error = ::getaddrinfo(argv[1], argv[2], &hints, &res)) != 0)
	{
		std::cout << "Return code getaddrinfo = " << getaddrinfo_error << std::endl;

		std::cerr << "Error getaddrinfo : " << 
			gai_strerror(getaddrinfo_error) << std::endl;
		
		return EXIT_FAILURE;
	}

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	int sockfd;

	int sockoptval = 1;
	for (p = res; p != NULL; p = p->ai_next)
	{
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd < 0)
			continue;

		
		::setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &sockoptval, sizeof(int));
	}

	if (p == NULL)
	{
		perror("Error note");
		std::cerr << "Not appropriate socket to bind. Terminating..." << std::endl;
		return EXIT_FAILURE;
	}

	freeaddrinfo(res);

*/

#ifdef _DEBUG
	{ // for 
		void* temp = operator new(INET_ADDRSTRLEN);
		
		if (inet_ntop(serv_addr.sin_family, &(serv_addr.sin_addr), 
			reinterpret_cast<char*>(temp), INET_ADDRSTRLEN) < 0)
		{
			perror ("inet_ntop error");
		} else 
			printf("Connection to the : %s\n", reinterpret_cast<char*>(temp));	
		
		operator delete(temp);
		temp = nullptr;
	}

#endif

	if (connect(sockfd, reinterpret_cast<sockaddr*>(&serv_addr),
		sizeof(serv_addr)) < 0)
	{
		perror("Connection to server error");
		close(sockfd);
		return EXIT_FAILURE;
	}
		
	std::thread receiver = std::thread(Recv, sockfd);
	std::thread sender = std::thread(Send, sockfd);
	
	receiver.join();
	sender.join();
	// create to thread or poll
	// lets do realize both way 
	perror("Terminate threads note");


	close(sockfd);
	return EXIT_SUCCESS;
}

typedef unsigned long long ull;

int Recv(int sockfd)
{
	fd_set master;
	FD_ZERO(&master);
	size_t kBUF_SIZE = 128;
	char buf[kBUF_SIZE];
	ull bytes_recv = 0;
	while (true)
	{
		FD_SET(sockfd, &master);
		if (select(sockfd + 1, &master, NULL, NULL, NULL) < 0) // wait until recv
		{
			return EXIT_FAILURE;
		} 

		if (FD_ISSET(sockfd, &master))
		{
			bytes_recv = recv(sockfd, buf, kBUF_SIZE, 0);

			if (bytes_recv < 0)
				return EXIT_FAILURE;
			else if (bytes_recv == 0)
			{
				return EXIT_SUCCESS;
			}
			buf[bytes_recv] = '\0';
			std::cout << buf << std::endl;
		}

	}

	return EXIT_SUCCESS;

}

int Send(int sockfd)
{
	std::cout << "What is your nickname? ";
	std::cout.flush();
	std::string msg;
	std::getline(std::cin, msg);

	ull bytes_send = 0;
	while (true)
	{
		bytes_send = send(sockfd, msg.c_str(), msg.length(), 0);

		if (bytes_send < 0)
			return EXIT_FAILURE;
		else if (bytes_send < msg.length())
		{
			ull add_bytes_send = 0;
			while (bytes_send < msg.length())
			{
				add_bytes_send = send(sockfd, msg.c_str() + bytes_send, 
					msg.length() - bytes_send, 0);

				if (add_bytes_send < 0)
					return EXIT_FAILURE;
				else if (add_bytes_send == 0)
					return EXIT_SUCCESS;

				bytes_send += add_bytes_send;
			}

		}

		bytes_send = 0;

		// if bytes_send < msg.length();

		std::getline(std::cin, msg);

		if (msg == "QUIT" || msg == "LEAVE")
		{
			// msg = "user:" + pseudo + "\ncommand:LEAVE";
			shutdown(sockfd, SHUT_RD);
			break;
		}
	}

	return EXIT_SUCCESS;
}

