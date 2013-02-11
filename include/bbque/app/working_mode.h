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

#ifndef BBQUE_WORKING_MODE_H_
#define BBQUE_WORKING_MODE_H_

#include <vector>

#include "bbque/app/working_mode_conf.h"
#include "bbque/res/bitset.h"
#include "bbque/plugins/logger.h"

#define AWM_NAMESPACE "bq.awm"

using bbque::plugins::LoggerIF;

namespace bbque { namespace app {

/**
 * @class WorkingMode
 *
 * @brief Collect information about a specific running mode of an
 * Application/EXC
 *
 * Each Application's Recipe must include a set of WorkingMode definitions.
 * An "Application Working Mode" (AWM) is characterized by a set of resource
 * usage requested and a "value", which is a Quality of Service indicator
 */
class WorkingMode: public WorkingModeConfIF {

public:

	/**
	 * @brief Default constructor
	 */
	WorkingMode();

	/**
	 * @brief Constructor with parameters
	 * @param id Working mode ID
	 * @param name Working mode descripting name
	 * @param value The QoS value read from the recipe
	 */
	explicit WorkingMode(uint8_t id, std::string const & name,
			float value);

	/**
	 * @brief Default destructor
	 */
	~WorkingMode();

	/**
	 * @see WorkingModeStatusIF
	 */
	inline std::string const & Name() const {
		return name;
	}

	/**
	 * @see WorkingModeStatusIF
	 */
	inline uint8_t Id() const {
		return id;
	}

	/**
	 * @brief Set the working mode name
	 * @param wm_name The name
	 */
	inline void SetName(std::string const & wm_name) {
		name = wm_name;
	}

	/**
	 * @see WorkingModeStatusIF
	 */
	inline AppSPtr_t const & Owner() const {
		return owner;
	}

	/**
	 * @brief Set the application owning the AWM
	 * @param papp Application descriptor pointer
	 */
	inline void SetOwner(AppSPtr_t const & papp) {
		owner = papp;
	}

	/**
	 * @brief Check if the AWM is hidden
	 *
	 * The AWM can be hidden if the current hardware configuration cannot
	 * accomodate the resource requirements included. This happens for example
	 * if the recipe has been profiled on a platform of the same family of the
	 * current one, but with a larger availability of resources, or whenever
	 * the amount of available resources changes at runtime, due to hardware
	 * faults or low-level power/thermal policies. This means that the AWM
	 * must not be taken into account by the scheduling policy.
	 *
	 * @return true if the AWM is hidden, false otherwise.
	 */
	inline bool Hidden() const {
		return hidden;
	}

	/**
	 * @brief Return the value specified in the recipe
	 */
	inline float RecipeValue() const {
		return value.recpv;
	}

	/**
	 * @see WorkingModeStatusIF
	 */
	inline float Value() const {
		return value.normal;
	}

	/**
	 * @brief Provide an ID-string supporting log messages readability
	 */
	const char * StrId() const {
		return str_id;
	}

	/**
	 * @brief Set the QoS value specified in the recipe
	 *
	 * The value is viewed as a kind of satisfaction level, from the user
	 * perspective, about the execution of the application.
	 *
	 * @param r_value The QoS value of the working mode
	 */
	inline void SetRecipeValue(float r_value) {
		// Value must be positive
		if (r_value < 0) {
			value.recpv = 0.0;
			return;
		}
		value.recpv = r_value;
	}

	/**
	 * @brief Set the normalized QoS value
	 *
	 * The value is viewed as a kind of satisfaction level, from the user
	 * perspective, about the execution of the application.
	 *
	 * @param n_value The normalized QoS value of the working mode. It must
	 * belong to range [0, 1].
	 */
	inline void SetNormalValue(float n_value) {
		// Normalized value must be in [0, 1]
		if ((n_value < 0.0) || (n_value > 1.0)) {
			logger->Error("SetNormalValue: value not normalized (v = %2.2f)",
					n_value);
			value.normal = 0.0;
			return;
		}
		value.normal = n_value;
	}

	/**
	 * @brief Validate the resource requests according to the current HW
	 * platform configuration/status
	 *
	 * Some HW architectures can be released in several different platform
	 * versions, providing variable amount of resources. Moreover, the
	 * availabilty of resources can vary at runtime due to faults or
	 * low level power/thermal policies. To support such a dynamicity at
	 * runtime it is useful to hide the AWM including a resource requirement
	 * that cannot be satisfied, according to its current total avalability.
	 *
	 * This method performs this check, setting the AWM to "hidden", in case
	 * the condition above is true. Hidden AWMs must not be taken into account
	 * by the scheduling policy
	 */
	ExitCode_t Validate();

	/**
	 * @brief Set the amount of a resource usage request
	 *
	 * This method should be mainly called by the recipe loader.
	 *
	 * @param rsrc_path Resource path
	 * @param amount The usage amount
	 *
	 * @return WM_RSRC_NOT_FOUND if the resource cannot be found in the
	 * system. WM_RSRC_ERR_NAME if the resource path is not valid (unknown
	 * types have been specified in the resource path string).
	 * WM_SUCCESS if the request has been correctly added
	 */
	ExitCode_t AddResourceUsage(std::string const & rsrc_path, uint64_t amount);

