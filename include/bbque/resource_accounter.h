/*
 * Copyright (C) 2012  Politecnico di Milano
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

#ifndef BBQUE_RESOURCE_ACCOUNTER_H_
#define BBQUE_RESOURCE_ACCOUNTER_H_

#include <set>

#include "bbque/resource_accounter_conf.h"
#include "bbque/configuration_manager.h"
#include "bbque/command_manager.h"

#include "bbque/res/resource_utils.h"
#include "bbque/res/resource_tree.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/utils/utility.h"
#include "bbque/cpp11/thread.h"
#include "bbque/cpp11/condition_variable.h"

#define RESOURCE_ACCOUNTER_NAMESPACE "bq.ra"

// Base path for sync session resource view token
#define SYNC_RVIEW_PATH "ra.sync."

// Max length for the resource view token string
#define TOKEN_PATH_MAX_LEN 30

// Prefix resource path used for Recipe validation
#define PREFIX_PATH "sys"

namespace ba = bbque::app;
namespace br = bbque::res;
namespace bu = bbque::utils;

namespace bbque {


/** Map of map of Usage descriptors. Key: Application UID*/
typedef std::map<AppUid_t, br::UsagesMapPtr_t> AppUsagesMap_t;
/** Shared pointer to a map of pair Application/Usages */
typedef std::shared_ptr<AppUsagesMap_t> AppUsagesMapPtr_t;
/** Map of AppUsagesMap_t having the resource state view token as key */
typedef std::map<br::RViewToken_t, AppUsagesMapPtr_t> AppUsagesViewsMap_t;
/** Set of pointers to the resources allocated under a given state view*/
typedef std::set<br::ResourcePtr_t> ResourceSet_t;
/** Shared pointer to ResourceSet_t */
typedef std::shared_ptr<ResourceSet_t> ResourceSetPtr_t;
/** Map of ResourcesSetPtr_t. The key is the view token */
typedef std::map<br::RViewToken_t, ResourceSetPtr_t> ResourceViewsMap_t;

// Forward declarations
class ApplicationManager;


/**
 * @brief Binding domain information
 *
 * Keep track of the runtime status of the binding domains (e.g., CPU
 * nodes)
 */
typedef struct BindingInfo {
	/** Base resource path object */
	br::ResourcePathPtr_t d_path;
	/** Number of managed resource types */
	std::list<br::Resource::Type_t> r_types;
	/** Resource pointer descriptor list */
	br::ResourcePtrList_t rsrcs;
	/** The IDs of all the possible bindings */
	std::vector<br::ResID_t> ids;
	/** Keep track the bindings without available processing elements */
	br::ResourceBitset full;
	/** Number of binding domains on the platform	 */
	uint16_t count;
} BindingInfo_t;


typedef std::pair<br::Resource::Type_t, BindingInfo_t *> BindingPair_t;
typedef std::map<br::Resource::Type_t, BindingInfo_t *> BindingMap_t;

/**
 * @brief Resources Accouter
 * @ingroup sec07_ra
 */
class ResourceAccounter: public ResourceAccounterConfIF, CommandHandler {

public:

	/**
	 * @brief The states of the module
	 */
	enum class State {
		/** Information not ready for query and accounting */
		NOT_READY,
		/** Information ready for query and accounting */
		READY,
		/** A synchronization step is in progress */
		SYNC
	};

	/**
	 * @brief Return the instance of the ResourceAccounter
	 */
	static ResourceAccounter & GetInstance();

	/**
	 * @brief Called when all the hardware resources have been registered and
	 * the platform is ready
	 */
	void SetPlatformReady();

	/**
	 * @brief Called when there are updates in the hardware resources and thus
	 * the platform cannot be considered ready
	 */
	void SetPlatformNotReady();

	/**
	 * @brief Destructor
	 */
	virtual ~ResourceAccounter();

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint32_t Total(std::string const & path) const;

	uint32_t Total(br::ResourcePtrList_t & rsrc_list) const;

	uint32_t Total(br::ResourcePathPtr_t ppath,	PathClass_t rpc = EXACT) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint32_t Available(
			std::string const & path,
			br::RViewToken_t vtok = 0,
			ba::AppSPtr_t papp = ba::AppSPtr_t()) const;

