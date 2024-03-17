#include <stdlib.h>

#include <cassert>

#include "Server.hh"

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		assert(false && "Usage: server port");
	}

	Server server;

	if (server.Setup(argv[1]) == EXIT_FAILURE)
	{
		perror("Setup server error");
		std::cout << "Info : " << server.GetServerReport();
		return EXIT_FAILURE;
	}

	if (server.StartUp() == EXIT_FAILURE)
	{
		perror("Start up server error");
		std::cout << "Info : " << server.GetServerReport();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;

}