#ifndef SAFTBUS_LOGGER_H_
#define SAFTBUS_LOGGER_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <sigc++/sigc++.h>

namespace saftbus 
{

class Logger 
{
public: 
	Logger(const std::string &filename, bool flush_often = true);

	void enable();
	void disable();

	Logger& newMsg(int severity);
	template<class T> 
	Logger& add(const T &content) {
		if (enabled) {
			msg << content;
		}
		return *this;
	}
	Logger& add(const std::string &content);
	void log();

private:

	bool enabled;

	std::string getTimeTag();

	bool flush_after_log;

	std::ostringstream msg;
	std::ofstream file;


};


} // namespace saftbus

#endif 