	uint32_t Available(
			br::ResourcePtrList_t & rsrc_list,
			br::RViewToken_t vtok = 0,
			ba::AppSPtr_t papp = ba::AppSPtr_t()) const;

	uint32_t Available(
			br::ResourcePathPtr_t ppath, PathClass_t rpc = EXACT,
			br::RViewToken_t vtok = 0,
			ba::AppSPtr_t papp = ba::AppSPtr_t()) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint32_t Used(std::string const & path, br::RViewToken_t vtok = 0) const;

	uint32_t Used(br::ResourcePtrList_t & rsrc_list, br::RViewToken_t vtok = 0) const;

	uint32_t Used(br::ResourcePathPtr_t ppath, PathClass_t rpc = EXACT,
			br::RViewToken_t vtok = 0) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint32_t Unreserved(std::string const & path) const;

	uint32_t Unreserved(br::ResourcePtrList_t & rsrc_list) const;

	uint32_t Unreserved(br::ResourcePathPtr_t ppath) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint16_t Count(br::ResourcePathPtr_t ppath) const;

	uint16_t CountPerType(br::ResourceIdentifier::Type_t type) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	inline uint16_t CountTypes() const {
		return r_count.size();
	}

	/**
	 * @see ResourceAccounterStatusIF
	 */
	inline std::list<br::Resource::Type_t> GetTypesList() const {
		return r_types;
	}

	/**
	 * @see ResourceAccounterStatusIF
	 */
	br::ResourcePtr_t GetResource(std::string const & path) const;

	br::ResourcePtr_t GetResource(br::ResourcePathPtr_t ppath) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	br::ResourcePtrList_t GetResources(std::string const & path) const;

	br::ResourcePtrList_t GetResources(br::ResourcePathPtr_t ppath) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	bool ExistResource(std::string const & path) const;

	bool ExistResource(br::ResourcePathPtr_t ppath) const;

	/**
	 * @brief Get the resource path object related to a string
	 *
	 * Given a resource path in char string format, the member function
	 * returns the related ResourcePath object. The string must be of "EXACT"
	 * type, meaning that it must reference a single resource
	 *
	 * @param path_str Resource path in char string format
	 *
	 * @return A shared pointer to a ResourcePath object
	 */
	br::ResourcePathPtr_t const GetPath(std::string const & path_str) const;

	/**
	 * @brief Get the cumulative amount of resource usage
	 *
	 * @param pum A map of Usage pointers
	 * @param r_type The type of resource to query
	 * @param r_scope_type The scope under which consider the resource
	 *
	 * @return The amount of resource usage
	 */
	uint32_t GetUsageAmount(br::UsagesMapPtr_t const & pum,
			br::ResourceIdentifier::Type_t r_type,
			br::ResourceIdentifier::Type_t r_scope_type = br::Resource::UNDEFINED) const;

	uint32_t GetUsageAmount(br::UsagesMap_t const & um,
			br::ResourceIdentifier::Type_t r_type,
			br::ResourceIdentifier::Type_t r_scope_type = br::Resource::UNDEFINED) const;

	/**
	 * @brief Show the system resources status
	 *
	 * This is an utility function for debug purpose that print out all the
	 * resources path and values about usage and total amount.
	 *
	 * @param vtok Token of the resources state view
	 * @param verbose print in INFO log level is true, while false in DEBUG
	 */
	void PrintStatusReport(br::RViewToken_t vtok = 0, bool verbose = false) const;

	/**
	 * @brief Print details about how resource usage is partitioned among
	 * applications/EXCs
	 *
	 * @param path The resource path
	 * @param vtok The token referencing the resource state view
	 * @param verbose print in INFO log level is true, while false in DEBUG
	 */
	void PrintAppDetails(br::ResourcePathPtr_t path, br::RViewToken_t vtok,
			bool verbose) const;

	/**
	 * @brief A prefix path for recipe validation
	 */
	br::ResourcePath const & GetPrefixPath() const;

