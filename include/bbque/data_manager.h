/*
 * Copyright (C) 2018  Politecnico di Milano
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BBQUE_DATA_MANAGER_H_
#define BBQUE_DATA_MANAGER_H_

#include "bbque/utils/worker.h"
#include "bbque/application_manager.h"
#include "bbque/configuration_manager.h"
#include "bbque/resource_accounter.h"
#include "dci/types.h"

#include <atomic>
#include <list>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

// Boost libraries
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/asio.hpp>

namespace bu = bbque::utils;
namespace br = bbque::res;

using namespace bbque::stat;

#define MAX_SUB_COMM_FAILURE 5

#define BBQUE_DM_ARCHIVE_PREFIX  BBQUE_PATH_PREFIX"/var/dm_archive_"

namespace bbque {
namespace data {

using sub_bitset_t = std::bitset<8>;

/**
 * @class Subscription
 * @brief Subscription objects for data requests
 */
class Subscription {
public:
	Subscription(){}

	Subscription(
			sub_bitset_t filter_bit,
			sub_bitset_t event_bit,
			uint16_t period_ms = -1):
			filter(filter_bit), 
			event(event_bit), 
			period_ms(period_ms) {}

	sub_bitset_t filter;     /// Type of information to subscribe
	sub_bitset_t event;      /// Type of events for which receive updates
	uint16_t     period_ms;  /// Update rate (milliseconds)

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		(void) version;
		ar & filter;
		ar & event;
		ar & period_ms;
	}
};

/**
 * @class Subscriber
 * @brief Object keeping track of the information of a subscriber
 */
class Subscriber {
public:
	Subscriber(){}

	Subscriber(std::string ip, uint32_t port, Subscription & sub):
			ip_address(ip), port_num(port), subscription(sub) {
		this->period_deadline_ms = sub.period_ms;
	}

	std::string  ip_address;          /// Client IP address
	uint32_t     port_num;            /// Client port number
	Subscription subscription;        /// Information subscribed data
	int16_t      period_deadline_ms;  /// Milliseconds before next update
	uint16_t     comm_failures = 0;   /// Number of communication failures

	bool cmp(Subscriber & s1, Subscriber & s2) {
		if(s1.period_deadline_ms < s2.period_deadline_ms)
			return true;
		return false;
	}

	bool operator==(const Subscriber &other) const{
		if (this == &other)
			return true;
		if (ip_address == other.ip_address && port_num == other.port_num)
			return true;
		return false;
	}

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		(void) version;
		ar & ip_address;
		ar & port_num;
		ar & subscription;
		ar & period_deadline_ms;
		ar & comm_failures;
	}
};

} // namespace data

using namespace bbque::data;
using namespace bbque::stat;

using SubscriberPtr_t     = boost::shared_ptr<Subscriber>;
using SubscriberPtrList_t = std::list<SubscriberPtr_t>;
using sockaddr_in_t       = struct sockaddr_in;
using SubscriberListIt_t  = std::list<SubscriberPtr_t>::iterator;

/**
 * @class DataManager
 * @brief This component manages the subscriptions coming from the Data
 * Communication Interface for sending data to external actors
 */
class DataManager: public utils::Worker {

public:
	/**
	 * @enum ExitCode_t
	 * @brief Class specific return codes
	 */
	enum ExitCode_t {
		OK = 0,             /** Successful call */
		ERR_CLIENT_COMM,    /** The client is unreachable */
		ERR_CLIENT_TIMEOUT, /** The communication timeout is elapsed, the client is removed*/
		ERR_UNKNOWN         /** A not specified error code   */
	};

	DataManager();

	virtual ~DataManager();

	/**
	 * @brief Return a reference to the DataManager module
	 * @return a reference to the DataManger module
	 */
	static DataManager & GetInstance();

	/**
	 * @brief Notify an update event to the DataManager
	 * @param event: the notified event
	 */
	void NotifyUpdate(status_event_t event);

private:

	/// The logger used by the application manager
	std::unique_ptr<bu::Logger> logger;

	/// Configuration manager instance
	ConfigurationManager & cfm;

	/// Resource Accounter instance
	ResourceAccounter & ra;

	/// Application Manager instance
	ApplicationManager & am;

	/// Signal the on going termination of the manager
	std::atomic<bool> is_terminating;

	/// Server thread managing incoming subscription requests
	std::thread subscription_server;

	/// Condition variable to signal the presence of subscribers
	std::condition_variable subs_cv;