	/**
	 * @see WorkingModeStatusIF
	 */
	uint64_t ResourceUsageAmount(ResourcePathPtr_t ppath) const;

	/**
	 * @see WorkingModeStatusIF
	 */
	inline UsagesMap_t const & RecipeResourceUsages() const {
		return resources.requested;
	}

	/**
	 * @see WorkingModeStatusIF
	 */
	inline size_t NumberOfResourceUsages() const {
		return resources.requested.size();
	}

	/*-----------------------------------------------------------------------*
	 *                RESOURCE BINDING function members                      *
	 *-----------------------------------------------------------------------*/

	/**
	 * @see WorkingModeConfIF
	 */
	ExitCode_t BindResource(ResourceIdentifier::Type_t r_type,
			ResID_t src_ID,	ResID_t dst_ID, uint16_t b_id = 0);

	/**
	 * @see WorkingModeStatusIF
	 */
	inline UsagesMapPtr_t GetSchedResourceBinding(uint16_t b_id = 0) const {
		return resources.sched_bindings[b_id];
	}

	/**
	 * @brief Set the resource binding to schedule
	 *
	 * This binds the map of resource usages pointed by "resources.sched_bindings"
	 * to the WorkingMode. The map will contain Usage objects
	 * specifying the the amount of resource requested (value) and a list of
	 * system resource descriptors to which bind the request.
	 *
	 * This method is invoked during the scheduling step to track the set of
	 * resources to acquire at the end of the synchronization step.
	 *
	 * @param b_id The scheduling binding id to set ready for synchronization
	 * (optional)
	 *
	 * @return WM_SUCCESS, or WM_RSRC_MISS_BIND if some bindings are missing
	 */
	ExitCode_t SetResourceBinding(uint16_t b_id = 0);


	/**
	 * @brief Get the map of scheduled resource usages
	 *
	 * This method returns the map of the resource usages built through the
	 * mandatory resource binding, in charge of the scheduling policy.
	 *
	 * It is called by the ResourceAccounter to scroll through the list of
	 * resources bound to the working mode assigned.
	 *
	 * @return A shared pointer to a map of Usage objects
	 */
	inline UsagesMapPtr_t GetResourceBinding() const {
		return resources.sync_bindings;
	}

	/**
	 * @brief Clear the scheduled resource binding
	 *
	 * The method reverts the effects of SetResourceBinding()
	 */
	void ClearResourceBinding();

	/**
	 * @see WorkingModeConfIF
	 */
	inline void ClearSchedResourceBinding() {
		resources.sched_bindings.clear();
	}

	/**
	 * @see WorkingModeStatusIF
	 */
	ResourceBitset BindingSet(ResourceIdentifier::Type_t r_type) const;

	/**
	 * @see WorkingModeStatusIF
	 */
	ResourceBitset BindingSetPrev(ResourceIdentifier::Type_t r_type) const;

	/**
	 * @see WorkingModeStatusIF
	 */
	bool BindingChanged(ResourceIdentifier::Type_t r_type) const;

private:

	/**
	 * @struct BindingInfo
	 *
	 * Store binding information, i.e., on which system resources IDs the
	 * resource (type) has been bound by the scheduling policy
	 */
	struct BindingInfo {
		/** Save the previous set of clusters bound */
		ResourceBitset prev;
		/** The current set of clusters bound */
		ResourceBitset curr;
		/** True if current set differs from previous */
		bool changed;
	};

	/** The logger used by the application manager */
	LoggerIF  *logger;

	/**
	 * A pointer to the Application descriptor containing the
	 * current working mode
	 */
	AppSPtr_t owner;

	/** A numerical ID  */
	uint8_t id;

	/** A descriptive name */
	std::string name;

	/** Logger messages ID string */
	char str_id[15];

	/**
	 * Whether the AWM includes resource requirements that cannot be
	 * satisfied by the current hardware platform/configuration it can be
	 * flagged as hidden. The idea is to support a dynamic reconfiguration
	 * of the underlying hardware such that some AWMs could be dynamically
	 * taken into account or not at runtime.
	 */
	bool hidden;

	/**
	 * @struct WorkingModeValue
	 *
	 * Store information regarding the value of the AWM
	 */
	struct WorkingModeValue {
		/** The QoS value associated to the working mode as specified in the
		 * recipe */
		float recpv;
		/** The normalized QoS value associated to the working mode */
		float normal;
	} value;

	/**
	 * @struct ResourceUsagesInfo
	 *
	 * Store information about the resource requests specified in the recipe
	 * and the bindings built by the scheduling policy
	 */
	struct ResourcesInfo {
		/**
		 * The map of resources usages from the recipe
		 */
		UsagesMap_t requested;
		/**
		 * The temporary map of resource bindings. This is built by the
		 * BindResource calls
		 */
		std::vector<UsagesMapPtr_t> sched_bindings;
		/**
		 * The map of the resource bindings allocated for the working mode.
		 * This is set by SetResourceBinding() as a commit of the
		 * bindings performed, reasonably by the scheduling policy.
		 */
		UsagesMapPtr_t sync_bindings;
		/**
		 *Info regarding bindings per resource
		 */
		std::vector<BindingInfo> binding_masks;

	} resources;

};

} // namespace app

} // namespace bbque

#endif	// BBQUE_WORKING_MODE_H_