	/**
	 * @brief Register a resource
	 *
	 * Setup informations about a resource installed into the system.
	 * Resources can be system-wide or placed on the platform. A resource is
	 * identified by its path, which also provides information about
	 * architectural hierarchies of the platform.
	 *
	 * @param path Resource path in char string format
	 * @param units Units for the amount value (i.e. "1", "Kbps", "Mb", ...)
	 * @param amount The total amount available
	 *
	 * @return RA_SUCCESS if the resource has been successfully registered.
	 * RA_ERR_MISS_PATH if the path string is empty. RA_ERR_MEM if the
	 * resource descriptor cannot be allocated.
	 */
	ExitCode_t RegisterResource(
			std::string const & path, std::string const & units, uint32_t amount);

	/**
	 * @brief Update availabilies for the specified resource
	 *
	 */
	ExitCode_t UpdateResource(
			std::string const & path, std::string const & units,uint32_t amount);

	/**
	 * @brief Book e a set of resources
	 *
	 * The method first check that the application doesn't hold another
	 * resource set, then check thier availability, and finally reserves, for
	 * each resource in the usages map specified, the required quantity.
	 *
	 * @param papp The application requiring resource usages
	 * @param rsrc_usages Map of Usage objects
	 * @param vtok The token referencing the resource state view
	 * @param do_check If true the controls upon set validity and resources
	 * availability are enabled
	 *
	 * @return RA_SUCCESS if the operation has been successfully performed.
	 * RA_ERR_MISS_APP if the application descriptor is null.
	 * RA_ERR_MISS_USAGES if the resource usages map is empty.
	 * RA_ERR_MISS_VIEW if the resource state view referenced by the given
	 * token cannot be retrieved.
	 * RA_ERR_USAGE_EXC if the resource set required is not completely
	 * available.
	 */
	ExitCode_t BookResources(
			ba::AppSPtr_t papp,
			br::UsagesMapPtr_t const & rsrc_usages,
			br::RViewToken_t vtok = 0);

	/**
	 * @brief Release the resources
	 *
	 * The method is typically called when an application stops running
	 * (exit or is killed) and releases all the resource usages.
	 * It lookups the current set of resource usages of the application and
	 * release it all.
	 *
	 * @param papp The application holding the resources
	 * @param vtok The token referencing the resource state view
	 */
	void ReleaseResources(ba::AppSPtr_t papp, br::RViewToken_t vtok = 0);


	/**
	 * @brief Define the amount of resource to be reserved
	 *
	 * A certain amount of resources could be dynamically reserved at
	 * run-time in order to avoid their allocation to demanding
	 * applications.
	 *
	 * Resources reservation could be conveniently used to both block
	 * access to resources temporarely not available as well as implement
	 * run-time resources management policies which, for examples, target
	 * thermal leveling issues.
	 *
	 * If a resource path template is specified, the specified amout of
	 * resource is reserved for each resource matching the template.
	 *
	 * @param path Resource path
	 * @param amount The amount of resource to reserve, 0 for note
	 *
	 * @return RA_SUCCESS if the reservation has been completed correctly,
	 * RA_FAILED otherwise.
	 */
	ExitCode_t  ReserveResources(br::ResourcePathPtr_t ppath, uint32_t amount);

	ExitCode_t  ReserveResources(std::string const & path, uint32_t amount);


	bool  IsOfflineResource(br::ResourcePathPtr_t ppath) const;

	ExitCode_t  OfflineResources(std::string const & path);

	ExitCode_t  OnlineResources(std::string const & path);


	/**
	 * @brief Check if resources are being reshuffled
	 *
	 * Resources reshuffling happens when two resources bindings are not
	 * the same, i.e. different kind or amount of resources.
	 *
	 * @param pum_current the "current" resources bindings
	 * @param pum_next the "next" resources bindings
	 *
	 * @return true when resources are being reshuffled
	 */
	bool IsReshuffling(
			br::UsagesMapPtr_t const & pum_current,
			br::UsagesMapPtr_t const & pum_next);

	/**
	 * @brief The resource binding information support
	 *
	 * @return A reference to a @ref BindingMap_t object
	 */
	inline BindingMap_t & GetBindingOptions() {
		return binding_options;
	}