	/// Time interval to sleep, corrected from the push delay
	uint16_t sleep_time;

	/// Time interval between different data publishing
	uint16_t actual_sleep_time;

	/// List of rate-based subscribers
	SubscriberPtrList_t subscribers_on_rate;

	/// List of event-based subscribers
	SubscriberPtrList_t subscribers_on_event;

	/// Mutex to protect concurrent access to the subscribers lists.
	std::mutex subscribers_mtx;

	/// DM archive paths for subscribers
	std::string archive_path_rate;
	std::string archive_path_event;

	/// The thread ID of the subscription server thread
	pid_t subscription_server_tid;

	/// Maximum communication attempts per client
	uint16_t max_client_attempts;

	/// list of all application updated status structs
	std::list<app_status_t> app_stats;

	/// list of all resources updated status structs
	std::list<resource_status_t> res_stats;

	/// The number of managed resources
	uint32_t num_resources;

	/// The number of current managed applications
	uint32_t num_applications;

	/// Mutex to protect concurrent access to the event map
	std::mutex events_mtx;

	/// Condition variable to signal event map changes
	std::condition_variable event_cv;

	/// This list allow to track all the events occurred and not served
	std::list<status_event_t> event_queue;

	/// This list allow to track all the events occurred and not served
	std::thread event_handler;

	/// Thread ID of the event handler thread
	pid_t event_handler_tid;

	/**
	 * @brief Utility method to print all the current subscribers
	 */
	void PrintSubscribers();

	/**
	 * @brief Utility method to store all the current subscribers
	 * on a local file
	 */
	void CheckpointSubscribers();

	/**
	 * @brief Utility method to load all the stored subscribers
	 */
	void RestoreSubscribers();

	/**
	 * @brief Utility method to build the resource bitset given a resource_path
	 * @param resource_path The resource path to convert
	 */
	res_bitset_t BuildResourceBitset(br::ResourcePathPtr_t resource_path);

	/**
	 * @brief Utility method to find the assigned resource path of a 
	  * specific task.
	 *
	 * @param res_map_ptr The pointer to the resource assigned map
	 * @param task 
	 *
	 */
	std::string FindResourceAssigned(
		res::ResourceAssignmentMapPtr_t res_map_ptr, 
		int mapped_sys,
		int mapped_cluster,
		int mapped_proc) const;

	/**
	 * @brief Utility method to find a subscriber into a specific list.
	 * The search is performed on the ip address.
	 *
	 * @param subscr The subscriber to search
	 * @param list The list in which to find the specified subscriber
	 * @return An iterator object pointing the found subscriber
	 */
	SubscriberPtr_t FindSubscriber(
			SubscriberPtr_t & subscr, SubscriberPtrList_t & list);

	/*******************************************************************/
	/*                      Communication fields                       */
	/*******************************************************************/

	/// Server port
	uint32_t server_port;

	/// Server IP address of server
	std::string ip_address = "0.0.0.0";

	/*******************************************************************/
	/*                    Publish/Subscribe methods                    */
	/*******************************************************************/

	/**
	 * @brief Publish the data to the @ref{subscribers_on_rate}
	 */
	void PublishOnRate();

	/**
	 * @brief Add a subscription
	 * @param subscr: the subscriber to add
	 * @param event: true if the subscription refers to an event, false otherwise
	 */
	void Subscribe(SubscriberPtr_t subscr, bool event);

	/**
	 * @brief Remove a subscription
	 */
	void Unsubscribe(SubscriberPtr_t subscr, bool event);

	/**
	 * @brief Publish the data to the @ref{subscribers_on_event}
	 * @param event The event triggering the publication
	 */
	void PublishOnEvent(status_event_t event);

	/**
	 * @brief Push the update to the specified subscriber based on its
	 * information filter
	 * @param sub The target subscriber
	 */
	ExitCode_t Push(SubscriberPtr_t sub);

	/**
	 * @brief Update the content data information
	 */
	void UpdateData();

	/**
	 * @brief Update the content of resources data information
	 */
	void UpdateResourcesData();

	/**
	 * @brief Update the content of applications data information
	 */
	void UpdateApplicationsData();

	/**
	 * @brief Server to handle the subscription/unsubscription requests
	 */
	void SubscriptionHandler();

	/**
	 * @brief Handles the event queue for updates publication
	 */
	void EventHandler();


	void Task();

	void _PreTerminate();

	void _PostTerminate();

};

} // namespace bbque

#endif // BBQUE_DATA_MANAGER_H_
