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


#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <thread>
#include <forward_list>
#include <type_traits>


#include "Server.hh"

#define BUFFER_SIZE 1024

typedef unsigned long long ull;
typedef std::char_traits<char> ch_traits;

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

int Server::SocketWrapperUtils::set_block(int socket)
{
	int flags;
	flags = fcntl(socket, F_GETFL, 0);
	if (flags == -1)
		return EXIT_FAILURE;
	if (fcntl(socket, F_SETFL, flags ^ O_NONBLOCK) == -1)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}


void mnet_error::perror(const char* note);

Server::User::User(int sockfd, sockaddr_storage user_addr, const socklen_t size_addr, std::string pseudo) :
	sockfd_(sockfd), channel_(0), size_addr_(size_addr), pseudo_(std::move(pseudo))
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
	struct stat sb;
	if (stat("files", &sb) != 0)
	{
		std::system("mkdir files");
	}

	parseFuncs_["/WHO"] = &Server::WhoRequest;
	parseFuncs_["/HELP"] = &Server::Help;
	parseFuncs_["/QUIT"] = &Server::Leave;
	parseFuncs_["/LEAVE"] = &Server::Leave;
	parseFuncs_["/CHANNEL"] = &Server::ChangeChannel;
	parseFuncs_["/PRIVATE"] = &Server::SendPrivateMessage;
	parseFuncs_["/FILE"] = &Server::FileRecv
	parseFuncs_["/FILE_LOAD"] = &Server::FileSend

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

std::forward_list<Server::User>::const_iterator Server::FindUserbyid(const int id) const // constexpr
{
	auto UserIter = std::find_if(connected_.cbegin(), connected_.cend(), [id](const User& user){
		return user.sockfd_ == id;
	});

	return UserIter;
}

std::forward_list<Server::User>::iterator Server::FindUserbyid(const int id) // constexpr
{
	auto UserIter = std::find_if(connected_.begin(), connected_.end(), [id](const User& user){
		return user.sockfd_ == id;
	});

	return UserIter;
}

std::forward_list<Server::User>::const_iterator Server::FindUserbyname(const std::string& name) const
{
	auto UserIter = std::find_if(connected_.cbegin(), connected_.cend(), [&name](const User& user){
		return user.pseudo_ == name;
	});

	return UserIter;
}

std::forward_list<Server::User>::iterator Server::FindUserbyname(const std::string& name)
{
	auto UserIter = std::find_if(connected_.begin(), connected_.end(), [&name](const User& user){
		return user.pseudo_ == name;
	});

	return UserIter;
}

