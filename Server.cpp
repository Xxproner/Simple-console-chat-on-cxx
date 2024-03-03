#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>


#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <thread>
#include <forward_list>

#include "Server.hh"

namespace mnet_error
{
	int error_code = 0;
	constexpr size_t kBufSize = 64;
	void perror(const char* note)
	{
		char error_info[kBufSize];
		switch (error_code) 
		{
			case 0:
				strcpy(error_info, "Success");
				break;
			case 3: 
				strcpy(error_info, "Fork error");
				break;
			case 1: 
				strcpy(error_info, "");
				break;
			default :
				break;
		}

		printf("%s:%s", note, error_info);
	}
}


#define BUFFER_SIZE 1024

typedef unsigned long long ull;

void* Server::SocketWrapperUtils::get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) 
	{
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int Server::SocketWrapperUtils::set_nonblock(int socket)
{
	int flags;
	// save current flags
	flags = fcntl(socket, F_GETFL, 0);
	if (flags == -1)
		return EXIT_FAILURE;
	// set socket to be non-blocking
	if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}


void mnet_error::perror(const char* note);

Server::User::User(int sockfd, sockaddr_storage user_addr, const socklen_t size_addr, std::string pseudo) :
	sockfd_(sockfd), size_addr_(size_addr), pseudo_(std::move(pseudo))
{
	// user_addr_ = std::move(user_addr);
	memmove(&user_addr_, &user_addr, sizeof(sockaddr_in));
}


bool Server::User::operator ==(const User& that_user) const
{
	return this->sockfd_ == that_user.sockfd_;
}


Server::Server() : cliNo_(0), listener_(-1)
{
	memset(&serv_addr_, 0, sizeof(sockaddr_in));
	memset(port_, 0, sizeof(port_));
}


int Server::Setup(const char* port)
{
	memcpy(port_, port, strlen(port));

	struct addrinfo hints, *p, *res;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (::getaddrinfo(NULL, port_, &hints, &res) != 0)
	{
		return EXIT_FAILURE;
	}
	
	for(p = res; p != NULL; p = p->ai_next)
	{
		listener_ = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener_ < 0)
			continue;

		int sockoptval = 1;
		if (setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, &sockoptval, sizeof(int)) < 0)
		{
			return EXIT_FAILURE;
		}
		
		
		if (::bind(listener_, p->ai_addr, p->ai_addrlen) == 0)
		{
			memset(&serv_addr_, 0, sizeof(serv_addr_));
			memmove(&serv_addr_, p->ai_addr, p->ai_addrlen);
			break;
		}

		close(listener_);
	}

	if (!p)
	{
		return EXIT_FAILURE;
	}

	::freeaddrinfo(res);

#ifdef _DEBUG
	{
		void* temp = operator new(INET6_ADDRSTRLEN); // 60 bytes
		inet_ntop(serv_addr_.sin_family, &(serv_addr_.sin_addr), 
			reinterpret_cast<char*>(temp), INET6_ADDRSTRLEN);

		std::cout << "Server configuration : \n[";

		printf("ip address : %s\n", reinterpret_cast<char*>(temp));
		std::cout << std::setw(5) << "port : " << port << '\n' << 
			std::setw(5) << "address family : " << serv_addr_.sin_family << "]\n";
		std::cout.flush();

		operator delete(temp);
		temp = nullptr;
	}
#endif

	if (::listen(listener_, MAX_QUEUE_SIZE) < 0 )
	{
		return EXIT_FAILURE;
	}

	if (SocketWrapperUtils::set_nonblock(listener_) == EXIT_FAILURE)
	{
		close(listener_);
		return EXIT_FAILURE;
	}
	
	return EXIT_SUCCESS;
}


