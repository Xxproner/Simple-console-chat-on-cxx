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
