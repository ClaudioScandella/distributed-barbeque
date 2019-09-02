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

#define PING_NUMBER 3

namespace bbque {

class DistributedManager : public utils::Worker
{
public:

	struct Instance_Stats_t {
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

	inline std::map<int, std::string> const & GetInstancesID() {
		return sys_to_ip_map;
	}

	inline std::map<int, Instance_Stats_t> const & GetInstancesStats() {
		return instance_stats_map;
	}

	inline int const & GetLocalID() {
		return local_ID;
	}

private:

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
	 * @brief Contains statistics for each discovered instance
	 */
	std::map<int, Instance_Stats_t> instance_stats_map;

	std::vector<std::thread> threads;

	std::mutex general_mutex;

#ifdef DEBUG
	std::mutex thread_debug_mutex;

	void PrintSysToIp();
#endif
};

}
#endif // BBQUE_DISTRIBUTED_MANAGER_H_
