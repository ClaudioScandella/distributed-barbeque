#ifndef BBQUE_REMOTE_PLATFORM_PROXY_H
#define BBQUE_REMOTE_PLATFORM_PROXY_H


#include "bbque/platform_proxy.h"
#include "bbque/plugins/agent_proxy_if.h"

#define REMOTE_PLATFORM_PROXY_NAMESPACE "bb.pp.rpp"

namespace bbque {
namespace pp {

class RemotePlatformProxy : public PlatformProxy, public bbque::plugins::AgentProxyIF
{
public:

	RemotePlatformProxy();

	virtual ~RemotePlatformProxy() {}

	/**
	 * @brief Return the Platform specific string identifier
	 */
	virtual const char* GetPlatformID(int16_t system_id=-1) const;

	/**
	 * @brief Return the Hardware identifier string
	 */
	virtual const char* GetHardwareID(int16_t system_id=-1) const;
	/**
	 * @brief Platform specific resource setup interface.
	 */
	virtual ExitCode_t Setup(SchedPtr_t papp);

	/**
	 * @brief Platform specific resources enumeration
	 *
	 * The default implementation of this method loads the TPD, is such a
	 * function has been enabled
	 */
	virtual ExitCode_t LoadPlatformData();

	/**
	 * @brief Platform specific resources refresh
	 */
	virtual ExitCode_t Refresh();

	/**
	 * @brief Platform specific resources release interface.
	 */
	virtual ExitCode_t Release(SchedPtr_t papp);

	/**
	 * @brief Platform specific resource claiming interface.
	 */
	virtual ExitCode_t ReclaimResources(SchedPtr_t papp);

	/**
	 * @brief Platform specific resource binding interface.
	 */
	virtual ExitCode_t MapResources(
		SchedPtr_t papp, ResourceAssignmentMapPtr_t pres, bool excl = true);

	virtual bool IsHighPerformance(bbque::res::ResourcePathPtr_t const & path) const;

	/**
	 * @brief Platform specific termination.
	 */
	virtual void Exit();

	//--- AgentProxy functions

	void StartServer();

	void StopServer();

	void WaitForServerToStop();
	
	
	bbque::agent::ExitCode_t Discover(
		std::string ip, bbque::agent::DiscoverRequest& iam);
		
	bbque::agent::ExitCode_t Ping(
		std::string ip, int & ping_value);

	bbque::agent::ExitCode_t GetResourceStatus(
		std::string const & resource_path, agent::ResourceStatus & status);


	bbque::agent::ExitCode_t GetWorkloadStatus(
		std::string const & system_path, agent::WorkloadStatus & status);

	bbque::agent::ExitCode_t GetWorkloadStatus(
		int system_id, agent::WorkloadStatus & status);


	bbque::agent::ExitCode_t GetChannelStatus(
		std::string const & system_path, agent::ChannelStatus & status);

	bbque::agent::ExitCode_t GetChannelStatus(
		int system_id, agent::ChannelStatus & status);


	bbque::agent::ExitCode_t SendJoinRequest(std::string const & system_path);

	bbque::agent::ExitCode_t SendJoinRequest(int system_id);


	bbque::agent::ExitCode_t SendDisjoinRequest(std::string const & system_path);

	bbque::agent::ExitCode_t SendDisjoinRequest(int system_id);


	bbque::agent::ExitCode_t SendScheduleRequest(
		std::string const & system_path,
		agent::ApplicationScheduleRequest const & request) ;

	RemotePlatformProxy operator=(RemotePlatformProxy other);

private:
	/**
	 * @brief The logger used by the worker thread
	 */
	std::unique_ptr<bu::Logger> logger;

	std::unique_ptr<bbque::plugins::AgentProxyIF> agent_proxy;

	ExitCode_t LoadAgentProxy();

};
}   // namespace pp
}   // namespace bbque

#endif // BBQUE_REMOTE_PLATFORM_PROXY_H