int Server::Ping(User& nextUser)
{
	for (const auto& user : connected_)
	{
		if (user.sockfd_ != nextUser.sockfd_ && user.pseudo_ == nextUser.pseudo_)
		{
			char buf[INET6_ADDRSTRLEN];
			inet_ntop(nextUser.user_addr_.ss_family, 
				SocketWrapperUtils::get_in_addr(reinterpret_cast<struct sockaddr*>(&nextUser.user_addr_)), 
					buf, INET6_ADDRSTRLEN);

			nextUser.pseudo_.push_back('@');
			nextUser.pseudo_.append(buf);

			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

int Server::DeleteUserbyid(int id)
{
	auto deleteUserIter = std::find_if(connected_.cbegin(), connected_.cend(), [id](const User& user){
		return user.sockfd_ == id;
	});


	if (deleteUserIter == connected_.cend())
	{
		SetServerReport("DeleteUserbyid logical error. Removing no exists id");
		return EXIT_FAILURE;
	}
	
	connected_.remove(*deleteUserIter);

	errno = 0;

	// Fork();
	return EXIT_SUCCESS;

}

int Server::FillFD_set(fd_set* master) const
{
	int FD_MAX = 0;
	FD_ZERO(master);

	for (const auto& user : connected_)
	{
		FD_SET(user.sockfd_, master);
		if (user.sockfd_ > FD_MAX)
			FD_MAX = user.sockfd_;
	}

	return FD_MAX;
}


int Server::SendAll(const char* msg, size_t size, std::ostream* out) // const possible exception : if user is disconnected
{
	*out << msg << "\n"; // BUILD message

	pid_t pid = fork();
	if (pid < 0)
	{
		SetServerReport("fork error. No possible create new process");
		return EXIT_FAILURE;

	} else if (pid == 0) // child 
	{ 
		int return_code = EXIT_SUCCESS;

		fd_set master;
		int FD_MAX = FillFD_set(&master);

		struct timeval timeoff; // std timeoff 
		timeoff.tv_sec = 1;
		timeoff.tv_usec = 0;

		int return_val_select; 
		if ((return_val_select = select(FD_MAX + 1, NULL, &master, NULL, &timeoff)) < 0)
			exit(EXIT_FAILURE);
		else if (return_val_select == 0) // no available writing
			exit(EXIT_SUCCESS);

		std::forward_list<std::thread> executing_thread; 
		for (int desc = 0; desc < FD_MAX + 1; ++desc)
		{
			if (FD_ISSET(desc, &master))
			{
				if (send(desc, msg, size, 0) <= 0)
				{
					std::thread delete_user_thread = std::thread(&Server::DeleteUserbyid, 
						this, desc);
					executing_thread.push_front(std::move(delete_user_thread));
				}
			}
		}

		for(auto& thread : executing_thread) // const or not?
		{
			if (thread.joinable())
				thread.join(); // either join method is not-const or detach
			else 
			{
				std::cerr << "WARNING : DETACH PRIORITY JOINABLE THREAD. UB\n";
				SetServerReport("Server::DeleteUserbyid thread DETACHED. DEFAULT JOINED");
				thread.detach();
			}
		}

		exit(return_code);
	} else { // parent
		int stat;
		wait(&stat);

		if (WIFEXITED(stat))
			// printf("%d", WEXITSTATUS(stat));
			return EXIT_SUCCESS;
		else
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}



int Server::StartUp()
{
	is_running_ = true;
	char buffer[BUFFER_SIZE];    
	fd_set master, cp_master; // master file descriptor list
	int FD_MAX; // maximum file descriptor number

	FD_ZERO(&master); // Clear the main monitor input port
	FD_ZERO(&cp_master);

	// add the listener to the main collection
	FD_SET(listener_, &master);
	// track the largest file defscriptor
	FD_MAX = listener_;

	// TODO : FIRST CONNECTION IS NONBLOCK ALWAYS

	ull bytes_recv;

	std::ofstream* notation  = new std::ofstream("notation.txt", std::ios_base::out | std::ios_base::trunc);
	if (!notation->is_open())
	{
		SetServerReport("The notation is not saved. Error open file");
		delete notation;
		notation = nullptr;
	}

	struct timeval timeoff = {.tv_sec = 3, .tv_usec = 0}; 
	while(is_running_)
	{
		cp_master = master; 
		// wait until at least one desc is ready
		
		if(select(FD_MAX + 1, &cp_master, NULL, NULL, &timeoff) == -1) // reconstruct select
		{
			return EXIT_FAILURE;
		}

		for (int i = 0; i < FD_MAX + 1; ++i)
		{
			if (FD_ISSET(i, &cp_master))
			{
				if (i == listener_)
				{
					struct sockaddr_storage cli_addr;
					socklen_t cli_addrlen ;
					// if (cliNo < 5)
					int newsockfd = accept(listener_, (struct sockaddr*)&cli_addr,
						&cli_addrlen);

					char pseudo[64];
					bytes_recv = recv(newsockfd, pseudo, 64, 0); // name

					if (bytes_recv > 0)
					{
						AddUser(newsockfd, std::move(cli_addr),
							cli_addrlen, pseudo);

						FD_SET(newsockfd, &master);
						SocketWrapperUtils::set_nonblock(newsockfd);

						if (newsockfd > FD_MAX)
							FD_MAX = newsockfd;	
						// set_nonblock()

					} else {
						close(newsockfd);
					}

				} else {

					// check connection 
					bytes_recv = recv(i, buffer, BUFFER_SIZE, 0);

					if (bytes_recv <= 0) // error or client shutdown connection
					{
						DeleteUserbyid(i);
						FD_CLR(i, &master);
						strncpy(buffer, "One of us left!", BUFFER_SIZE);
						bytes_recv = strlen("One of us left!");
					} else {
						std::for_each(std::begin(buffer), std::begin(buffer) + bytes_recv, putchar);
						std::cout.flush();
					}


					if (SendAll(buffer, bytes_recv, notation) == EXIT_FAILURE)
					{
						return EXIT_FAILURE;
					}

				}
			}

		}
	}

	notation->close();
	delete notation;

	return EXIT_SUCCESS;
}


Server::~Server()
{	

	for (const auto& user : connected_)
	{
		close(user.sockfd_);
	}

	close(listener_);
}

void Server::SetServerReport(const char* info)
{
	if (info_.max_size() - info_.length() < 
		std::char_traits<char>::length(info))
	{
		throw std::overflow_error("Info max size!");
	}

	info_.push_back('!');
	info_.append(info);
	info_.push_back('\n');

}

const char* Server::GetServerReport() const
{
	return info_.c_str();
}