	/**
	 * @see ResourceAccounterConfIF
	 */
	ExitCode_t GetView(std::string who_req, br::RViewToken_t & tok);

	/**
	 * @see ResourceAccounterConfIF
	 */
	void PutView(br::RViewToken_t tok);

	/**
	 * @brief Get the system resource state view
	 *
	 * @return The token of the system view
	 */
	inline br::RViewToken_t GetSystemView() {
		return sys_view_token;
	}

	/**
	 * @brief Get the scheduled resource state view
	 *
	 * @return The token of the scheduled view
	 */
	inline br::RViewToken_t GetScheduledView() {
		return sch_view_token;
	}

	/**
	 * @brief Set the scheduled resource state view
	 *
	 * @return The token of the system view
	 */
	void SetScheduledView(br::RViewToken_t svt);

	/**
	 * @brief Start a synchronized mode session
	 *
	 * Once a scheduling/resource allocation has been performed we need to
	 * make the changes effective, by updating the system resources state.
	 * For doing that a "synchronized mode session" must be started. This
	 * method open the session and init a new resource view by computing the
	 * resource accounting info of the Applications/ExC having a "RUNNING"
	 * scheduling state (the ones that continue running without
	 * reconfiguration or migrations).
	 *
	 * @return @see ExitCode_t
	 */
	ExitCode_t SyncStart();

	/**
	 * @brief Acquire resources for an Application/ExC
	 *
	 * If the sync session is not open does nothing. Otherwise it does
	 * resource booking using the state view allocated for the session.
	 * The resource set is retrieved from the "next AWM".
	 *
	 * @param papp Application/ExC acquiring the resources
	 *
	 * @return @see ExitCode_t
	 */
	ExitCode_t SyncAcquireResources(ba::AppSPtr_t const & papp);

	/**
	 * @brief Abort a synchronized mode session
	 *
	 * Changes are trashed away. The resource bookings performed in the
	 * session are canceled.
	 */
	void SyncAbort();

	/**
	 * @brief Commit a synchronized mode session
	 *
	 * Changes are made effective. Resources must be allocated accordingly to
	 * the state view built during in the session.
	 *
	 * @return @see ExitCode_t
	 */
	ExitCode_t SyncCommit();


	/**
	 * @see CommandHandler
	 */
	int CommandsCb(int argc, char *argv[]);

	/**
	 * @brief Handler for the shell command that allows to redefine the total
	 * amount of resource that can be allocated.
	 *
	 * @param r_path The resource path (char string)
	 * @value value  The new total amount
	 *
	 * @return 0 if success, a positive integer value otherwise
	 */
	int SetQuotaHandler(char * r_path, char * value);

private:

	/**
	 * @brief This is used for selecting the state attribute to return from
	 * QueryStatus
	 */
	enum QueryOption_t {
		/** Amount of resource available */
		RA_AVAIL = 0,
		/** Amount of resource used */
		RA_USED,
		/** Total amount of not reserved resource */
		RA_UNRESERVED,
		/** Total amount of resource */
		RA_TOTAL
	};

	/**
	 * @struct SyncSession_t
	 * @brief Store info about a synchronization session
	 */
	struct SyncSession_t {
		/** Token for the temporary resource view */
		br::RViewToken_t view;
		/** Count the number of session elapsed */
		uint32_t count;
	} sync_ssn;

	/** The logger used by the resource accounter */
	std::unique_ptr<bu::Logger> logger;

	/** The Application Manager component */
	bbque::ApplicationManager & am;

	/** The Command Manager component */
	CommandManager & cm;

	/** The Configuration Manager */
	ConfigurationManager & fm;


	/** Mutex protecting Resource Accounter status */
	std::mutex status_mtx;

	/** Conditional variable for status synchronization */
	std::condition_variable status_cv;

	/** This contain the status of the Resource Accounter */
	State status;


	/** The tree of all the resources in the system.*/
	br::ResourceTree resources;

	/** The resource paths registered (strings and objects) */
	std::map<std::string, br::ResourcePathPtr_t> r_paths;

	/** Resources that can be allocated in 'slice', i.e. the assigned amount
	 * is distributed over all the resources referenced by the mixed/template
	 * path specified */
	std::map<br::ResourcePathPtr_t, uint32_t> r_sliced;

