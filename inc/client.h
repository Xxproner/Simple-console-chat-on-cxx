#pragma once 

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <fstream>
#include <thread>

#define DEFAULT_VALUE 30

class Client
{
private:

	struct JoinDeleter
	{
		JoinDeleter() = default;
		void operator()(std::thread* thread) const noexcept;
		~JoinDeleter() = default;
	};

	using joined_thrd = std::unique_ptr<std::thread, JoinDeleter>;

	using SendRecv_thrds = std::pair<joined_thrd, joined_thrd>;

	enum class FILE_STATUS
	{
		SEND ,
		RECV
	};

	struct sockaddr_in serv_addr_;
	socklen_t serv_addr_len_;
	SendRecv_thrds communication_threads_;
	bool keep_;  // mutable
	int communication_socket_;
	inline void network_error(const char*) const;
	constexpr static size_t error_msg_size = 30;
	mutable char error_msg[error_msg_size] = "NO ERROR";

	int EstablishNewConnection(const std::string& cmdFILE_file_name,
		FILE_STATUS STATUS) const;

	int Send();
	int Recv() const;

	int FileRecv(std::string, int, const struct sockaddr*, socklen_t) const;

	int FileSend(std::string, int, const struct sockaddr*, socklen_t) const;
	
	std::string BuildMessage(std::string& raw_msg, const std::string& name) const;

public:

	Client() = default;
	
	void SetServerConfig(const char* ip_address, int port);

	int CreateSocket();
	
	int ConnectServer();
	
	void network_perror(const char*) const;

	~Client();
};


