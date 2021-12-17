#ifndef MINI_SAFTLIB_SERVICE_
#define MINI_SAFTLIB_SERVICE_

#include "saftbus.hpp"

#include <memory>
#include <string>
#include <vector>

namespace mini_saftlib {

	class Service {
		struct Impl; std::unique_ptr<Impl> d;
	public:
		Service(std::vector<std::string> interface_names);
		void call(int client_fd, Deserializer &received, Serializer &send);
		virtual ~Service();
		void add_signal_group(int fd);
	protected:
		virtual void call(unsigned interface_no, unsigned function_no, int client_fd, Deserializer &received, Serializer &send) = 0;
	};



	// Container of all Services provided by mini-saftlib.
	class ServiceContainer {
		struct Impl; std::unique_ptr<Impl> d;
	public:
		ServiceContainer();
		~ServiceContainer();
		// insert an object and return the saftlib_object_id for this object
		// return 0 in case the object_path is unknown
		unsigned create_object(const std::string &object_path, std::unique_ptr<Service> service);
		// return saftlib_object_id if the object_path was found, 0 otherwise
		unsigned register_proxy(const std::string &object_path, int client_fd, int signal_group_fd);
		void unregister_proxy(unsigned saftlib_object_id, int client_fd, int signal_group_fd);
		// call a Service identified by the saftlib_object_id
		// return false if the saftlib_object_id is unknown
		bool call_service(unsigned saftlib_object_id, int client_fd, Deserializer &received, Serializer &send);
	};

	// A Service to access the Container of Services
	// mainly Proxy (de-)registration 
	class ContainerService : public Service {
		struct Impl; std::unique_ptr<Impl> d;
		static std::vector<std::string> gen_interface_names();
	public:
		ContainerService(ServiceContainer *container);
		~ContainerService();
		void call(unsigned interface_no, unsigned function_no, int client_fd, Deserializer &received, Serializer &send);
	};

}

#endif
