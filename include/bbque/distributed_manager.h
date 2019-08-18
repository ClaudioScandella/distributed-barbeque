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

// #define LOCAL_TEST

// CONFIG_BBQUE_DIST_FULLY
// CONFIG_BBQUE_DIST_HIERARCHICAL

namespace bbque {

class DistributedManager : public utils::Worker
{
public:

	struct Instance_Stats_t {
		double RTT;
		int availability;
	};

	/**
	 * @brief Get a reference to the paltform proxy
	 */
	static DistributedManager & GetInstance();
	
	virtual ~DistributedManager() {};

	/**
	 * @brief Show the system resources status
	 */
	void PrintStatusReport();

	inline std::vector<int> const & GetInstances() {
		return instance_set;
	}

	inline std::map<int, Instance_Stats_t> const & GetInstancesStats() {
		return instance_stats_map;
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

	int Ping(int system_id);

	/**
	 * @brief Read configuration variables and store them in the configuration variables
	 */
	bool Configure();

	/**
	 * @brief Build al the IP addresses in the range between start_address and end_address and store them in 
	 */
	void BuildIPAddresses();

	/**
	 * @brief Get the local IP addresses. If not found then return false
	 */
	bool FindMyOwnIPAddresses();

	bool getInterfacesIPs();

	/**
	 * @brief The thread main code
	 */
	virtual void Task() override final;

	std::unique_ptr<bu::Logger> logger;

	/** The discovered instances */
	std::vector<int> instance_set;

	/** All the known possibly running instances */
	std::vector<int> known_instance_set;

	/** Contains statistics for each discovered instance */
	std::map<int, Instance_Stats_t> instance_stats_map;

	/**
	 * @brief Pointer to agent proxy
	 */
	// std::unique_ptr<bbque::plugins::AgentProxyIF> agent_proxy;

	/**
	 * @brief Pointer to remote platform proxy
	 */
	// pp::RemotePlatformProxy const rpp;

	/**
	 * @brief Says if configure has been done or not
	 */
	bool configured;

	// Configuration variables
	std::string start_address;
	std::string end_address;
	uint16_t discover_period_s;
	uint16_t ping_period_s;
#ifdef LOCAL_TEST
	std::string port_num;
#endif

	/**
	 * @brief Contains the only IP address that is in the range  of addresses specified in configuration file
	 */
	std::string local_IP;

	// Contains all the possible IP addresses that an instance can have.
	std::vector<std::string> ipAddresses;

	/**
	 * @brief Contains all the local ip addresses get from all interfaces
	 */
	std::set<std::string> local_IP_addresses;

	/**
	 * @brief Mapping between system id and IP address of the instances of Barbeque
	 */
	std::map<int, std::string> instances_map;

	/**
	 * @brief Contains the IP address of the discovered instances. If an instance is no more discovered
	 * on successive discovering then it is removed from the set.
	 */
	std::set<std::string> discovered_instances;

	std::vector<std::thread> threads;

	std::mutex general_mutex;
};

}
#endif // BBQUE_DISTRIBUTED_MANAGER_H_