	/** Counter for the total number of registered resources */
	std::map<br::Resource::Type_t, uint16_t> r_count;

	/** List that keeps track of the managed resource types */
	std::list<br::Resource::Type_t> r_types;

	/** Resource path (pointer) referencing the prefix */
	br::ResourcePathPtr_t r_prefix_path;

	/** Keep track of the max length between resources path string */
	uint8_t path_max_len = 0;

	/**
	 * A map object containing all the support information for the resource
	 * binding performed by the scheduling policy
	 */
	BindingMap_t binding_options;

	/**
	 * Map containing the pointers to the map of resource usages specified in
	 * the current working modes of each application. The key is the view
	 * token. For each view an application can hold just one set of resource
	 * usages.
	 */
	AppUsagesViewsMap_t usages_per_views;

	/**
	 * Keep track of the resources allocated for each view. This data
	 * structure is needed to supports easily a view deletion or to set a view
	 * as the new system state.
	 */
	ResourceViewsMap_t rsrc_per_views;

	/**
	 * Pointer (shared) to the map of applications resource usages, currently
	 * describing the resources system state (default view).
	 */
	AppUsagesMapPtr_t sys_usages_view;

	/**
	 * The token referencing the system resources state (default view).
	 */
	br::RViewToken_t sys_view_token;

	/**
	 * @brief Token referencing the view on scheduled resources
	 *
	 * When a new scheduling has been selected, this is the token referencing
	 * the corresponding view on resources state. When that schedule has been
	 * committed, i.e. resources usage synchronized, this has the same value
	 * of sys_view_token.
	 */
	br::RViewToken_t sch_view_token = 0;

	/**
	 * Default constructor
	 */
	ResourceAccounter();


	/**
	 * @brief Initialize the resource binding support information
	 */
	void InitBindingOptions();

	/**
	 * @brief Load the resource binding support information
	 *
	 * @note This can be done only when the status is READY
	 */
	void LoadBindingOptions();

	/**
	 * @brief Set the status to READY
	 */
	void SetReady();

	/**
	 * @brief Wrap the class of resource path on resource tree matching flags
	 *
	 * @param rpc The resource path class
	 *
	 * @return A resource tree matching flag
	 */
	inline uint16_t RTFlags(PathClass_t rpc) const {
		switch (rpc) {
		case EXACT:
			return RT_MATCH_FIRST;
		case MIXED:
			return RT_MATCH_MIXED;
		case TEMPLATE:
			return RT_MATCH_TYPE;
		default:
			return 0;
		}
	}


	/**
	 * @brief Thread unsafe version of @ref GetView
	 */
	ExitCode_t _GetView(std::string who_req, br::RViewToken_t & tok);

	/**
	 * @brief Thread unsafe version of @ref SetView
	 */
	br::RViewToken_t _SetView(br::RViewToken_t tok);

	/**
	 * @brief Thread unsafe version of @ref PutView
	 */
	void _PutView(br::RViewToken_t tok);


	/**
	 * @brief Get a list of resource descriptor
	 *
	 * The list is retrieved depending on the resource path class specified.
	 * If this is "undefined" it will be up to the function to understand the
	 * class of the resource path, by invoking function member GetResources.
	 * Otherwise, whenever the path class is given, the ResourceTree findList
	 * member function is called with the suitable matching flags.
	 *
	 * @param ppath The resource path referencing the resources
	 * @param rpc The token referencing the resource state view
	 *
	 * @return A list of pointers (shared) to resource descriptors
	 */
	br::ResourcePtrList_t GetList(
			br::ResourcePathPtr_t ppath,
			PathClass_t rpc = EXACT) const;

	/**
	 * @brief Return a state parameter (availability, resources used, total
	 * amount) for the resource.
	 *
	 * @param rsrc_list A list of descriptors of resources of the same type
	 * @param q_opt Resource state attribute requested (@see QueryOption_t)
	 * @param vtok The token referencing the resource state view
	 * @param papp The application interested in the query
	 *
	 * @return The value of the attribute request
	 */
	uint32_t QueryStatus(
			br::ResourcePtrList_t const & rsrc_list,
			QueryOption_t q_opt, br::RViewToken_t vtok = 0,
			ba::AppSPtr_t papp = ba::AppSPtr_t()) const;

