#include "client.h"

#include <cassert>

int main(int argc, char* argv[])
{

	if (argc != 3)
	{
		assert("Usage: server ip_address, port" && false);
	}


	Client client;

	client.SetServerConfig(argv[1], atoi(argv[2]));
	
	if (client.CreateSocket() == EXIT_FAILURE)
	{
		client.network_perror("SetServerConfig() method error");
		return EXIT_FAILURE;
	}

	client.ConnectServer();

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

	client.network_perror("Client work note");

	return EXIT_SUCCESS;
}