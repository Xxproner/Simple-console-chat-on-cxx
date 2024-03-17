#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>

#include <string>
#include <iostream>
#include <fstream>
#include <cassert>
#include <thread>
#include <algorithm>
#include <type_traits>

#include "client.h"

typedef unsigned long long ull;
// class client

void Client::JoinDeleter::operator()(std::thread* thread) const noexcept
{
	thread->join();
	delete thread;
};

inline void Client::network_error(const char* error) const
{
	std::char_traits<char>::copy(error_msg, error, Client::error_msg_size);
};

void Client::network_perror(const char* note_msg) const
{
	if (std::char_traits<char>::length(error_msg) == std::char_traits<char>::length("NO ERROR") && 
		std::char_traits<char>::compare(error_msg, "NO ERROR", 8) == 0)
	{
		perror(note_msg);	
		return;
	}
	
	fprintf(stderr, "%s:%s", note_msg, error_msg);
	std::char_traits<char>::copy(error_msg, "NO ERROR", 8);

};

void Client::SetServerConfig(const char* ip_address, int port)
{ 
	bzero((char*)&serv_addr_, sizeof(struct sockaddr));

	serv_addr_.sin_family = AF_INET;
	serv_addr_.sin_addr.s_addr = inet_addr(ip_address);
	serv_addr_.sin_port = htons(port);
}

int Client::CreateSocket()
{
	int desc = ::socket(AF_INET, SOCK_STREAM, 0);
	if (desc < 0)
	{
		return EXIT_FAILURE;
	}

	communication_socket_ = desc;
	return EXIT_SUCCESS;
}

int Client::ConnectServer()
{

	if (connect(communication_socket_, reinterpret_cast<sockaddr*>(&serv_addr_),
		sizeof(serv_addr_)) < 0)
	{
		close(communication_socket_);
		communication_socket_ = -1;
		return EXIT_FAILURE;
	}

	keep_ = true;

	try
	{

		communication_threads_ = SendRecv_thrds(joined_thrd(new std::thread(&Client::Send, this)), 
			joined_thrd(new std::thread(&Client::Recv, this)));
	} catch (const std::exception& ex)
	{
		//processing error;
		throw ex.what();
	}
	
	return EXIT_SUCCESS;
}	

int Client::Recv() const 
{
	fd_set master;
	FD_ZERO(&master);
	size_t kBUF_SIZE = 128;
	char buf[kBUF_SIZE];
	ull bytes_recv = 0;
	while (true)
	{ // epol
		FD_SET(communication_socket_, &master);
		if (select(communication_socket_ + 1, &master, NULL, NULL, NULL) < 0) // wait until recv
		{
			// return EXIT_FAILURE;
			break;
		} 

		if (FD_ISSET(communication_socket_, &master))
		{
			bytes_recv = recv(communication_socket_, buf, kBUF_SIZE, 0);

			if (bytes_recv < 0)
				// return EXIT_FAILURE;
				break;
			else if (bytes_recv == 0)
			{
				// return EXIT_SUCCESS;
				break;
			}

			buf[bytes_recv] = '\0';
			std::cout << buf << std::endl;
		}

	}

	perror("[RECV] status");
	return EXIT_SUCCESS;
}

int Client::Send()
{
	std::cout << "What is your nickname? ";
	std::cout.flush();
	std::string msg;
	std::getline(std::cin, msg);

	if (send(communication_socket_, msg.c_str(), msg.length(), 0) <= 0)
	{
		keep_ = false;
	}

	const std::string nickname = std::move(msg);
	ull bytes_send = 0;
	
	while (keep_)
	{
		std::getline(std::cin, msg);
		
		// std::string builtMsg = BuildMessage(msg, nickname);
		
		bytes_send = send(communication_socket_, msg.c_str(), msg.length(), 0);

		if (bytes_send < 0)
			// return EXIT_FAILURE;
			break;
		else if (bytes_send < msg.length())
		{
			ull add_bytes_send = 0;
			while (bytes_send < msg.length())
			{
				add_bytes_send = send(communication_socket_, msg.c_str() + bytes_send, 
					msg.length() - bytes_send, 0);

				if (add_bytes_send < 0)
					// return EXIT_FAILURE;
					break;
				else if (add_bytes_send == 0)
					// return EXIT_SUCCESS;
					break;

				bytes_send += add_bytes_send;
			}

		}

		if (msg == "/LEAVE" || msg == "/QUIT")
			keep_ = false;
		else if (std::char_traits<char>::compare(
					msg.c_str(), "FILE", 4) == 0)
		{
			std::thread file_transfer_thread = std::thread(
				&Client::EstablishNewConnection, this, msg, FILE_STATUS::SEND);
			file_transfer_thread.detach();
		}
			
		bytes_send = 0;

	}

	perror("[SEND] status");
	shutdown(communication_socket_, SHUT_RD);
	return EXIT_SUCCESS;
}