	/**
	 * @brief Get the cumulative amount of resource usage, given iterators of
	 * a UsagesMap.
	 *
	 * This is an internal member function called by all the versions of the
	 * public member function GetUsageAmount().
	 *
	 * @param begin UsagesMap begin iterator
	 * @param end   UsagesMap end iterator
	 * @param r_type       The type of resource to query
	 * @param r_scope_type The scope under which consider the resource
	 *
	 * @return The amount of resource usage
	 */
	uint32_t GetAmountFromUsagesMap(
			br::UsagesMap_t::const_iterator & begin,
			br::UsagesMap_t::const_iterator & end,
			br::ResourceIdentifier::Type_t r_type,
			br::ResourceIdentifier::Type_t r_scope_type = br::Resource::UNDEFINED)
		const;

	/**
	 * @brief Check the resource availability for a whole set
	 *
	 * @param usages A map of Usage objects to check
	 * @param vtok The token referencing the resource state view
	 * @param papp The application interested in the query
	 * @return RA_SUCCESS if all the resources are availables,
	 * RA_ERR_USAGE_EXC otherwise.
	 */
	ExitCode_t CheckAvailability(
			br::UsagesMapPtr_t const & usages,
			br::RViewToken_t vtok = 0,
			ba::AppSPtr_t papp = ba::AppSPtr_t()) const;

	/**
	 * @brief Get a pointer to the map of applications resource usages
	 *
	 * Each application (or better, "execution context") can hold just one set
	 * of resource usages. It's the one defined through the working mode
	 * scheduled. Such assertion is valid inside the scope of the resources
	 * state view referenced by the token.
	 *
	 * @param vtok The token referencing the resource state view
	 * @param apps_usages The map of applications resource usages to get
	 * @return RA_SUCCESS if the map is found. RA_ERR_MISS_VIEW if the token
	 * doesn't match any state view.
	 */
	ExitCode_t GetAppUsagesByView(br::RViewToken_t vtok,
			AppUsagesMapPtr_t &	apps_usages);

	/**
	 * @brief Book e a set of resources (not thread-safe)
	 *
	 * The method reserves for each resource in the usages map specified the
	 * required quantity.
	 *
	 * @param papp The application requiring resource usages
	 * @param rsrc_usages Map of Usage objects
	 * @param vtok The token referencing the resource state view
	 * @param do_check If true the controls upon set validity and resources
	 * availability are enabled
	 *
	 * @return RA_SUCCESS if the operation has been successfully performed.
	 * RA_ERR_MISS_APP if the application descriptor is null.
	 * RA_ERR_MISS_USAGES if the resource usages map is empty.
	 * RA_ERR_MISS_VIEW if the resource state view referenced by the given
	 * token cannot be retrieved.
	 * RA_ERR_USAGE_EXC if the resource set required is not completely
	 * available.
	 */
	ExitCode_t _BookResources(
			ba::AppSPtr_t papp,
			br::UsagesMapPtr_t const & rsrc_usages,
			br::RViewToken_t vtok = 0);

	/**
	 * @brief Increment the resource usages counts
	 *
	 * Each time an application acquires a set of resources (specified in the
	 * working mode scheduled), the counts of resources used must be increased
	 *
	 * @param app_usages Map of next resource usages
	 * @param app The application acquiring the resources
	 * @param vtok The token referencing the resource state view
	 */
	ExitCode_t IncBookingCounts(
			br::UsagesMapPtr_t const & app_usages,
			ba::AppSPtr_t const & papp,
			br::RViewToken_t vtok = 0);

	/**
	 * @brief Book a single resource
	 *
	 * Divide the usage amount between the resources referenced in the "binds"
	 * list.
	 *
	 * @param papp The Application/ExC using the resource
	 * @param pusage Usage object
	 * @param vtok The token referencing the resource state view
	 *
	 * @return RA_ERR_USAGE_EXC if the usage required overcome the
	 * availability. RA_SUCCESS otherwise.
	 */
	ExitCode_t DoResourceBooking(
			ba::AppSPtr_t const & papp,
			br::UsagePtr_t & pusage,
			br::RViewToken_t vtok);

