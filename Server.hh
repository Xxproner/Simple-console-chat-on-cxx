#pragma once 

#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include <iostream>
#include <vector>
#include <forward_list>
#include <thread>

#define MAX_QUEUE_SIZE 5

namespace mnet_error
{
	extern int error_code;
	void perror(const char* note);
}



// template <typename char_type>
class Server
{
private:

	class SocketWrapperUtils
	{
	public:
		static int set_nonblock(int socket);
		static void* get_in_addr(struct sockaddr* sa);
	};

	struct User
	{
		int sockfd_; // accepted socket, dont needed user_addr_
		sockaddr_storage user_addr_;
		socklen_t size_addr_;
		std::string pseudo_;
		User(int sockfd, sockaddr_storage user_addr, const socklen_t, std::string pseudo);
		bool operator ==(const User& user) const;
	};

	struct MessageData
	{
		std::string user_;
		std::string command;
		std::string message;
		std::string channel;
	};
	
	int Ping(User& nextUser);

	constexpr static size_t kMaxUsers_ = MAX_QUEUE_SIZE;
	std::forward_list<struct User> connected_;
	
	size_t cliNo_;
	bool is_running_;
	sockaddr_in serv_addr_;
	char port_[6];
	int listener_;

	std::string info_;

	
	
public:

	Server();

	int Setup(const char* port);

	int StartUp();

	template <typename... Args>
	int AddUser(Args ...args);

	int SendAll(const char* msg, size_t size, std::ostream* out);

	int FillFD_set(fd_set* master) const;

	int DeleteUserbyid(int id);

	const char* GetServerReport( ) const;

	void SetServerReport(const char* info) ;

	~Server();
};



template <typename... Args>
int Server::AddUser(Args ...args)
{
	connected_.emplace_front(args...);

	// TODO: find nth argrument and 
	Ping(connected_.front());

	++cliNo_;

	return EXIT_SUCCESS;
}

// template <typename Allocator = std::allocator<char>>



