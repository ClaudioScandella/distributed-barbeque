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
#include "bbque/command_manager.h"

#include "bbque/res/resource_utils.h"
#include "bbque/res/resource_tree.h"
#include "bbque/plugins/logger.h"
#include "bbque/utils/utility.h"
#include "bbque/cpp11/thread.h"

#define RESOURCE_ACCOUNTER_NAMESPACE "bq.ra"

// Base path for sync session resource view token
#define SYNC_RVIEW_PATH "ra.sync."

// Max length for the resource view token string
#define TOKEN_PATH_MAX_LEN 30

using bbque::plugins::LoggerIF;
using bbque::app::AppSPtr_t;


namespace bbque {


/** Map of map of Usage descriptors. Key: Application UID*/
typedef std::map<AppUid_t, UsagesMapPtr_t> AppUsagesMap_t;
/** Shared pointer to a map of pair Application/Usages */
typedef std::shared_ptr<AppUsagesMap_t> AppUsagesMapPtr_t;
/** Map of AppUsagesMap_t having the resource state view token as key */
typedef std::map<RViewToken_t, AppUsagesMapPtr_t> AppUsagesViewsMap_t;
/** Set of pointers to the resources allocated under a given state view*/
typedef std::set<ResourcePtr_t> ResourceSet_t;
/** Shared pointer to ResourceSet_t */
typedef std::shared_ptr<ResourceSet_t> ResourceSetPtr_t;
/** Map of ResourcesSetPtr_t. The key is the view token */
typedef std::map<RViewToken_t, ResourceSetPtr_t> ResourceViewsMap_t;

// Forward declarations
class ApplicationManager;


/**
 * @brief Resources Accouter
 * @ingroup sec07_ra
 */
class ResourceAccounter: public ResourceAccounterConfIF, CommandHandler {

public:

	/**
	 * @brief Return the instance of the ResourceAccounter
	 */
	static ResourceAccounter & GetInstance();

	/**
	 * @brief Destructor
	 */
	virtual ~ResourceAccounter();

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint64_t Total(std::string const & path) const;

	uint64_t Total(ResourcePtrList_t & rsrc_list) const;

	uint64_t Total(ResourcePathPtr_t ppath,	PathClass_t rpc = EXACT) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint64_t Available(std::string const & path,
			RViewToken_t vtok = 0, AppSPtr_t papp = AppSPtr_t()) const;

	uint64_t Available(ResourcePtrList_t & rsrc_list,
			RViewToken_t vtok = 0, AppSPtr_t papp = AppSPtr_t()) const;

	uint64_t Available(ResourcePathPtr_t ppath, PathClass_t rpc = EXACT,
			RViewToken_t vtok = 0, AppSPtr_t papp = AppSPtr_t()) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint64_t Used(std::string const & path, RViewToken_t vtok = 0) const;

	uint64_t Used(ResourcePtrList_t & rsrc_list, RViewToken_t vtok = 0) const;

	uint64_t Used(ResourcePathPtr_t ppath, PathClass_t rpc = EXACT,
			RViewToken_t vtok = 0) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint64_t Unreserved(std::string const & path) const;

	uint64_t Unreserved(ResourcePtrList_t & rsrc_list) const;

	uint64_t Unreserved(ResourcePathPtr_t ppath) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	uint16_t Count(ResourcePathPtr_t ppath) const;

	uint16_t CountPerType(ResourceIdentifier::Type_t type) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	inline uint16_t CountTypes() const {
		return r_count.size();
	}

	/**
	 * @see ResourceAccounterStatusIF
	 */
	inline std::list<Resource::Type_t> GetTypesList() const {
		return r_types;
	}

	/**
	 * @see ResourceAccounterStatusIF
	 */
	ResourcePtr_t GetResource(std::string const & path) const;

	ResourcePtr_t GetResource(ResourcePathPtr_t ppath) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	ResourcePtrList_t GetResources(std::string const & path) const;

	ResourcePtrList_t GetResources(ResourcePathPtr_t ppath) const;

	/**
	 * @see ResourceAccounterStatusIF
	 */
	bool ExistResource(std::string const & path) const;

	bool ExistResource(ResourcePathPtr_t ppath) const;

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
	ResourcePathPtr_t const GetPath(std::string const & path_str) const;

	/**
	 * @brief Get the cumulative amount of resource usage
	 *
	 * @param pum A map of Usage pointers
	 * @param r_type The type of resource to query
	 *
	 * @return The amount of resource usage
	 */
	uint64_t GetUsageAmount(UsagesMapPtr_t const & pum,
			ResourceIdentifier::Type_t r_type) const;

	/**
	 * @brief Show the system resources status
	 *
	 * This is an utility function for debug purpose that print out all the
	 * resources path and values about usage and total amount.
	 *
	 * @param vtok Token of the resources state view
	 * @param verbose print in INFO log level is true, while false in DEBUG
	 */
	void PrintStatusReport(RViewToken_t vtok = 0, bool verbose = false) const;

	/**
	 * @brief Print details about how resource usage is partitioned among
	 * applications/EXCs
	 *
	 * @param path The resource path
	 * @param vtok The token referencing the resource state view
	 * @param verbose print in INFO log level is true, while false in DEBUG
	 */
	void PrintAppDetails(ResourcePathPtr_t path, RViewToken_t vtok,
			bool verbose) const;

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
	ExitCode_t RegisterResource(std::string const & path,
			std::string const & units, uint64_t amount);

