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

#ifndef BBQUE_P2012_PP_H_
#define BBQUE_P2012_PP_H_

#include "bbque/command_manager.h"
#include "bbque/config.h"
#include "bbque/platform_proxy.h"
#include "bbque/resource_accounter.h"
#include "bbque/utils/attributes_container.h"
#include "bbque/utils/deferrable.h"

#include "p2012_lnx_dd.h"
#include "p2012_pil.h"


#define PLATFORM_ID         		"com.st.sthorm"

#define FABRIC_POWER_RESOURCE   "sys0.acc0.pwr0"
#define FABRIC_POWER_IDLE_MW     300
#define FABRIC_POWER_FULL_MW     2000

#define DEFAULT_POWER_SAMPLE_T_MS  500  // milliseconds
#define DEFAULT_POWER_CHECK_T_S    10   // seconds
#define DEFAULT_POWER_GUARD_THR    5    // percentage

#define POWER_EMA_SAMPLES   5

namespace br = bbque::res;

namespace bbque {

class P2012PP: public PlatformProxy, public CommandHandler {

public:

	/**
	 * @brief Constructor
	 */
	P2012PP();

	/**
	 * @brief Destructor
	 */
	~P2012PP();

private:

	/**
	 * @brief Exponential Moving Average accumulator
	 *
	 * This class provides a simple accumulator for on-line computation of an
	 * Exponential Moving Average, with an exponential factor @see alpha.
	 *
	 * The alpha factor is expressed in terms of N samples, where alpha =
	 * 2/(N+1). For example, N = 19 is equivalent to alpha = 0.1.
	 * The half-life of the weights (the interval over which the
	 * weights decrease by a factor of two) is approximately N/2.8854
	 * (within 1% if N > 5).
	 */
	class EMA {
	private:
		double alpha;
		double value;
		uint8_t load;
	public:
		EMA(int samples = 6, double firstValue = 0) :
			alpha(2.0 / (samples +1)),
			value(firstValue),
			load(samples) {
		}
		double update(double newValue) {
			value = ((alpha * newValue) + ((1 - alpha) * value));
			if (unlikely(load > 0))
				--load;
			return value;
		}
		void reset(int samples, double firstValue) {
			alpha = 2.0 / (samples +1);
			value = firstValue;
			load  = samples;
		}
		double get() const {
			if (unlikely(load > 0))
				return 0;
			return value;
		}
	};

	/**
	 * @brief A pointer to an EMA-defined accounter
	 */
	typedef std::shared_ptr<EMA> pEma_t;

	/**
	 * @brief This summarize data needed for resource mapping
	 */
	struct PlatformResourceBinding_t {
		/** Cluster ID */
		int16_t cluster_id;
		/** Amount of resource to assign */
		uint64_t amount;
		/** Resource type */
		ResourceIdentifier::Type_t type;
	};

	/***************************************************
	 *            Power management support             *
	 ***************************************************/

	enum PowerSetting_t {
		BUDGET_MW = 0   ,
		SAMPLING_PERIOD ,
		CHECKING_PERIOD ,
		GUARD_MARGIN
	};

	struct PowerManagement_t {
		/** Nominal power budget set */
		uint32_t budget_mw;
		/** Period of power consumption polling */
		uint32_t sample_period;
		/** Period of power checking */
		uint32_t check_period;
		/** After how may samples the policy is called */
		uint32_t check_samples;
		/** Guard threshold for the power checking */
		uint32_t guard_margin;

		/** Current power consumption (EMA)*/
		uint32_t curr_mw;
		/** Current power read timestamp */
		uint32_t curr_ts;
		/** Sample counter */
		uint32_t count_s;
		/** Current amount of total unreserved power resource */
		uint32_t unreserved;
	} power;

	/** Deferrable power sampling **/
	Deferrable pwr_sample_dfr;

	/** Moving Exponential Average for the power consumption sampling */
	pEma_t pwr_sample_ema;

	/** Placeholder variables
	 * TODO: Remove once STHORM PIL updated **/
	uint32_t p2012_ts;
	uint32_t p2012_mw;

	/**************************************************/


	/**
	 * @brief Shared pointer to struct PlatformResourceBinding_t
	 */
	typedef std::shared_ptr<PlatformResourceBinding_t> PlatformResourceBindingPtr_t;

	/**
	 * @brief P2012 shared memory buffer
	 *
	 * This buffer is shared with the Linux device driver. It will store the
	 * device descriptor
	 */
	 P2012_buffer_mem_t sh_mem;

	 /**
	  * @brief The output message queue ID
	  *
	  * The queue into which to send messages to the platform
	  */
	 P2012_queue_id_t out_queue_id;

	 /**
	  * @brief The input message queue ID
	  *
	  * The queue from which to fetch messages coming from the platform
	  */
	 P2012_queue_id_t in_queue_id;

	/**
	 * @brief P2012 device descriptor
	 *
	 * This is a pointer to the data structure that will contain both static
	 * and runtime information regarding the P2012. This is where the
	 * BarbequeRTRM writes information regarding the resource partitioning
	 * choices.
	 */
	ManagedDevice_t * pdev;

	/**
	 * @brief Max fabric fraction usage
	 *
	 * Ratio between the number of clusters available at runtime and the
	 * maximum defined into the PIL header file
	 */
	float pe_fabric_quota_max;

/*******************************************************************************
 *  Platform Specific (low-level) methods
 ******************************************************************************/

	/**
	 * @brief Return the string ID of the platform
	 */
	const char* _GetPlatformID();

	/**
	 * @brief Initialize resource information
	 */
	ExitCode_t _LoadPlatformData();

