#ifndef BBQUE_DISTRIBUTED_MANAGER_H_
#define BBQUE_DISTRIBUTED_MANAGER_H_

#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <iostream>
#include <functional>
#include <limits>

#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#include "bbque/plugins/agent_proxy_if.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/utils/worker.h"
#include "bbque/pp/remote_platform_proxy.h"
#include "bbque/platform_manager.h"

#include "boost/math/common_factor_rt.hpp"

#define DISTRIBUTED_MANAGER_NAMESPACE "bq.dism"

#define LOCAL_TEST
#define DEBUG2

// Number of ping per time for each discovered instance (see example)
#define PING_NUMBER 3

// Number of ping cycles to which calculate mean RTT and avaialability (see example)
#define PING_CYCLES 3

/*

Example:

	If PING_NUMBER = 3 and PING_CYCLES = 5 then the instance ping 3 times in a row an instance (this is 1 cycle). For the successive
	4 cycles the old pings are kept and used along with the 3 new ones to calculate the RTT. At the 6th cycle the 3 values of the first cycle
	are overwritten with the 3 new ones. And so on...

*/

namespace bbque {

class DistributedManager : public utils::Worker
{
public:

	struct Instance_Public_Stats_t {
		double RTT;
		double availability;
	};

	/**
	 * @brief Get a reference to the paltform proxy
	 */
	static DistributedManager & GetInstance();
	
	virtual ~DistributedManager() {};

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL

	/**
	 * @brief Get the first available id and set it to the new instance
	 */
	int GetNewID();

#endif

	bool GetIPFromID(int16_t id, std::string & ip);

	bool GetIDFromIP(std::string ip, int16_t & id);

	inline std::map<int, std::string> const & GetInstancesID() {
		general_mutex.lock();
		return sys_to_ip_map;
		general_mutex.unlock();
	}

	inline std::map<std::string, Instance_Public_Stats_t> const & GetInstancesStats() {
		general_mutex.lock();
		return instance_public_stats_map;
		general_mutex.unlock();
	}

	inline std::set<std::string> const & GetSlowInstances() {
		general_mutex.lock();
		return slow_instances;
		general_mutex.unlock();
	}

	inline int const & GetLocalID() {
		return local_ID;
	}

private:

	struct Instance_Private_Stats_t {
		int last_pings[PING_NUMBER * PING_CYCLES] = { 0 };
		int ping_pointer = 0;
	};

	/**
	 * @brief   Build a new instance of the distributed manager
	 */
	DistributedManager();

	/**
	 * @brief Check all the available instances
	 */
	void DiscoverInstances();

	/**
	 * @brief Ping all the available instances
	 */
	void PingInstances();

	void Discover(std::string ip);

	void Ping(std::string ip);

	/**
	 * @brief Read configuration variables and store them in the configuration variables
	 */
	bool Configure();

	/**
	 * @brief Build al the IP addresses in the range between start_address and end_address and store them in 
	 */
	void BuildIPAddresses();

	/**
	 * @brief Get the local IP address that fit into the range of ip in configuration file
	 */
	bool FindMyOwnIPAddresses();

	/**
	 * @brief Get the local IP addresses. If not found then return false
	 */
	bool getInterfacesIPs();

	/**
	 * @brief Calculate the mean RTT (mean ping value) from the ping values in instance_private_stats_map
	 */
	const double calculateRTT(std::string const ip);

	/**
	 * @brief Calculate the availability from the ping values in instance_private_stats_map
	 */
	const double calculateAvailability(std::string const ip);

	/**
	 * @brief Show the system resources status
	 */
	void PrintStatusReport();

	/**
	 * @brief The thread main code
	 */
	virtual void Task() override final;

	std::unique_ptr<bu::Logger> logger;

	/**
	 * @brief Mark that configure has been done or not
	 */
	bool configured;

	// Configuration variables from configuration file
	std::string start_address;
	std::string end_address;
	uint16_t discover_period_s;
	uint16_t ping_period_s;
#ifdef LOCAL_TEST
	std::string port_num;
#endif

	/**
	 * @brief Contains the only local IP address that is in the range of addresses specified in configuration file
	 */
	std::string local_IP;

	/**
	 * @brief Contains the ID assigned to this instance. When this variable is 0 then this instance is the master in a hierarchical system.
	 	Possible values:
	 		-1 - when the instance is new so it does not know if it will be a master or a slave;
	 		 0 - when the instance is the master;
	 		>0 - when the instance is a slave.
	 */
	int local_ID;

	/**
	 * @brief Contains all the possible IP addresses in the range of IP as specified in configuration file
	 */
	std::vector<std::string> ipAddresses;

	/**
	 * @brief Contains all the local ip addresses get from all interfaces
	 */
	std::set<std::string> local_IP_addresses;

	/**
	 * @brief Mapping between system id and IP address of Barbeque instances
	 */
	std::map<int, std::string> sys_to_ip_map;
	std::map<std::string, int> ip_to_sys_map;

	// Contains all the intances that during a ping cycle did not respond to any ping.
	// The instances in this set are not removed from sys_to_ip_map and ip_to_sys_map as it happens when they are not discovered
	// but they are just tagged as slow.
	// In the report in the console they are not shown. This set can be inspected from outside distributed manager.
	std::set<std::string> slow_instances;

#ifdef CONFIG_BBQUE_DIST_HIERARCHICAL
	/**
	 * @brief It is used at every discover session to check if at least one instance replied to discover request.
	 */
	bool amIAlone;

	/**
	 * @brief It is used at every discover session to check if the MASTER replied to discover request.
	 */
	bool masterFound;
#endif

	/**
	 * @brief Contains mean RTT and availability statistics for each discovered instance
	 */
	std::map<std::string, Instance_Public_Stats_t> instance_public_stats_map;

	/**
	 * @brief Contains last pings values (along with its pointer) for each discovered instance
	 */
	std::map<std::string, Instance_Private_Stats_t> instance_private_stats_map;

	std::vector<std::thread> threads;

	std::mutex general_mutex;

#ifdef DEBUG
	std::mutex thread_debug_mutex;

	void PrintSysToIp();
#endif
};

}
#endif // BBQUE_DISTRIBUTED_MANAGER_H_
