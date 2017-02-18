#ifndef BBQUE_MANGO_PLATFORM_PROXY_H
#define BBQUE_MANGO_PLATFORM_PROXY_H

#include "bbque/config.h"
#include "bbque/platform_proxy.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/pp/mango_platform_description.h"

#define MANGO_PP_NAMESPACE "bq.pp.mango"


namespace bbque {
namespace pp {



class MangoPlatformProxy : public PlatformProxy
{
public:

	static MangoPlatformProxy * GetInstance();

	virtual ~MangoPlatformProxy();

	/**
	 * @brief Return the Platform specific string identifier
	 */
	const char* GetPlatformID(int16_t system_id=-1) const noexcept override final;

	/**
	 * @brief Return the Hardware identifier string
	 */
	const char* GetHardwareID(int16_t system_id=-1) const noexcept override final;

	/**
	 * @brief Platform specific resource setup interface.
	 */
	ExitCode_t Setup(AppPtr_t papp) noexcept override final;

	/**
	 * @brief Platform specific resources enumeration
	 *
	 * The default implementation of this method loads the TPD, is such a
	 * function has been enabled
	 */
	ExitCode_t LoadPlatformData() noexcept override final;

	/**
	 * @brief Platform specific resources refresh
	 */
	ExitCode_t Refresh() noexcept override final;

	/**
	 * @brief Platform specific resources release interface.
	 */
	ExitCode_t Release(AppPtr_t papp) noexcept override final;

	/**
	 * @brief Platform specific resource claiming interface.
	 */
	ExitCode_t ReclaimResources(AppPtr_t papp) noexcept override final;

	/**
	 * @brief Platform specific resource binding interface.
	 */
	ExitCode_t MapResources(
	        AppPtr_t papp, ResourceAssignmentMapPtr_t pres, bool excl) noexcept override final;


	bool IsHighPerformance(bbque::res::ResourcePathPtr_t const & path) const override;


	

private:
//-------------------- CONSTS

//-------------------- ATTRIBUTES

	std::unique_ptr<bu::Logger> logger;
	bool refreshMode;

	int architecture_id = -1;

//-------------------- METHODS

	MangoPlatformProxy();

};

}   // namespace pp
}   // namespace bbque

#endif // MANGO_PLATFORM_PROXY_H
