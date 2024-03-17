#include <sstream>

int Server::WhoRequest(const int id, const std::string&)
{
	std::string all_users_name;
	for (const auto& user : connected_)
	{
		if (user.sockfd_ == id)
		{
			all_users_name.append("[You] aka ");
		}

		all_users_name.append(user.pseudo_);
		all_users_name.push_back('\n');
	}

	if (send(id, all_users_name.c_str(), all_users_name.length(), MSG_NOSIGNAL) < 0); // ERROR CLOSE OR WAIT
	{
		queue_for_deleting.push(id);
	}


	return EXIT_SUCCESS;
}

int Server::Leave(const int id, const std::string&) 

{
	queue_for_deleting.push(id);

	std::string msg;
	auto deleteUserIter = FindUserbyid(id);
	if (deleteUserIter != connected_.cend())
	{
		msg = (*deleteUserIter).pseudo_ + " left us. Bye(";

	} else 
	{
		msg = "Someone left us. Bye(";
	}

	SendAll(msg.c_str(), msg.length(), id);

	return EXIT_SUCCESS;	  
}

int Server::Help(const int id, const std::string&)  
{
	std::stringstream note;

	PossibleCommandsInfo(note);

	ull bytes_sendD = 0, bytes_send;

	while (bytes_sendD < note.str().length())
	{
		bytes_send = send(id, note.str().c_str() + bytes_sendD, 
			note.str().length() - bytes_sendD, MSG_NOSIGNAL);


		if (bytes_send <= 0)
		{
			queue_for_deleting.push(id);
			return EXIT_FAILURE;
		}

		bytes_sendD += bytes_send;
	}

	return EXIT_SUCCESS;
}

int Server::ChangeChannel(const int id, const std::string& params) 
{
	int new_channel;
	try
	{
		new_channel = stoi(params);
		auto user = FindUserbyid(id);
		if (user == connected_.end())
			return EXIT_FAILURE;

		(*user).channel_ = new_channel;

	} catch (const std::exception& ex)
	{
		send(id, ex.what(), ch_traits::length(ex.what()), MSG_NOSIGNAL);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int Server::SendPrivateMessage(const int id, const std::string& params) 
{
	size_t idx = params.find(' ');
	if (idx == std::string::npos)
	{
		const char* error_msg = "Too few arguments. [WHO] [WHAT]. Use /HELP command for more information!";
		send(id, error_msg, ch_traits::length(error_msg), MSG_NOSIGNAL);
		return EXIT_FAILURE;
	}
	
	std::string whom = params.substr(0, idx);
	std::string what = params.substr(idx + 1);

	auto user = FindUserbyname(whom);
	if (user == connected_.cend())
	{
		const char* error_msg = "User not found. Use /WHO command for available ones!";
		send(id, error_msg, ch_traits::length(error_msg), MSG_NOSIGNAL);
		return EXIT_FAILURE;
	}

	send((*user).sockfd_, what.c_str(), what.length(), MSG_NOSIGNAL);

	return EXIT_SUCCESS;
}

void Server::PossibleCommandsInfo(std::ostream& out) const
{
	std::ifstream info;
	info.open("../COMMANDS.txt", std::ios::in);
	if (!info.is_open())
	{
		std::cerr << "COMMANDS.txt is not found\n";
		out << "Commands list is not found. Sorry";
	} else 
	{
		std::string line;
		while (std::getline(info, line))
		{
			out << line << '\n';
		}

		info.close();		
	}

}

int Server::EstablishNewConnection(const std::string& cmdFILE_file_name) const
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

int Server::FileRecv(std::string file_name,
	int sock, const struct sockaddr* addr, socklen_t addr_size) const
{

	sockaddr* cli_addr = new sockaddr;
	socklen_t* cli_addr_len = new socklen_t;
	// recvfrom(sock, buf, buf_size, addr, addr_size, flags);
	long netw_data;
	long file_size;
	size_t data_size;

	const char* materials_path = "materials/";
	file_name.insert(file_name.cbegin(), &materials_path[0], &materials_path[strlen(materials_path)]);

	std::ofstream file(file_name.c_str(), std::ios::trunc | std::ios::binary);
	
	if (!file.is_open())
	{
		// network_error(("Cannot create or open file: " + file_name).c_str());
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


int Server::FileSend(std::string file_name, int sock,
	const struct sockaddr* addr, socklen_t addr_size) const
{

	const char* materials_path = "materials/";
	file_name.insert(file_name.cbegin(), &materials_path[0], &materials_path[strlen(materials_path)]);

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
			// network_error(("Overflow file max size" + std::to_string(sizeof(long))).c_str());
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