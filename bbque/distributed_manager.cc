#include "bbque/distributed_manager.h"
#include "bbque/configuration_manager.h"
#include "bbque/modules_factory.h"

// #define LOCAL_TEST

#define DISM_DIV1 "================================================================="
#define DISM_DIV2 "|-----------------------+---------+--------------+--------------|"
#define DISM_HEAD "|          IP           |   RTT   | AVAILABILITY |    STATUS    |"
#define DISM_DIV3 "|                       |         |              |              |"

namespace bbque {

DistributedManager::DistributedManager() {

	this->configured = false;

	// PlatformManager & plm(PlatformManager::GetInstance());
	// plm = PlatformManager::GetInstance();

	// rpp(plm.GetRemotePlatformProxy());

	// Get a logger
	logger = bu::Logger::GetLogger(DISTRIBUTED_MANAGER_NAMESPACE);
	assert(logger);

	Configure();

	// Insert the IP address (local in this case) of the known instances. There are three different ports beacuse I will test on 3 different instances runing on my PC.
	// known_instance_set.push_back(0.0.0.0:8850);
	// known_instance_set.push_back(0.0.0.0:8851);
	// known_instance_set.push_back(0.0.0.0:8852);

	// this->rpp = std::unique_ptr<bbque::pp::RemotePlatformProxy>(plm.GetRemotePlatformProxy());
	//this->rpp = std::unique_ptr<pp::RemotePlatformProxy>(new pp::RemotePlatformProxy());
}

DistributedManager & DistributedManager::GetInstance() {
	static DistributedManager instance;
	return instance;
}

void DistributedManager::Discover(std::string ip) {
	// logger->Debug("dism: Discover: 0");

	std::mutex m;
	std::condition_variable cv;

	bbque::agent::ExitCode_t result = bbque::agent::ExitCode_t::AGENT_UNREACHABLE;

	bool discovered;

	PlatformManager & plm = PlatformManager::GetInstance();
	pp::RemotePlatformProxy* rpp = plm.GetRemotePlatformProxy();

	std::thread t([&cv, &result, this, ip, rpp]()
	{
		bbque::agent::DiscoverRequest iam;
		iam.iam = bbque::agent::IAm::INSTANCE;

		result = rpp->Discover(ip, iam);

		// logger->Debug("dism: Discover: 1");

		cv.notify_one();
	});

	t.detach();

	{
		std::unique_lock<std::mutex> l(m);
		if(cv.wait_for(l, std::chrono::seconds(2)) == std::cv_status::timeout) {
			// logger->Debug("dism: Discover: 3");

			discovered = false;
		}
	}

	if (result != bbque::agent::ExitCode_t::OK) {
		// logger->Debug("dism: Discover: 4");

		discovered = false;
	}
	else {
		// logger->Debug("dism: Discover: 4.5");

		discovered = true;
	}

	// logger->Debug("dism: Discover: 5");

	general_mutex.lock();

	// If the instance has been discovered then map to a system with the lowest available sys_id.
		if (discovered) {
			// logger->Debug("dism: Discover: 6");

			// If the instance was already discovered previously.
			if (discovered_instances.count(ip)){
				// logger->Debug("dism: Discover: 7");

				general_mutex.unlock();

				return;
			}
			else {
				// logger->Debug("dism: Discover: 8");

				discovered_instances.insert(ip);

				// Get the first available id and set it to the new instance.
				int id = 1;
				bool stop = false;
				while (!stop) {
					// logger->Debug("dism: Discover: 9");

					if (instances_map.count(id) == 0) {
						// logger->Debug("dism: Discover: 10");

						instances_map[id] = ip;
						stop = true;
					}
					id++;
				}
			}
		}
		else {
			// logger->Debug("dism: Discover: 11");

			// If the instance is in the map and set variables then remove it from them.
			if (discovered_instances.count(ip)) {
				// logger->Debug("dism: Discover: 12");

				discovered_instances.erase(ip);
	
				int id = 1;
				bool stop = false;
				while (!stop) {
					// logger->Debug("dism: Discover: 13");

					if(instances_map[id] == ip) {
						// logger->Debug("dism: Discover: 14");

						instances_map.erase(id);
						stop = true;
					}

					id++;
				}
			}
		}

		general_mutex.unlock();

		return;
}

void DistributedManager::DiscoverInstances() {
	for (std::vector<std::string>::iterator it = ipAddresses.begin(); it != ipAddresses.end(); ++it) {
		if(*it == (local_IP))
		{
			// char buffer[20];
			// sprintf(buffer, "local_IP: %s", local_IP.c_str());
			// logger->Debug(buffer);

			continue;
		}

		threads.push_back(std::thread(&DistributedManager::Discover, this, *it));
	}

	for (auto & th: threads)
		if (th.joinable())
			th.join();

	logger->Debug("DiscoverInstances: joined all threads");
}

int DistributedManager::Ping(int system_id) {
	std::mutex m;
	std::condition_variable cv;

	int ping_value = 0;
	bbque::agent::ExitCode_t result;

	std::thread t([&cv, &result, this, system_id, &ping_value]()
	{
		// result = this->rpp->Ping(system_id, ping_value);
		cv.notify_one();
	});

	t.detach();

	{
		std::unique_lock<std::mutex> l(m);
		if(cv.wait_for(l, std::chrono::seconds(5)) == std::cv_status::timeout)
			return 0;
	}

	if (result != bbque::agent::ExitCode_t::OK) return 0;

	return ping_value;
}

void DistributedManager::PingInstances() {
	instance_stats_map.clear();

	for (std::vector<int>::const_iterator it = instance_set.begin(); it != instance_set.end(); ++it)
	{
		int i;
		int ping_value;
		int ping_sum = 0;
		int successful_pings_counter = 0;
		double mean_ping_value;
		Instance_Stats_t stats;

		// Ping the instance 10 times to get a truthful mean ping value.
		for (i = 0; i < 10; i++)
		{
			ping_value = Ping(*it);

			// If ping has not failed.
			if (ping_value != 0)
			{
				ping_sum += ping_value;
				successful_pings_counter++;
			}
		}

		// Compute mean ping.
		mean_ping_value = ping_sum / successful_pings_counter;

		// Save stats in map data structure.
		stats.RTT = mean_ping_value;
		stats.availability = successful_pings_counter * 10; // % value
		instance_stats_map[*it] = stats;
	}
}

void DistributedManager::PrintStatusReport() {
	logger->Debug("Report on instances:");
	logger->Debug(DISM_DIV1);
	logger->Debug(DISM_HEAD);
	logger->Debug(DISM_DIV2);

	for (std::vector<std::string>::const_iterator it = ipAddresses.begin(); it != ipAddresses.end(); ++it) {
		std::string sys;
		std::string RTT;
		std::string availability;
		std::string status;
		char buffer[70];

		// If the iterator is considering the ip address of this machine
		if(*it == local_IP)
		{
			sprintf(buffer, "| %21s |    -    |       -      |    MYSELF    |", (*it).c_str());
			logger->Debug(buffer);

			continue;
		}

		// If the instance at the current IP is available.
		if (std::find(discovered_instances.begin(), discovered_instances.end(), *it) != discovered_instances.end()) {
			sprintf(buffer, "| %21s |    -    |       -      |      OK      |", (*it).c_str());
			logger->Debug(buffer);
		}
		else {
			// sprintf(buffer, "| %21s | %5.1f |      %2i      |      OK      |", *it, instance_stats_map[*it].RTT, instance_stats_map[*it].availability);
			sprintf(buffer, "| %21s |    -    |       -      | DISCONNECTED |", (*it).c_str());
			logger->Debug(buffer);
		}
	}

	logger->Debug(DISM_DIV3);
	logger->Debug(DISM_DIV1);

	logger->Debug("instances:\n");
	int index;
	general_mutex.lock();
	for (index = 0; index < instances_map.size(); index++) {
		logger->Debug("%d: %s\n", index, instances_map[index].c_str());
	}
	general_mutex.unlock();
	logger->Debug("-----------------");
}

bool DistributedManager::Configure() {

	if (this->configured)
		return true;

	ConfigurationManager & cm = ConfigurationManager::GetInstance();

	// agent_proxy = std::unique_ptr<bbque::plugins::AgentProxyIF>(
	// 	ModulesFactory::GetModule<bbque::plugins::AgentProxyIF>(
	// 		std::string(AGENT_PROXY_NAMESPACE) + ".grpc"));

		//---------- Loading configuration
	boost::program_options::options_description
		distributed_manager_opts_desc("Distributed Manager options");
	distributed_manager_opts_desc.add_options()
		("DistributedManager.start_address",
		 boost::program_options::value<std::string>
		 (&start_address)->default_value(""),
		 "Distributed start address");
	distributed_manager_opts_desc.add_options()
		("DistributedManager.end_address",
		 boost::program_options::value<std::string>
		 (&end_address)->default_value(""),
		 "Distributed end address");
	distributed_manager_opts_desc.add_options()
		("DistributedManager.discover_period_s",
		 boost::program_options::value<uint16_t>
		 (&discover_period_s)->default_value(0),
		 "Distributed discover period");
	distributed_manager_opts_desc.add_options()
		("DistributedManager.ping_period_s",
		 boost::program_options::value<uint16_t>
		 (&ping_period_s)->default_value(0),
		 "Distributed ping period");
#ifdef LOCAL_TEST
	distributed_manager_opts_desc.add_options()
		("AgentProxy.port",
		 boost::program_options::value<std::string>
		 (&port_num)->default_value(""),
		 "Server port number");
#endif
	boost::program_options::variables_map opts_vm;
	cm.ParseConfigurationFile(distributed_manager_opts_desc, opts_vm);

	BuildIPAddresses();

	this->configured = true;

	return true;
}

void DistributedManager::BuildIPAddresses() {
	logger->Debug("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
#ifdef LOCAL_TEST
	logger->Debug("CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC");

	// Get the ports in string
	std::string colon = ":";
	std::string start_port_str = start_address.substr(start_address.find(colon) + 1);
	std::string end_port_str = end_address.substr(end_address.find(colon) + 1);

	// Get the port in int
	std::string::size_type size;
	int start_port = std::stoi(start_port_str, &size);
	int end_port = std::stoi(end_port_str, &size);

	// Build the address strings and put them in ipAddresses
	std::string ipAddress;
	std::string port_str;
	int port = start_port;
	while(port <= end_port)
	{
		port_str = std::to_string(port);
		ipAddress = "127.0.0.1:" + port_str;
		ipAddresses.push_back(ipAddress);

		port++;
	}

	// Print the addresses for debug
	// logger->Debug("ipAddresses length: %d", ipAddresses.size());
	// for(auto it = ipAddresses.begin(); it != ipAddresses.end(); ++it)
	// {
	// 	logger->Debug("%s", (*it).c_str());
	// }		
#else
	logger->Debug("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
	std::string ipAddress;
	std::string::size_type size;
	size_t pos;
	std::string token;
	int i;
	std::string start_address_temp = start_address;
	std::string end_address_temp = end_address;
	for (i = 0; i < 4; i++)
	{
		pos = start_address_temp.find(".");
		token = start_address_temp.substr(0, pos);
		start_address_temp.erase(0, pos + 1);
	}
	int counter = std::stoi(token, &size);
	for (i = 0; i < 4; i++)
	{
		pos = end_address_temp.find(".");
		token = end_address_temp.substr(0, pos);
		end_address_temp.erase(0, pos + 1);
	}
	int counter_end = std::stoi(token, &size);

	char buffer[50];
	sprintf(buffer, "counter: %d", counter);
	logger->Debug(buffer);
	sprintf(buffer, "counter_end: %d", counter_end);
	logger->Debug(buffer);

	// baseAddress is in the form "x.y.z."
	start_address_temp = start_address;
	std::string baseAddress;
	for (i = 0; i < 3; i++)
	{
		pos = start_address_temp.find(".");
		token = start_address_temp.substr(0, pos);
		start_address_temp.erase(0, pos + 1);
		baseAddress = baseAddress + token + ".";
	}

	sprintf(buffer, "baseAddress: %s", baseAddress.c_str());
	logger->Debug(buffer);

	while(counter <= counter_end)
	{
		ipAddress = baseAddress + std::to_string(counter);
		ipAddresses.push_back(ipAddress);

		counter++;
	}

	// Print the addresses for debug
	logger->Debug("ipAddresses length: %d", ipAddresses.size());
	for(auto it = ipAddresses.begin(); it != ipAddresses.end(); ++it)
	{
		sprintf(buffer, "ipAddresses: %s", (*it).c_str());
		logger->Debug(buffer);
	}	

	return;
#endif

	return;
}

bool DistributedManager::FindMyOwnIPAddresses() {

	if (getInterfacesIPs() == false)
		return false;

	// FInd wich of the IP addresses is the one in the range as specified in configuration file
	for(auto it = local_IP_addresses.begin(); it != local_IP_addresses.end(); ++it)
	{
#ifdef LOCAL_TEST
		for(auto ip_it = ipAddresses.begin(); ip_it != ipAddresses.end(); ++ip_it)
		{
			std::string ip = ip_it->substr(0, ip_it->find(":"));

			// Just for debug print all the supported IP addresses
			// char buffer[20];
			// sprintf(buffer, "ip_it: %s", (*ip_it).c_str());
			// logger->Debug(buffer);

			if(ip == *it)
			{
				// local_IP in LOCAL_TEST need to be along with the port specified in configuration file
				local_IP = ip + ":" + port_num;
				
				return true;
			}			
		}
#else
		for(auto ip_it = ipAddresses.begin(); ip_it != ipAddresses.end(); ++ip_it)
		{
			if(*ip_it == *it)
			{
				local_IP = *it;
				
				return true;
			}			
		}
#endif
	}

	return false;
}

bool DistributedManager::getInterfacesIPs() {
	struct ifaddrs *ifaddr, *ifa;
	int family, s, n;
	char host[20];

	if (getifaddrs(&ifaddr) == -1) {
		return false;
	}

	/* Walk through linked list, maintaining head pointer so we
	  can free list later */
	for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
		if (ifa->ifa_addr == NULL)
			continue;
		family = ifa->ifa_addr->sa_family;

		/* For an AF_INET* interface address, display the address */
		if (family == AF_INET) {
	    	s = getnameinfo(ifa->ifa_addr,
	    		(family == AF_INET) ? sizeof(struct sockaddr_in) :
	    		sizeof(struct sockaddr_in6),host, 20,
	    		NULL, 0, NI_NUMERICHOST);

	    	if (s != 0) {
	    		if(local_IP_addresses.empty())
	    			return false;
	    		else
	    			continue;
	    	}

	    	local_IP_addresses.insert(host);
	   }
	}

	// Just for debug print all the local IP addresses
	// for(auto it = local_IP_addresses.begin(); it != local_IP_addresses.end(); ++it)
	// {
	// 	char buffer[20];
	// 	sprintf(buffer, "it: %s", (*it).c_str());
	// 	logger->Debug(buffer);
	// }

	freeifaddrs(ifaddr);

	return true;
}

void DistributedManager::Task() {
	logger->Info("Distributed Manager monitoring thread STARTED");

	if(FindMyOwnIPAddresses() == false)
	{
		logger->Error("Distributed Manager didn't found any local ip address");

		return;
	}

	instances_map[0] = local_IP;

	// just for debug print the local IP address
	// char buffer[20];
	// sprintf(buffer, "local_IP: %s", local_IP.c_str());
	// logger->Debug(buffer);
	// sprintf(buffer, "port_num: %s", port_num.c_str());
	// logger->Debug(buffer);

	// Calculate the greates common divider. The result will be used as seconds in the sleep function.
	boost::math::gcd_evaluator<int> gcd_eval;
	int gcd = gcd_eval(discover_period_s, ping_period_s);

	// Contains how many times discover and ping period are greater than gcd.
	// They are used to trigger discover and ping functions at specific awakening rate time of the loop.
	int times_discover_period = discover_period_s / gcd;
	int times_ping_period = ping_period_s / gcd;

	int discover = times_discover_period;
	int ping = times_ping_period;

	while (!done) {

		if(discover % times_discover_period == 0)
		{
			discover = 0;

			logger->Debug("------------------------");
			logger->Debug("Discover instances START");
			DiscoverInstances();
			logger->Debug("Discover instances END");
			logger->Debug("----------------------");
		}

		if(ping % times_ping_period == 0)
		{
			ping = 0;
			// PingInstances();
			// logger->Info("Distributed Manager: PING");
		}

		discover++;
		ping++;

		PrintStatusReport();

		std::this_thread::sleep_for(std::chrono::seconds(gcd));
		// logger->Debug("gcd: %d", gcd);
	}

	logger->Info("Distributed Manager monitoring thread END");
}

}