std::string Server::DeleteUserbyid(int id)
{
	auto deleteUserIter = FindUserbyid(id);

	if (deleteUserIter == connected_.cend())
	{
		SetServerReport("DeleteUserbyid logic error. Removing no exists id");
		return "";
		// crack processing
	}
	

	close((*deleteUserIter).sockfd_);
	std::string delete_user_nickname = (*deleteUserIter).pseudo_;
	// connected_.remove(*deleteUserIter);
	connected_.remove(*deleteUserIter);

	mMutexAll_.erase(id);

	errno = 0;
	return delete_user_nickname;

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


int Server::SendAll(const char* msg, size_t size, int except_id) // const possible exception : if user is disconnected
{
	const auto& this_user = FindUserbyid(except_id);

	// size_t builtMsg_size = (*this_user).pseudo_.length() + ch_traits::length(msg) + 1;
	std::string builtMsg = (*this_user).pseudo_ + ": " + msg

	for(const auto& user : connected_)
	{
		if (user.sockfd_ != except_id && this_user.channel_ == user.channel_)
		{
			if (send(user.sockfd_, builtMsg, builtMsg.length(), MSG_NOSIGNAL) < 0)
			{
				queue_for_deleting.push_back(user.sockfd_);
			}
		}
	}

	return EXIT_SUCCESS;
}

int Server::AcceptNewConnection()
{
	struct sockaddr_storage cli_addr;
	socklen_t cli_addrlen ;
	int newsockfd;
	ull bytes_recv = 0;
	if (cliNo_ < 5)
	{
		// newsockfd = accept4(listener_, (struct sockaddr*)&cli_addr,
			// &cli_addrlen, SOCK_NONBLOCK);

		newsockfd = accept(listener_, (struct sockaddr*)&cli_addr,
			&cli_addrlen);

		if (newsockfd < 0)
		{
			if (newsockfd != EWOULDBLOCK) // accept is NONBLOCK and no connection is the queue;
			{
				std::string str = "Accept error. ";
				str.append(strerror(errno)); 
				SetServerReport(str.c_str());	
				return -1;
			}
			
			return -1;
		}

		char pseudo[64];
		bytes_recv = recv(newsockfd, pseudo, 64, 0); // should wait the name

		if (bytes_recv > 0)
		{
			pseudo[bytes_recv] = 0;
			AddUser(newsockfd, std::move(cli_addr),
				cli_addrlen, pseudo);
			++cliNo_;

		} else {
			SetServerReport("New connection is not valid");
			close(newsockfd);
			return -1;
		}
	} else 
	{
		return -1;
	}

	return newsockfd;
}


void Server::Wait()
{
	std::string admin_command;
	while (is_running_)
	{
		std::getline(std::cin, admin_command);

		if (admin_command == "/STOP" || admin_command == "/QUIT")
			is_running_ = false;

		else if (admin_command == "/WHO")
		{
			for (const auto& user : connected_)
			{
				std::cout << user.pseudo_ << '\n';
			}
		}

		std::cout.flush();
	}
}

int Server::StartUp()
{
	std::system("rm -rf materials/*");

	is_running_ = true;

	std::thread server_admin = std::thread(&Server::Wait, this);

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

	std::ofstream* notation  = new std::ofstream;
	notation.open("notation.txt", std::ios::out | std::ios::trunc);
	if (!notation->is_open())
	{
		std::cerr << "The notation is not saved. Error open file\n";
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

		int newsockfd = 0; 
		for (int i = 0; i < FD_MAX + 1; ++i)
		{
			if (FD_ISSET(i, &cp_master))
			{
				if (i == listener_)
				{
					newsockfd = AcceptNewConnection(); // non-block is set
					if (newsockfd > 0)
					{
						// SocketWrapperUtils::set_nonblock(newsockfd); // should we need it?
						FD_SET(newsockfd, &master);
					}
				} else {

					// check connection 
					// std::this_thread.sleep_for(std::chrono::seconds(1));
					bytes_recv = recv(i, buffer, BUFFER_SIZE, MSG_DONTWAIT); // MSG_DONTWAIT | MSG_WAITALL); // flag conflict?
					// check on integrity data

					if (bytes_recv < 0)
					{
						auto& problematic_user = *FindUserbyid(i);
						std::cerr << "Error recv data[ " << buffer << " ] to " <<
							"[ " << problematic_user.pseudo_ << " ] " << reinterpret_cast<char*>(
								SocketWrapperUtils::get_in_addr(reinterpret_cast<struct sockaddr*>(
									&problematic_user.user_addr_)));
						perror("");
						//proseccing error
					} else if (bytes_recv == 0) // error or client shutdown connection
						queue_for_deleting.push(i);
					else // when user disconnected buffer is empty 
					{
						buffer[bytes_recv] = 0;
						std::cout << "__"<< buffer << "__" << std::endl;
						ParseMessage(buffer, bytes_recv, i);	
					}
				}
			}
		}

		if (newsockfd > FD_MAX)
			FD_MAX = newsockfd;

		ClearQueueforDelete(&master, Server::deletePolicy::CHECKCON);
	}

	server_admin.detach();
	notation->close();
	delete notation;

	return EXIT_SUCCESS;
}


int Server::ParseMessage(const std::string& raw_msg, size_t size_msg, const int id) 
{
	if (raw_msg[0] == '/') // command
	{
		std::string commands;
		std::string params;

		size_t idx = raw_msg.find(' ');
		if (idx == std::string::npos)
		{
			commands = raw_msg;
			params = "";
		} else {
			commands = raw_msg.substr(0, idx);
			params = raw_msg.substr(idx + 1);
		}
			
		try 
		{
			int return_code = (this->*parseFuncs_.at(commands))(id, params);
		} catch (...)
		{
		 	commands += ": command not found. Use /HELP command for available ones!";
			if (send(id, commands.c_str(), commands.length(), MSG_NOSIGNAL) < 0) //when connection is aborted by
			{
				queue_for_deleting.push(id);
			}
		}	
	} else 
	
	{
		if (SendAll(raw_msg.c_str(), size_msg, id) == EXIT_FAILURE)
		{
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

int Server::ClearQueueforDelete(fd_set* master, Server::deletePolicy pPolicy)
{
	// id can be dublicate
	std::string leave_users_name;
	while (!queue_for_deleting.empty())
	{
		int id = queue_for_deleting.front();
		queue_for_deleting.pop();

		if (pPolicy == Server::deletePolicy::CHECKCON)
		{
			std::this_thread::sleep_for(std::chrono::seconds{1});
			char buf[2];
			if (recv(id, buf, 2, 0) != 0)
				continue;
		}

		leave_users_name += DeleteUserbyid(id);
		leave_users_name.push_back('\n');
		FD_CLR(id, master);

		
	}

	return EXIT_SUCCESS;
}


Server::~Server()
{	

	for (auto& user : connected_)
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




#include "commands.cpp"