	/**
	 * @brief Release the resources
	 *
	 * This is the synchronization session mutex free verios of the dual
	 * public method ReleaseResources
	 *
	 * @see ReleaseResources
	 */
	void _ReleaseResources(ba::AppSPtr_t papp, br::RViewToken_t vtok = 0);

	/**
	 * @brief Allocate a quota of resource in the scheduling case
	 *
	 * Allocate a quota of the required resource in a single resource binding
	 * taken from the "binds" list of the Usage associated.
	 *
	 * @param papp The Application/ExC using the resource
	 * @param rsrc The resource descriptor of the resource binding
	 * @param requested The amount of resource required
	 * @param vtok The token referencing the resource state view
	 */
	void SchedResourceBooking(
			ba::AppSPtr_t const & papp,
			br::ResourcePtr_t & rsrc,
			uint32_t & requested,
			br::RViewToken_t vtok);

	/**
	 * @brief Allocate a quota of resource in the synchronization case
	 *
	 * Allocate a quota of the required resource in a single resource binding
	 * taken checking the assigments done by the scheduler. To retrieve this
	 * information, the scheduled view is properly queried.
	 *
	 * @param papp The Application/ExC using the resource
	 * @param rsrc The resource descriptor of the resource binding
	 * @param requested The amount of resource required
	 */
	void SyncResourceBooking(
			ba::AppSPtr_t const & papp,
			br::ResourcePtr_t & rsrc,
			uint32_t & requested);

	/**
	 * @brief Decrement the resource usages counts
	 *
	 * Each time an application releases a set of resources the counts of
	 * resources used must be decreased.
	 *
	 * @param app_usages Map of current resource usages
	 * @param app The application releasing the resources
	 * @param vtok The token referencing the resource state view
	 */
	void DecBookingCounts(
			br::UsagesMapPtr_t const & app_usages,
			ba::AppSPtr_t const & app,
			br::RViewToken_t vtok = 0);

	/**
	 * @brief Unbook a single resource
	 *
	 * Remove the amount of usage from the resources referenced in the "binds"
	 * list.
	 *
	 * @param papp The Application/ExC using the resource
	 * @param pusage Usage object
	 * @param vtok The token referencing the resource state view
	 */
	void UndoResourceBooking(
			ba::AppSPtr_t const & papp,
			br::UsagePtr_t & pusage,
			br::RViewToken_t vtok);

	/**
	 * @brief Init the synchronized mode session
	 *
	 * This inititalizes the sync session view by adding the resource usages
	 * of the RUNNING Applications/ExC. Thus the ones that will not be
	 * reconfigured or migrated.
	 *
	 * @return RA_ERR_SYNC_INIT if the something goes wrong in the assignment
	 * resources to the previuosly running applications/EXC.
	 */
	ExitCode_t SyncInit();

	/**
	 * @brief Clean closure of a synchronization session
	 */
	ExitCode_t SyncFinalize();

	/**
	 * @brief Abort a synchronized mode session
	 *
	 * This is the synchronization session mutex free verios of the dual
	 * public method SyncAbort
	 *
	 * @see SyncAbort
	 */
	void _SyncAbort();

	/**
	 * @brief Thread-safe checking of sychronization step in progress
	 *
	 * @return true if the synchronization of the resource accounting is in
	 * progress, false otherwise
	 */
	inline bool Synching() {
		return (status == State::SYNC);
	}

	/**
	 * @brief Set a view as the new resources state of the system
	 *
	 * Set a new system state view means that for each resource used in that
	 * view, such view becomes the default one.
	 * This is called once a new scheduling has performed, Applications/ExC
	 * must be syncronized, and thus system resources state  must be update
	 *
	 * @param tok The token used as reference to the resources view.
	 * @return The token referencing the system state view.
	 */
	br::RViewToken_t SetView(br::RViewToken_t vtok);

};

}   // namespace bbque

#endif // BBQUE_RESOURCE_ACCOUNTER_H_