std::string Client::BuildMessage(std::string& raw_msg, const std::string& name) const
{
	std::string msg = raw_msg;
	if (raw_msg[0] == '/') // command
	{
		while (isspace(raw_msg.back())) { raw_msg.pop_back(); }
		size_t idx = raw_msg.find(' ');
		std::string command, params; 

		if (idx == std::string::npos)
		{
			command = raw_msg.substr(1, idx);
			params = "";
		}else 
		{
			command = raw_msg.substr(1, idx);
			params = raw_msg.substr(idx + 1);
		}

		msg = "user:" + name + "\nCOMMAND:" + command + "\nOPTION:" + params;
	}

	return msg;
}

int Client::EstablishNewConnection(const std::string& cmdFILE_file_name,
	FILE_STATUS STATUS) const
{
	const size_t kDATA_SIZE = 30; 
	size_t idx = cmdFILE_file_name.find(" ");
	if (idx == std::string::npos)
	{
		network_error("Needed file name");
		return EXIT_FAILURE;
	}

	const size_t kBUF_SIZE = 25;
	char buf[kBUF_SIZE];

	if (recv(communication_socket_, buf, kBUF_SIZE, 0) < 0)
		return EXIT_FAILURE;

	char* port = strchr(buf, ':');

	if (!port)
		return EXIT_FAILURE;
	port += 1;

	struct sockaddr_in addr_for_transfer;
	socklen_t addr_for_transfer_size = sizeof(addr_for_transfer);
	memcpy(&addr_for_transfer, &serv_addr_, addr_for_transfer_size);
	addr_for_transfer.sin_port = atoi(port);

	int newsockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (newsockfd < 0)
		return EXIT_FAILURE;

	if (STATUS == FILE_STATUS::SEND)
	{
		FileSend(cmdFILE_file_name.c_str() + (idx + 1), newsockfd, 
			reinterpret_cast<sockaddr*>(&addr_for_transfer), addr_for_transfer_size);
	} else
	{
		if (bind(newsockfd, reinterpret_cast<sockaddr*>(&addr_for_transfer), addr_for_transfer_size) < 0) 
			return EXIT_FAILURE;
		FileRecv(cmdFILE_file_name.c_str() + (idx + 1), newsockfd, 
			reinterpret_cast<sockaddr*>(&addr_for_transfer), addr_for_transfer_size);
	}
	close(newsockfd);
	return EXIT_SUCCESS;
}