	/**
	 * @brief Update availabilies for the specified resource
	 *
	 */
	ExitCode_t UpdateResource(std::string const & path,
			std::string const & units, uint64_t amount);

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
	ExitCode_t BookResources(AppSPtr_t papp,
			UsagesMapPtr_t const & rsrc_usages, RViewToken_t vtok = 0);

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
	void ReleaseResources(AppSPtr_t papp, RViewToken_t vtok = 0);


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
	ExitCode_t  ReserveResources(ResourcePathPtr_t ppath, uint64_t amount);

	bool  IsOfflineResource(ResourcePathPtr_t ppath) const;

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
	bool IsReshuffling(UsagesMapPtr_t const & pum_current,
			UsagesMapPtr_t const & pum_next);

	/**
	 * @see ResourceAccounterConfIF
	 */
	ExitCode_t GetView(std::string who_req, RViewToken_t & tok);

	/**
	 * @see ResourceAccounterConfIF
	 */
	void PutView(RViewToken_t tok);

	/**
	 * @brief Get the system resource state view
	 *
	 * @return The token of the system view
	 */
	inline RViewToken_t GetSystemView() {
		return sys_view_token;
	}

	/**
	 * @brief Get the scheduled resource state view
	 *
	 * @return The token of the scheduled view
	 */
	inline RViewToken_t GetScheduledView() {
		return sch_view_token;
	}

	/**
	 * @brief Set the scheduled resource state view
	 *
	 * @return The token of the system view
	 */
	void SetScheduledView(RViewToken_t svt);

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
	ExitCode_t SyncAcquireResources(AppSPtr_t const & papp);

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
	 * @brief Handler for the shell command that allows to specify a reserved
	 * amount of resources. This means that such amount will not be allocable.
	 *
	 * @param r_path The resource path (char string)
	 * @value value  The amount to reserve
	 *
	 * @return 0 if success, an positive integer value otherwise
	 */
	int ReserveHandler(char * r_path, char * value);

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
		/** Mutex for protecting the session */
		std::mutex mtx;
		/** If true a synchronization session has started */
		bool started;
		/** Token for the temporary resource view */
		RViewToken_t view;
		/** Count the number of session elapsed */
		uint32_t count;

	} sync_ssn;

	/** The logger used by the resource accounter */
	LoggerIF  *logger = NULL;

	/** The Application Manager component */
	bbque::ApplicationManager & am;

	/** The Command Manager component */
	CommandManager &cm;

	/** Mutex protecting resource release and acquisition */
	std::recursive_mutex status_mtx;

	/** The tree of all the resources in the system.*/
	ResourceTree resources;

	/** The set of all the resource paths registered */
	std::set<std::string> paths;

	/** The resource paths registered (strings and objects) */
	std::map<std::string, ResourcePathPtr_t> r_paths;

	/** Counter for the total number of registered resources */
	std::map<Resource::Type_t, uint16_t> r_count;

	/** List that keeps track of the managed resource types */
	std::list<Resource::Type_t> r_types;

	/** Keep track of the max length between resources path string */
	uint8_t path_max_len = 0;


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
	RViewToken_t sys_view_token;

	/**
	 * @brief Token referencing the view on scheduled resources
	 *
	 * When a new scheduling has been selected, this is the token referencing
	 * the corresponding view on resources state. When that schedule has been
	 * committed, i.e. resources usage synchronized, this has the same value
	 * of sys_view_token.
	 */
	RViewToken_t sch_view_token = 0;

	/**
	 * Default constructor
	 */
	ResourceAccounter();

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
	ResourcePtrList_t GetList(ResourcePathPtr_t ppath,
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
	uint64_t QueryStatus(ResourcePtrList_t const & rsrc_list,
				QueryOption_t q_opt, RViewToken_t vtok = 0,
				AppSPtr_t papp = AppSPtr_t()) const;

	/**
	 * @brief Check the resource availability for a whole set
	 *
	 * @param usages A map of Usage objects to check
	 * @param vtok The token referencing the resource state view
	 * @param papp The application interested in the query
	 * @return RA_SUCCESS if all the resources are availables,
	 * RA_ERR_USAGE_EXC otherwise.
	 */
	ExitCode_t CheckAvailability(UsagesMapPtr_t const & usages,
			RViewToken_t vtok = 0, AppSPtr_t papp = AppSPtr_t()) const;

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
	ExitCode_t GetAppUsagesByView(RViewToken_t vtok,
			AppUsagesMapPtr_t &	apps_usages);

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
	ExitCode_t IncBookingCounts(UsagesMapPtr_t const & app_usages,
			AppSPtr_t const & papp, RViewToken_t vtok = 0);

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
	ExitCode_t DoResourceBooking(AppSPtr_t const & papp,
			UsagePtr_t & pusage, RViewToken_t vtok);


	/**
	 * @brief Release the resources
	 *
	 * This is the synchronization session mutex free verios of the dual
	 * public method ReleaseResources
	 *
	 * @see ReleaseResources
	 */
	void _ReleaseResources(AppSPtr_t papp, RViewToken_t vtok = 0);

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
	void SchedResourceBooking(AppSPtr_t const & papp, ResourcePtr_t & rsrc,
			uint64_t & requested, RViewToken_t vtok);

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
	void SyncResourceBooking(AppSPtr_t const & papp, ResourcePtr_t & rsrc,
			uint64_t & requested);

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
	void DecBookingCounts(UsagesMapPtr_t const & app_usages,
			AppSPtr_t const & app, RViewToken_t vtok = 0);

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
	void UndoResourceBooking(AppSPtr_t const & papp, UsagePtr_t & pusage,
			RViewToken_t vtok);

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
		return sync_ssn.started;
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
	RViewToken_t SetView(RViewToken_t vtok);

};

}   // namespace bbque

#endif // BBQUE_RESOURCE_ACCOUNTER_H_
