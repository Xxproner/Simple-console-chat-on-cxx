#pragma once 

#include <unistd.h>
#include <netinet/in.h>
#include <string.h>

#include <queue>
#include <map>
#include <iostream>
#include <vector>
#include <forward_list>
#include <thread>
#include <chrono>

#ifndef
	#define DELAY_F_CONN(x) std::this_thread::sleep_for(std::chrono::seconds{(x)});
#endif

class Server
{
private:
	constexpr static size_t MAX_QUEUE_SIZE = 5;
	typedef int (Server::*Function) (const int, const std::string& );
	typedef std::map<std::string, Function> mapFunctions;
	mapFunctions parseFuncs_;

	// mutable std::map<int, std::mutex> mMutexAll_;

	class SocketWrapperUtils
	{
	public:
		static int set_nonblock(int socket);
		static void* get_in_addr(struct sockaddr* sa);
		static int set_block(int socket);
	};

	struct User
	{
		int sockfd_; // accepted socket, dont needed user_addr_
		int channel_; 
		sockaddr_storage user_addr_;
		socklen_t size_addr_;
		std::string pseudo_;
		User(int sockfd, sockaddr_storage user_addr, const socklen_t, std::string pseudo);
		bool operator ==(const User& user) const;
	};

	int Ping(User& nextUser);
	void Wait();

	constexpr static size_t kMaxUsers_ = MAX_QUEUE_SIZE;
	std::forward_list<struct User> connected_;
	
	size_t cliNo_;
	bool is_running_;
	sockaddr_in serv_addr_;
	char port_[6];
	int listener_; // for chatting only

	enum class deletePolicy
	{
		ANYWAY, // delete user anyway
		CHECKCON // check connection again 
	};

	std::queue<int> queue_for_deleting;
	
	int ClearQueueforDelete(fd_set* master, Server::deletePolicy pPolicy);

	std::string info_;

	template <typename... Args>
	int AddUser(Args ...args);

	int ParseMessage(const std::string& raw_msg, size_t size_msg, const int id) ;

	int SendAll(const char* msg, size_t size, int id = -1);

	int FillFD_set(fd_set* master) const;

	std::forward_list<User>::const_iterator FindUserbyid(const int id) const;

	std::forward_list<User>::iterator FindUserbyid(const int id);

	std::forward_list<User>::const_iterator FindUserbyname(const std::string& name) const;

	std::forward_list<User>::iterator FindUserbyname(const std::string& name);

	std::string DeleteUserbyid(int id);

	const char* GetServerReport( ) const;

	int AcceptNewConnection(); 

	void SetServerReport(const char* info) ;

	int WhoRequest(const int id, const std::string&);

	int Leave(const int id, const std::string&);

	int Help(const int id, const std::string&);

	int ChangeChannel(const int id, const std::string& params);

	int SendPrivateMessage(const int id, const std::string& params);

	void PossibleCommandsInfo(std::ostream& out = std::cout) const;

	int EstablishNewConnection(const std::string& cmdFILE_file_name,
		FILE_STATUS STATUS) const;

	int FileRecv(std::string file_name,
		int sock, const struct sockaddr* addr, socklen_t addr_size) const;

	int FileSend(std::string file_name, int sock,
		const struct sockaddr* addr, socklen_t addr_size) const;

public:

	Server();

	int Setup(const char* port);

	int StartUp();

	
	
	~Server();
};

template <typename... Args>
int Server::AddUser(Args ...args)
{
	mMutexAll_.insert(id, std::mutex{});
	
	connected_.emplace_front(args...);
	Ping(connected_.front());

	++cliNo_;

	return EXIT_SUCCESS;
}


