#include "loop.hpp"
#include "server_connection.hpp"
#include "service.hpp"
#include "make_unique.hpp"

#include <iostream>
#include <memory>


#include <fcntl.h>  // open()
#include <unistd.h> // read()

bool timeout_tick() {
	std::cout << "tick" << std::endl;
	static int i = 0;
	return (++i==3)?(i=0):i;
}

bool timeout_tock() {
	std::cout << "  tock" << std::endl;
	static int i = 0;
	// static int j = 0;
	if (++i == 6) {
		i = 0;
		mini_saftlib::Loop::get_default().connect(
			std::move(
				std2::make_unique<mini_saftlib::TimeoutSource>(
					sigc::ptr_fun(timeout_tick),std::chrono::milliseconds(1000), std::chrono::milliseconds(-500)
				)
			)
		);
	}
	return true;
}

static bool fd_callback(int fd, int condition);
void init_fd(){ 
	std::cerr << "init_fd" << std::endl;
	int fd = open("my_pipe", O_RDONLY | O_NONBLOCK);
	mini_saftlib::Loop::get_default().connect(
		std::move(
			std2::make_unique<mini_saftlib::IoSource>(
				sigc::ptr_fun(fd_callback), fd, POLLIN | POLLHUP
			)
		)
	);
}
bool fd_callback(int fd, int condition) {
	if (condition & POLLHUP) {
		std::cerr << "pollhup called" << std::endl;
		close(fd);
		init_fd();
		return false;
	}
	const int size = 10;
	char ch[size] = {0,};
	read(fd, &ch, size-1);
	std::cerr << ch;
	return true;
}

int main() {
	// two lines just to play around ... has nothing to do with the saftd functionality.
	init_fd();
	mini_saftlib::Loop::get_default().connect(std::move(std2::make_unique<mini_saftlib::TimeoutSource>(sigc::ptr_fun(timeout_tock), std::chrono::milliseconds(1000), std::chrono::milliseconds(500))));

	// create a mini-saftlib-server and let it run
	mini_saftlib::ServerConnection server_connection;
	mini_saftlib::Loop::get_default().run();

	return 0;
}