#include "bbque/distributed_manager.h"
#include "bbque/configuration_manager.h"
#include "bbque/modules_factory.h"

#define LOCAL_TEST

#define DISM_DIV1 "======================================================================="
#define DISM_DIV2 "|-----------------------+-----+---------+--------------+--------------|"
#define DISM_HEAD "|          IP           | Sys |   RTT   | AVAILABILITY |    STATUS    |"
#define DISM_DIV3 "|                       |     |         |              |              |"

namespace bbque {

DistributedManager::DistributedManager() {

	this->configured = false;

	// Get a logger
	logger = bu::Logger::GetLogger(DISTRIBUTED_MANAGER_NAMESPACE);
	assert(logger);

	Configure();
}

DistributedManager & DistributedManager::GetInstance() {
	static DistributedManager instance;
	return instance;
}

void DistributedManager::Discover(std::string ip) {
	std::mutex m;
	std::condition_variable cv;

	bbque::agent::ExitCode_t result = bbque::agent::ExitCode_t::AGENT_UNREACHABLE;

	bool discovered;

	PlatformManager & plm = PlatformManager::GetInstance();
	pp::RemotePlatformProxy* rpp = plm.GetRemotePlatformProxy();

	bbque::agent::DiscoverRequest iam;
	bbque::agent::DiscoverReply reply;

#ifdef CONFIG_BBQUE_DIST_FULLY

		iam.iam = bbque::agent::IAm::INSTANCE;

#else
#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

		if(local_ID == -1){
			iam.iam = bbque::agent::IAm::NEW;
		}
		else if (local_ID == 0) {
			iam.iam = bbque::agent::IAm::MASTER;
		}
		else{
			iam.iam = bbque::agent::IAm::SLAVE;
		}

#endif
#endif

	std::thread t([&cv, &result, this, ip, rpp, iam, &reply]()
	{
		logger->Debug("Sending Discover to %s", ip.c_str());
		result = rpp->Discover(ip, iam, reply);
		cv.notify_one();
	});

	t.detach();

	{
		std::unique_lock<std::mutex> l(m);
		if(cv.wait_for(l, std::chrono::seconds(2)) == std::cv_status::timeout) {
			logger->Debug("Discover timeout");
			discovered = false;
		}
	}

	if (result != bbque::agent::ExitCode_t::OK) {
		discovered = false;
	}
	else {

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

		amIAlone = false;

#endif

		discovered = true;
	}

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

	if(discovered) {
		switch(local_ID) {
		case -1:
			// NEW
			if(reply.iam == bbque::agent::IAm::MASTER) {
				masterFound = true;
				local_ID = reply.id;
				sys_to_ip_map[0] = ip;
				ip_to_sys_map[ip] = 0;
				general_mutex.lock();
				sys_to_ip_map[local_ID] = local_IP;
				ip_to_sys_map[local_IP] = local_ID;
				general_mutex.unlock();

				logger->Info("MASTER assigned me number: %d", local_ID);
			}
			if(reply.iam == bbque::agent::IAm::SLAVE) {
				general_mutex.lock();
				sys_to_ip_map[reply.id] = ip;
				ip_to_sys_map[ip] = reply.id;
				general_mutex.unlock();

				logger->Debug("SLAVE replied");
			}
			break;
		case 0:
			// MASTER
			if(reply.iam == bbque::agent::IAm::MASTER) {
				logger->Error("Duplicate MASTER found.");

				exit(-1);
			}
			else if (reply.iam == bbque::agent::IAm::SLAVE) {
				// If the discovered instance is a SLAVE then nothing has to be done here.
				;
			}
			else {
				return;
			}
			break;
		default:
			// SLAVE

			// Checking if the instance changed id. If it changed the id remove its reference in sys_to_ip_map
			if(ip_to_sys_map.count(ip) != 0 && ip_to_sys_map[ip] != reply.id && sys_to_ip_map[ip_to_sys_map[ip]] == ip) {
				general_mutex.lock();
				sys_to_ip_map.erase(ip_to_sys_map[ip]);
				general_mutex.unlock();
			}

			if(reply.iam == bbque::agent::IAm::MASTER) {
				masterFound = true;
				sys_to_ip_map[0] = ip;
				ip_to_sys_map[ip] = 0;

				logger->Debug("MASTER replied");
			}
			else if(reply.iam == bbque::agent::IAm::SLAVE) {
				general_mutex.lock();
				sys_to_ip_map[reply.id] = ip;
				ip_to_sys_map[ip] = reply.id;
				general_mutex.unlock();

				logger->Debug("SLAVE replied");
			}
			break;
		}
	}
	else if(local_ID > 0) {
		/* This block is executed only if the instance to which has been send a discover request did not replied
		* to it and this instance is a SLAVE.
		* If this instance knows the MASTER and it does not replies to the request then it remove it from sys_to_ip_map and ip_to_sys_map.
		* If this instance knows some SLAVE and it does not replies to the request then it remove it from sys_to_ip_map and ip_to_sys_map.
		*/

		general_mutex.lock();

		if(ip_to_sys_map.count(ip) != 0 && ip_to_sys_map[ip] == 0 && sys_to_ip_map[0] == ip) {
			// If the MASTER did not replied
			sys_to_ip_map.erase(0);
		}

		if(ip_to_sys_map.count(ip) != 0 && ip_to_sys_map[ip] > 0 && sys_to_ip_map[ip_to_sys_map[ip]] == ip) {
			// If the SLAVE did not replied
			sys_to_ip_map.erase(ip_to_sys_map[ip]);
		}

		ip_to_sys_map.erase(ip);

		general_mutex.unlock();
	}

	// If I am MASTER then the actions are (almost) the same as in FULLY distributed.
	if(local_ID == 0) {
#endif

		// If the instance has been discovered then map to a system with the lowest available sys_id.
		if (discovered) {
			// If the instance was already discovered in a previous discovery session.
			if (ip_to_sys_map.count(ip)){

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

				// Update system id
				int old_id = ip_to_sys_map[ip];
				general_mutex.lock();
				sys_to_ip_map.erase(old_id);
				sys_to_ip_map[reply.id] = ip;
				ip_to_sys_map[ip] = reply.id;
				general_mutex.unlock();

#endif

				return;
			}
			else {

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

				general_mutex.lock();
				sys_to_ip_map[reply.id] = ip;
				ip_to_sys_map[ip] = reply.id;
				general_mutex.unlock();

				logger->Debug("SLAVE tracked with id: %d", reply.id);
#else
#ifdef CONFIG_BBQUE_DIST_FULLY

				// Get the first available id and set it to the new instance.
				int id = 1;
				bool stop = false;
				while (!stop) {
					if (sys_to_ip_map.count(id) == 0) {
						general_mutex.lock();
						sys_to_ip_map[id] = ip;
						ip_to_sys_map[ip] = id;
						general_mutex.unlock();
						stop = true;
					}
					id++;
				}
#endif
#endif
			}
		}
		else {
			// If the instance is in the map variables then remove it from them.
			if (ip_to_sys_map.count(ip)) {
				general_mutex.lock();
				sys_to_ip_map.erase(ip_to_sys_map[ip]);
				ip_to_sys_map.erase(ip);
				general_mutex.unlock();
			}
		}

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

	}

#endif

	return;
}

void DistributedManager::DiscoverInstances() {
#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

	amIAlone = true;
	masterFound = false;

	if (local_ID == -1) {
		sys_to_ip_map.clear();
		ip_to_sys_map.clear();
	}

#endif

	for (std::vector<std::string>::iterator it = ipAddresses.begin(); it != ipAddresses.end(); ++it) {
		// Skip discover of myself
		if(*it == (local_IP)) continue;

		threads.push_back(std::thread(&DistributedManager::Discover, this, *it));
	}

	for (auto & th: threads)
		if (th.joinable())
			th.join();

	logger->Debug("DiscoverInstances: joined all threads");

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

	// If I am alone then I become the MASTER
	if(amIAlone){
		logger->Debug("I am alone: I become MASTER");

		local_ID = 0;

		sys_to_ip_map.clear();
		ip_to_sys_map.clear();

		sys_to_ip_map[0] = local_IP;
		ip_to_sys_map[local_IP] = 0;
	}
	else if(!masterFound && local_ID != 0) {
		logger->Debug("Master not found");

		// If I am a new instance then I skip this discover session in order to make the rest of instances set the MASTER
		if(local_ID == -1) {
			logger->Debug("Since I am a new instance I wait until a MASTER is established");

			return;
		}

		// MASTER becomes the instance with lower ID. The decision should be the same among instances.
		ip_to_sys_map.erase(sys_to_ip_map[0]);
		sys_to_ip_map.erase(0);

		logger->Debug("The system with lower ID becomes MASTER");

		// Now the system with the lower ID becomes MASTER
		int newMasterSystem = sys_to_ip_map.begin()->first;
		std::string newMasterIp = sys_to_ip_map[newMasterSystem];

		ip_to_sys_map[newMasterIp] = 0;
		sys_to_ip_map[0] = newMasterIp;

		// Delete the old system id of the instance that just became MASTER
		sys_to_ip_map.erase(newMasterSystem);

		if(newMasterSystem == local_ID) {
			logger->Debug("The lower ID is my ID: I become MASTER");

			// The new MASTER it's me!
			local_ID = 0;
		}
	}

	if(local_ID == 0) {
		logger->Debug("Checking which ID are freed up");

		// Loop assigned IDs to check with discovered instances which ID has been freed up.
		for(auto it = sys_to_ip_map.begin(); it != sys_to_ip_map.end(); ++it) {
			if(it->second == "") {
				sys_to_ip_map.erase(it->first);
			}
		}
	}

#endif
}

void DistributedManager::PrintSysToIp() {
	general_mutex.lock();
	for (auto it = sys_to_ip_map.begin(); it != sys_to_ip_map.end(); ++it) {
		logger->Debug("%d: %s\n", (it->first), (it->second).c_str());
	}
	general_mutex.unlock();
}

const double DistributedManager::calculateRTT(std::string const ip) {
	logger->Debug("Calculating instances RTT...");
	double ping_sum = 0;
	double pings = (PING_NUMBER * PING_CYCLES);
	double RTT;

	for(int index = 0; index < PING_NUMBER * PING_CYCLES; index++) {
		if(instance_private_stats_map[ip].last_pings[index] == 0 || instance_private_stats_map[ip].last_pings[index] == -1) {
			pings--;
			continue;
		}
		ping_sum += instance_private_stats_map[ip].last_pings[index];
	}

	RTT = ping_sum / pings;

	return RTT;
}

const double DistributedManager::calculateAvailability(std::string const ip) {
	logger->Debug("Calculating instances availability...");

	double dividend = 0;
	double divisor = 0;
	double availability;

	for(int index = 0; index < PING_NUMBER * PING_CYCLES; index++) {
		if(instance_private_stats_map[ip].last_pings[index] != 0) {
			divisor++;
		}
		if(instance_private_stats_map[ip].last_pings[index] > 0) {
			dividend++;
		}
	}

	availability = dividend / divisor;

	return availability;
}

void DistributedManager::Ping(std::string ip) {
	std::mutex m;
	std::condition_variable cv[PING_NUMBER];
	std::vector<std::thread> t;

	int ping_value[PING_NUMBER] = { 0 };
	bbque::agent::ExitCode_t result[PING_NUMBER] = { bbque::agent::ExitCode_t::AGENT_UNREACHABLE };

	PlatformManager & plm = PlatformManager::GetInstance();
	pp::RemotePlatformProxy* rpp = plm.GetRemotePlatformProxy();

	// Check if there is an entry for the ip in statistics. If not, then initialize one entry
	if(instance_private_stats_map.count(ip) == 0)
	{
		Instance_Private_Stats_t s;
		instance_private_stats_map[ip] = s;
	}

	// int temp_ping_sum = 0;
	bool at_least_one_ping_successfull = false;
	Instance_Public_Stats_t stats;

	// Ping PING_NUMBER times
	for(int i = 0; i < PING_NUMBER; i++)
	{
		logger->Debug("Sending Ping no %d to %s", i, ip.c_str());
		t.push_back(std::thread([&cv, &result, this, ip, &ping_value, rpp, i]()
		{
			result[i] = rpp->Ping(ip, ping_value[i]);
			cv[i].notify_one();
		}));

		t[i].detach();

		{
			std::unique_lock<std::mutex> l(m);
			if(cv[i].wait_for(l, std::chrono::seconds(2)) == std::cv_status::timeout) {
				logger->Debug("Ping no %d timeout", i);
				general_mutex.lock();
				instance_private_stats_map[ip].last_pings[instance_private_stats_map[ip].ping_pointer] = -1;
				instance_private_stats_map[ip].ping_pointer = (instance_private_stats_map[ip].ping_pointer + 1) % (PING_NUMBER * PING_CYCLES);
				general_mutex.unlock();
			}
			else if (ping_value[i] != 0)
			{
				logger->Debug("Pong no %d received", i);

				// If ping has not failed.

				at_least_one_ping_successfull = true;
				general_mutex.lock();
				instance_private_stats_map[ip].last_pings[instance_private_stats_map[ip].ping_pointer] = ping_value[i];
				instance_private_stats_map[ip].ping_pointer = (instance_private_stats_map[ip].ping_pointer + 1) % (PING_NUMBER * PING_CYCLES);
				general_mutex.unlock();
			}
			else {
				general_mutex.lock();
				instance_private_stats_map[ip].last_pings[instance_private_stats_map[ip].ping_pointer] = -1;
				instance_private_stats_map[ip].ping_pointer = (instance_private_stats_map[ip].ping_pointer + 1) % (PING_NUMBER * PING_CYCLES);
				general_mutex.unlock();
			}
		}
	}

	general_mutex.lock();
	stats.RTT = calculateRTT(ip);
	stats.availability = calculateAvailability(ip);
	instance_public_stats_map[ip] = stats;
	general_mutex.unlock();

	if(at_least_one_ping_successfull == false) {
		logger->Debug("Removing from discovered instances the instance that did not reply any ping");

		// Remove from discovered instances the instance that did not returned to any PING_NUMBER pings
		general_mutex.lock();
		slow_instances.insert(ip);
		general_mutex.unlock();
	}

	return;
}

void DistributedManager::PingInstances() {
	instance_public_stats_map.clear();
	slow_instances.clear();
	for (auto it = ip_to_sys_map.begin(); it != ip_to_sys_map.end(); ++it) {
		// Skip ping of myself
		if(it->first == (local_IP)) continue;

		threads.push_back(std::thread(&DistributedManager::Ping, this, it->first));
	}

	for (auto & th: threads)
		if (th.joinable())
			th.join();

	logger->Debug("PingInstances: joined all threads");
}

void DistributedManager::PrintStatusReport() {
	logger->Notice("Report on instances:");
	logger->Notice(DISM_DIV1);
	logger->Notice(DISM_HEAD);
	logger->Notice(DISM_DIV2);

	for (std::vector<std::string>::const_iterator it = ipAddresses.begin(); it != ipAddresses.end(); ++it) {
		char buffer[70];

		// If the iterator is at the local IP
		if(*it == local_IP)
		{
			sprintf(buffer, "| %21s | %3d |    -    |       -      |    MYSELF    |", (*it).c_str(), local_ID);
			logger->Notice(buffer);

			continue;
		}
#ifdef CONFIG_BBQUE_DIST_FULLY
		// If the instance at the current IP is available.
		if (ip_to_sys_map.find(*it) != ip_to_sys_map.end()) {
			if (slow_instances.find(*it) != slow_instances.end()) {
				// If the instance is marked as slow (it missed all the pings in the last ping cycle)

				sprintf(buffer, "| %21s | %3d | %7.2f |    %6.2f    |     SLOW     |", (*it).c_str(), ip_to_sys_map[*it], instance_public_stats_map[*it].RTT, instance_public_stats_map[*it].availability);
				logger->Notice(buffer);
			}
			else {
				sprintf(buffer, "| %21s | %3d | %7.2f |    %6.2f    |      OK      |", (*it).c_str(), ip_to_sys_map[*it], instance_public_stats_map[*it].RTT, instance_public_stats_map[*it].availability);
				logger->Notice(buffer);
			}
		}
		else {
			sprintf(buffer, "| %21s |  -  |    -    |       -      | DISCONNECTED |", (*it).c_str());
			logger->Notice(buffer);
		}
#else
#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL
		
		if (ip_to_sys_map.find(*it) != ip_to_sys_map.end()) {
			// If the instance at the current IP is available.

			if(local_ID == 0) {
				// If I am MASTER

				if (slow_instances.find(*it) != slow_instances.end()) {
					// If the instance is marked as slow (it missed all the pings in the last ping cycle)

					sprintf(buffer, "| %21s | %3d | %7.2f |    %6.2f    |     SLOW     |", (*it).c_str(), ip_to_sys_map[*it], instance_public_stats_map[*it].RTT, instance_public_stats_map[*it].availability);
					logger->Notice(buffer);
				}
				else {
					sprintf(buffer, "| %21s | %3d | %7.2f |    %6.2f    |      OK      |", (*it).c_str(), ip_to_sys_map[*it], instance_public_stats_map[*it].RTT, instance_public_stats_map[*it].availability);
					logger->Notice(buffer);
				}
			}
			else {
				sprintf(buffer, "| %21s | %3d |    -    |       -      |      OK      |", (*it).c_str(), ip_to_sys_map[*it]);
				logger->Notice(buffer);
			}
		}
		else {
			sprintf(buffer, "| %21s |  -  |    -    |       -      | DISCONNECTED |", (*it).c_str());
			logger->Notice(buffer);
		}
#endif
#endif
	}

	logger->Notice(DISM_DIV3);
	logger->Notice(DISM_DIV1);
}

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

int DistributedManager::GetNewID() {
	int id = 0;
	bool stop = false;

	// Find the first id (from 1) that is not found in sys_to_ip_map, insert into it with an empty string and then return it.
	do {
		id++;
		if (sys_to_ip_map.count(id) == 0) {
			sys_to_ip_map[id] = "";
			stop = true;
		}
	} while(!stop);

	return id;
}

#endif

bool DistributedManager::GetIPFromID(int16_t id, std::string & ip) {
	ip = sys_to_ip_map[id];

	return ip != "";
}

bool DistributedManager::GetIDFromIP(std::string ip, int16_t & id) {
	id = ip_to_sys_map[ip];

	return id != 0;
}

void DistributedManager::Task() {
	logger->Info("Distributed Manager monitoring thread STARTED");

	if(FindMyOwnIPAddresses() == false)
	{
		logger->Error("Distributed Manager didn't found any suitable local ip address");

		return;
	}

#ifdef CONFIG_BBQUE_DIST_FULLY

	local_ID = 0;
	sys_to_ip_map[0] = local_IP;
	ip_to_sys_map[local_IP] = 0;

#else
#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

	local_ID = -1;

#endif
#endif

	// Calculate the greatest common divider. The result will be used as seconds in the sleep function.
	boost::math::gcd_evaluator<int> gcd_eval;
	int gcd = gcd_eval(discover_period_s, ping_period_s);

	/*
	* Contains how many times discover and ping period are greater than gcd.
	* They are used to trigger discover and ping functions at specific awakening rate time of the loop.
	*/
	int times_discover_period = discover_period_s / gcd;
	int times_ping_period = ping_period_s / gcd;

	int discover = times_discover_period;
	int ping = times_ping_period;

	while (!done) {
		if(discover % times_discover_period == 0)
		{
			logger->Debug("Discovering instances...");

			discover = 0;
			DiscoverInstances();
		}

		if(ping % times_ping_period == 0)
		{
			logger->Debug("Ping instances...");

			ping = 0;

			// In hierarchical only the MASTER ping instances. In fully distributed every instance has id equal to 0, internally.
			if (local_ID == 0){
				PingInstances();
			}
		}

		discover++;
		ping++;

		PrintStatusReport();

		// std::this_thread::sleep_for(std::chrono::seconds(gcd));

		std::this_thread::sleep_for(std::chrono::seconds(0));
		int a;
		logger->Debug("------------------------------------");
		logger->Debug("------------------------------------");
		logger->Debug("------------------------------------");
		std::cin >> a;

	}

	logger->Info("Distributed Manager monitoring thread END");
}

bool DistributedManager::Configure() {

	if (this->configured)
		return true;

	ConfigurationManager & cm = ConfigurationManager::GetInstance();

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
#ifdef LOCAL_TEST

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

#else

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

	// Walk through linked list, maintaining head pointer so we can free list later
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

	freeifaddrs(ifaddr);

	return true;
}
} // namespace