	/**
	 * @brief Setup the platform before resource mapping
	 *
	 * NOTE: Empty implemention right now. By extending the signature with a
	 * "void *" argument we could replace method InitExcConstraints()
	 */
	ExitCode_t _Setup(AppPtr_t papp);

	/**
	 * @brief Release the resources previously assigned
	 *
	 * @note The method simply calls _ReclaimResources(), since in the case of
	 * P2012 the two actions are equivalent
	 *
	 * @param papp The application/EXC releasing the resources
	 */
	ExitCode_t _Release(AppPtr_t papp);

	/**
	 * @brief Reclaim the resources previously assigned
	 *
	 * @param papp The application/EXC releasing the resources
	 */
	ExitCode_t _ReclaimResources(AppPtr_t papp);

	/**
	 * @brief Map the resource assignment into the platform descriptor
	 *
	 * @param papp The application/EXC acquiring the resources
	 * @param pres The map of resource usages of the scheduled AWM
	 * @param rvt Token for the resource state view to synchronize
	 * @param excl Specify the exclusive usage of the resources (unused)
	 */
	ExitCode_t _MapResources(AppPtr_t papp, UsagesMapPtr_t pres,
			RViewToken_t rvt, bool excl);

	/**
	 * Graceful disconnection from the platform
	 */
	void _Stop();

	/**
	 * @brief Waits for and processes events from the platform driver
	 *
	 * The method will keep itself waiting for new messages incoming
	 * from the platform driver. The message are expected to be events
	 * notification (i.e. change in the availability of resources, thermal
	 * overheating, and so on...)
	 */
	void Monitor();

	/**
	 * Platform messages received processor
	 */
	void Task();

/*******************************************************************************
 *  Class specific methods
 ******************************************************************************/

	/**
	 * @brief Initialize the platform communication channels
	 *
	 * The method initialises the message queues, the shared memory buffer,
	 * and hence the device descriptor.
	 *
	 * @return OK for success, PLATFORM_INIT_FAILED if the communication with
	 * the platform cannot be initialized.
	 */
	ExitCode_t InitPlatformComm();

	/**
	 * @brief Register the platform resources
	 *
	 * @return OK for success, PLATFORM_ENUMERATION_FAILED if resource
	 * initialization has failed.
	 */
	ExitCode_t InitResources();

	/**
	 * @brief Register a "processing element" resource of a cluster
	 *
	 * @return OK for success, PLATFORM_ENUMERATION_FAILED if resource
	 * initialization has failed.
	 */
	ExitCode_t RegisterClusterPE(uint8_t cluster_id, uint8_t pe_id);

	/**
	 * @brief Register a DMA channel of a cluster
	 *
	 * @param cluster_id Cluster ID
	 * @param pe_id Processing element ID
	 *
	 * @return OK for success, PLATFORM_ENUMERATION_FAILED if resource
	 * initialization has failed.
	 */
	ExitCode_t RegisterClusterDMA(uint8_t cluster_id, uint8_t dma_id);

	/**
	 * @brief Initialize an EXC constraints descriptor
	 *
	 * Whenever a new scheduling has been performed, for each scheduled
	 * application an EXC constraints descriptor must be properly filled.
	 * This method provides a clean descriptor.
	 *
	 * @param papp The scheduled Application/EXC
	 *
	 * @return A clean EXC constraints descriptor index
	 */
	int16_t InitExcConstraints(AppPtr_t papp);

	/**
	 * @brief Get and EXC descriptor of a running application (to be
	 * reconfigured).
	 *
	 * @param papp The scheduled Application/EXC
	 *
	 * @return An EXC constraints descriptor index for the application/EXC
	 */
	int16_t GetExcConstraints(AppPtr_t papp);

	/**
	 * @brief Get a free EXC descriptor for a starting application.
	 *
	 * The method looks for a free descriptor or one of an application to
	 * unschedule (blocked or disabled).
	 *
	 * @return A free EXC constraints descriptor index for the application/EXC
	 */
	int16_t GetExcConstraintsFree();

	/**
	 * @brief Clear an EXC constraints descriptor
	 *
	 * @param xcs_id The index of the EXC constraints descriptor to clean up
	 */
	void ClearExcConstraints(int16_t xcs_id);

	/**
	 * @brief Clear the whole EXC constraints vector
	 */
	void ClearExcConstraints();

	/**
	 * @brief Update resource assignment data into the device descriptor
	 *
	 * The method looks for a free descriptor or one of an application to
	 * unschedule (blocked or disabled).
	 *
	 * @param papp The scheduled Application/EXC
	 * @param xcs_id The EXC constraints descriptor index to update
	 * @param pbind The binding object with the resource mapping data
	 *
	 * @return OK for success, PLATFORM_DATA_PARSING_ERROR if resource
	 * information are not correctly specified
	 */
	ExitCode_t UpdateExcConstraints(AppPtr_t papp,
			int16_t xcs_id, PlatformResourceBindingPtr_t pbind);

	/**
	 * @brief The processing elements (PE) usage quota of the whole fabric
	 *
	 * @param The processing elements usage quota featured into the AWM
	 *
	 * @return a value between 0 and 10.000, in order to provide a percentage
	 * with a resolution of two decimals.
	 */
	uint16_t GetPeFabricQuota(float const & pe_cluster_quota);

	void PowerSample();

	void PowerPolicy();

	void PowerConfig(PowerSetting_t pwr_sett, uint32_t value);

	/**
	 * @brief Command handler
	 */
	int CommandsCb(int argc, char *argv[]);

};

}

#endif // BBQUE_P2012_PP_H_