int Client::FileRecv(const int id, std::string file_name,
	int sock, const struct sockaddr* addr, socklen_t addr_size) const
{
	size_t idx = cmdFILE_file_name.find(" ");
	if (idx == std::string::npos)
	{
		network_error("Needed file name");
		return EXIT_FAILURE;
	}

	const size_t kBUF_SIZE = 25;
	char buf[kBUF_SIZE];

	const auto user = FindUserbyid(id);
	if (user == connected_.cend())
		return EXIT_FAILURE;

	struct sockaddr_storage addr_for_transfer;
	memset(&addr_for_transfer, 0, sizeof(struct sockaddr));
	memcpy(&addr_for_transfer, &(*user).user_addr_, (*user).size_addr_); 

	if (addr_for_transfer.ss_family == AF_INET)
		reinterpret_cast<sockaddr_in*>(&addr_for_transfer)->sin_port = 0;
	else if (addr_for_transfer.ss_family == AF_INET6)
		reinterpret_cast<sockaddr_in6*>(&addr_for_transfer)->sin_port6 = 0;
	else
		throw std::logic_error("Config address for transfer");

	int newsockfd = ::socket(addr_for_transfer.ss_family, SOCK_DGRAM, 0);
	if (bind(newsockfd, reinterpret_cast<sockaddr*>(&addr_for_transfer), addr_for_transfer_size) < 0)
		return EXIT_FAILURE; 

	if (recv(communication_socket_, buf, kBUF_SIZE, 0) < 0)
		return EXIT_FAILURE;

	char* port = strchr(buf, ':');

	if (!port)
		return EXIT_FAILURE;
	port += 1;

	struct sockaddr_in addr_for_transfer;
	socklen_t addr_for_transfer_size = sizeof(addr_for_transfer);
	memcpy(&addr_for_transfer, &serv_addr_, addr_for_transfer_size);
	addr_for_transfer.sin_port = atoi(port);

	int newsockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (newsockfd < 0)
		return EXIT_FAILURE;

	if (STATUS == FILE_STATUS::SEND)
	{
		FileSend(cmdFILE_file_name.c_str() + (idx + 1), newsockfd, 
			reinterpret_cast<sockaddr*>(&addr_for_transfer), addr_for_transfer_size);
	} else
	{
		if (bind(newsockfd, reinterpret_cast<sockaddr*>(&addr_for_transfer), addr_for_transfer_size) < 0) 
			return EXIT_FAILURE;
		FileRecv(cmdFILE_file_name.c_str() + (idx + 1), newsockfd, 
			reinterpret_cast<sockaddr*>(&addr_for_transfer), addr_for_transfer_size);
	}
	close(newsockfd);
	return EXIT_SUCCESS;

	sockaddr* cli_addr = new sockaddr;
	socklen_t* cli_addr_len = new socklen_t;
	// recvfrom(sock, buf, buf_size, addr, addr_size, flags);
	long netw_data;
	long file_size;
	size_t data_size;

	const char* materials_path = "cli_materials/";
	file_name.insert(file_name.cbegin(), &materials_path[0], &materials_path[strlen(materials_path)]);

	std::ofstream file(file_name.c_str(), std::ios::trunc | std::ios::binary);
	
	if (!file.is_open())
	{
		network_error(("Cannot create or open file: " + file_name).c_str());
		delete cli_addr; delete cli_addr_len;
		return EXIT_FAILURE;
	}

	recvfrom(sock, &netw_data, sizeof(long), 0,
		cli_addr, cli_addr_len);
	netw_data = ntohl(netw_data);

	recvfrom(sock, &netw_data, sizeof(long), 0,
		cli_addr, cli_addr_len);
	data_size = ntohl(data_size);

	char* buf = new char[data_size];

	size_t bytes_recv;
	do
	{
		bytes_recv = recvfrom(sock, buf, data_size, MSG_WAITALL,
			cli_addr, cli_addr_len);
		if (bytes_recv < 0)
		{
			delete cli_addr; delete cli_addr_len;
			return EXIT_FAILURE;
		}
		buf[bytes_recv] = 0;
		file.write(buf, data_size);
	}while (bytes_recv > 0);
	
	file.close();
	delete buf;
	delete cli_addr; delete cli_addr_len;
	return EXIT_SUCCESS;
}


int Client::FileSend(std::string file_name, int sock,
	const struct sockaddr* addr, socklen_t addr_size) const
{
	const size_t kDATA_SIZE = 30;
	std::ifstream file(file_name.c_str(), std::ios::ate | std::ios::binary);
	
	if (!file.is_open())
	{
		network_error(("Cannot open file: " + file_name).c_str());
		return EXIT_FAILURE;
	}

	auto file_end_pos = file.tellg();
	long file_size;

	if (std::is_convertible<decltype(file_end_pos), long>::value)
	{
		try
		{
			file_size = static_cast<long>(file_end_pos);
		} catch(const std::exception& ex)
		{
			network_error(("Overflow file max size" + std::to_string(sizeof(long))).c_str());
			file.close();
			return EXIT_FAILURE;
		}
	} 

	file_size = htonl(file_size);
	sendto(sock, &file_size, sizeof(long), MSG_CONFIRM,
		addr, addr_size);

	long newt_data = htonl(static_cast<long>(kDATA_SIZE));
	sendto(sock, &newt_data, sizeof(long), MSG_CONFIRM,
		addr, addr_size);

	char* buf = new char[kDATA_SIZE];
	while (file.read(buf, kDATA_SIZE))
	{
		sendto(sock, buf, kDATA_SIZE, 0,
			addr, addr_size);
	}

	size_t addition_num_bytes = file.gcount();
	buf[addition_num_bytes] = 0;

	sendto(sock, buf, addition_num_bytes, 0,
		addr, addr_size);

	file.close();
	delete buf;
	return EXIT_SUCCESS;
}


Client::~Client()
{
	if (communication_socket_ != -1)
		close(communication_socket_);
}

