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

#ifndef BBQUE_METRICS_CONTRIBUTE_
#define BBQUE_METRICS_CONTRIBUTE_

#include <cstring>

#include "bbque/configuration_manager.h"
#include "bbque/resource_accounter.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/res/resource_path.h"

#define SC_CONF_BASE_STR 	SCHEDULER_POLICY_CONFIG".Contrib."
#define SC_NAME_MAX_LEN 	11

namespace br = bbque::res;
namespace bu = bbque::utils;

namespace bbque { namespace plugins {


/**
 * @brief Base class for the implementation of scheduling metrics contribute
 *
 * The scheduling policy "YaMS" bases its decision on a modular metrics, made
 * by several contributes. This base class allows the definition of a single
 * metrics contribute. Such contribute must be and index (i.e. € [0, 1])
 * that captures the impact of scheduling the given application, in the
 * working mode specified, having the resources bound into the assigned
 * cluster. The impact must be related to a specific aspect, i.e.
 * reconfiguration costs, migration overheads, power consumption, thermal
 * heatings, and so on...
 */
class SchedContrib {

public:

	/**
	 * @brief ExitCode_t
	 */
	enum ExitCode_t {
		/** Success */
		SC_SUCCESS,
		/** Generic fail during initialization */
		SC_INIT_FAILED,
		/** No more processing element in a given cluster */
		SC_RSRC_NO_PE,
		/** A specific resource is not available */
		SC_RSRC_UNAVL,
		/** Missing a valid resource state view token */
		SC_ERR_VIEW,
		/** Unexpected error condition */
		SC_ERROR
	};

	/**
	 * @brief Statistical metrics
	 */
	enum MCTMetrics_t {
		/** Computing time */
		SC_COMP_TIME,

		SC_METRICS_COUNT
	};

	/**
	 * @brief Common configuration parameters
	 *
	 * The set of common configuration parameters that can be set for each
	 * type or resource
	 */
	enum ConfigParams_t {
		/** Maximum Saturation Level */
		SC_MSL,

		SC_CONFIG_COUNT
	};

	/**
	 * @brief Levels of resource usage determining the region boundaries
	 *
	 * According to the current usage level of a resource, we distinguish
	 * among different regions in order to provide a coarse-grained
	 * information about it, that derived class implementing the metrics
	 * contribute computation, could exploit for their evaluations.To be
	 * precise, the idea is to bind a specific function to each region, to
	 * evaluate the impact of a resource requirement. The regions are
	 * distinguished since it is reasonable to think about the aim of penalize
	 * a request the closer it gets the 100% of usage.
	 */
	struct ResourceThresholds_t {
		/** Maximum Saturation Level */
		uint32_t saturate;
		/** Current usage level (system resource state)*/
		uint32_t usage;
		/** Amount of resource remaining before reaching the saturation */
		uint32_t free;
		/** Difference between saturation and free resources */
		uint32_t sat_lack;
		/** Total amount of resource */
		uint32_t total;
	};

	/**
	 * @brief Parametes for a generic linear function
	 */
	struct LParams_t {
		/** Scale:     SCALE * x       */
		float scale;
		/** XOffset:   f(x +/- OFFSET) */
		float xoffset;
	};

	/**
	 * @brief Parameters for a generic exponential function
	 */
	struct EParams_t {
		/** Base:      BASE ^ (x)              */
		float base;
		/** XOffset:   base ^ (x +/- OFFSET)   */
		float xoffset;
		/** XScale:    base ^ {SCALE * (x)}    */
		float xscale;
		/** YScale:    SCALE * base ^ (x)      */
		float yscale;
	};

	/**
	 * @brief Parameters for a CLE filter
	 */
	struct CLEParams_t {
		/** Constant */
		float k;
		/** Parameters for the linear function */
		LParams_t lin;
		/** Parameters for the exponential functional */
		EParams_t exp;
	};


	/************************ Static data ****************************/


	/** Type of configuration parameters string */
	static char const * ConfigParamsStr[SC_CONFIG_COUNT];

	 /** Default values per type of configuration parameters */
	static uint16_t const ConfigParamsDefault[SC_CONFIG_COUNT];


	/*********************** Public methods **************************/

	/**
	 * @brief Constructor
	 *
	 * @param name A name identifying the specific contribute
	 * @param _bd_info Information about binding domains
	 * @param cfg_params Global configuration parameters
	 */
	SchedContrib(
		const char * name,
		BindingInfo_t const & _bd_info,
		uint16_t const cfg_params[]);

	virtual ~SchedContrib();

	/**
	 * @brief The identifying name of the contribution
	 */
	inline const char *Name() {
		return name;
	}

	/**
	 * @brief Set information for referencing the current state view
	 *
	 * @param _sv Pointer to the System instance
	 * @param _vtok The token of scheduling resource state view
	 */
	inline void SetViewInfo(System * _sv, br::RViewToken_t _vtok) {
		sv   = _sv;
		vtok = _vtok;
	}

	/**
	 *@brief Set the binding information
	 *
	 * @param _bd_info A binding information data structure
	 */
	inline void SetBindingInfo(BindingInfo_t & _bd_info) {
		bd_info = _bd_info;
	}

	/**
	 * @brief Perform setup operations
	 *
	 * A metrics contribute can implement this method to place some work that
	 * should be done once during a scheduling run. instead of being repeated
	 * at each Compute() call.
	 *
	 * @param Pointer to generic parameters
	 *
	 * @return An exit code defined in @see ExitCode_t
	 */
	 virtual ExitCode_t Init(void * params) = 0;

	/**
	 * @brief Metrics computation
	 *
	 * Compute the scheduling metrics for the application, taking into account
	 * the working mode specified an thus the bound resource set. The higher
	 * is computd value, the better is the choice of schedule the entity.
	 * Converserly, the lower is the computed the more penalizing would be the
	 * scheduling.
	 *
	 * @param evl_ent The scheduling entity to evaluate (App/AWM/Cluster)
	 * @param ctrib The computed contribute to set
	 *
	 * @return @see ExitCode_t
	 */
	 ExitCode_t Compute(SchedulerPolicyIF::EvalEntity_t const & evl_ent,
			 float & ctrib);

protected:

	 /** Logger */
	 std::unique_ptr<bu::Logger> logger;

	 /** Configuration manager instance */
	 ConfigurationManager & cm;


	 /** Pointer to the System instance */
	 System * sv = NULL;

	 /** The token of scheduling resource state view */
	 br::RViewToken_t vtok = 0;

	 /**
	  * The base resource path for the binding step.
	  * From a scheduling contribution perspective, this represents the domain
	  * in which consider the index computation
	  */
	 BindingInfo_t bd_info;


	 /** Contribute identifier name */
	 char name[SC_NAME_MAX_LEN];

	/**
	 * @brief Resource usage thresholds
	 *
	 * Return the resource thresholds related to the usage in the current
	 * scheduling state view. This information are usually exploited to
	 * distinguish among three regions:
	 *
	 * 1. SUB-SATURATION: The new resource usage would be included between 0
	 * and the previously scheduled usage level.
	 * 2. IN-SATURATION: The new resource usage would be included between the
	 * previously scheduled usage level and the maximum saturation level
	 * (defined though a configuration parameter).
	 * 3. OVER-SATURATION: The new resource usage would be overpass the
	 * maximum saturation level.
	 *
	 * @param r_path Resource path
	 * @param amount Requested resource usage amount
	 * @param evl_ent Entity to evaluate for scheduling
	 * @param rt The structure filled with the information regarding the
	 * resource thresholds information
	 */
	 void GetResourceThresholds(ResourcePathPtr_t r_path, uint32_t amount,
			 SchedulerPolicyIF::EvalEntity_t const & evl_ent,
			 ResourceThresholds_t & rt);

	 /**
	  * @brief Filter function for resource usage index computation
	  *
	  * Give a resource request, the method returns an index of the goodness
	  * of performing a given resource allocation.
	  *
	  * <tt>
	  *
	  * Index
	  * ^
	  * |----------       Constant
	  * |          \
	  * |           \     Linear
	  * |            .
	  * |             .   Exponential
	  * |_________._.__`-.___________
	  *          /   \
	  *         c     l
	  *
	  * </tt>
	  *
	  * @param c_thresh Threshold of constant index
	  * @param l_thresh Threshold of linear decreasing index
	  * @param rsrc_usage Amount of resource request
	  * @param params The parameters for the internal functions
	  *
	  * @return The index value
	  */
	 float CLEIndex(uint32_t c_thresh, uint32_t l_thresh, float rsrc_usage,
			 CLEParams_t const & params);


	 /***  Mathematical functions ***/

	 /**
	  * @brief Generic linear function
	  *
	  * @param x The independent variable (assigned)
	  * @param params Parameters for the computation
	  *
	  * @return A floating point value
	  */
	 float FuncLinear(float x, LParams_t const & params);

	 /**
	  * @brief Generic exponential function
	  *
	  * @param x The independent variable (assigned)
	  * @param params Parameters for the computation
	  *
	  * @return A floating point value
	  */
	 float FuncExponential(float x, EParams_t const & params);


	 /******************* To be implemented by derived classes **********/


	 /**
	  * @brief Compute the contribute (override required)
	  *
	  * This must be implemented by the derived class implementing the
	  * specific metrics contribute computation
	  *
	  * @param evl_ent Entity to evaluate for scheduling
	  * @param ctrib The contribute value to set
	  *
	  * @return @see ExitCode_t
	  */
	 virtual ExitCode_t _Compute(
			 SchedulerPolicyIF::EvalEntity_t const & evl_ent,
			 float & ctrib) = 0;

private:

	 /** Maximum Saturation Levels per resource */
	 float msl_params[br::ResourceIdentifier::TYPE_COUNT];

};


} // namespace plugins

} // namespace bbque

#endif // BBQUE_METRICS_CONTRIBUTE_
