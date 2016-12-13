/*
 * Copyright (C) 2016  Politecnico di Milano
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

#ifndef BBQUE_RTEXC_H_
#define BBQUE_RTEXC_H_

#include <bbque/bbque_exc.h>

#include <cassert>

namespace bu = bbque::utils;

namespace bbque
{
namespace rtlib
{


/**
 * @class BbqueRTEXC
 * @brief The AEM base-class for Real-Time processes
 * @ingroup rtlib_sec02_aem
 *
 * This is a base class suitable for the implementation of an EXC that should
 * be managed by the Barbeque RTRM.
 */
class BbqueRTEXC : public BbqueEXC
{

public:

	/**
	 * @brief Build a new EXC
	 *
	 * A new EXecution Context could be build by specifying a <i>name</i>,
	 * which identifies it within the system and it is used mostly for
	 * logging statements, and a <i>recipe</i>, which specifies the set of AWM
	 * supported by the specific stream processing application being
	 * defined.
	 *
	 * The creation of a new EXC requires also a valid handler to the RTLib, which
	 * will be used for the (application transparent) communication with the
	 * BarbequeRTRM. Thus, an application willing to instantiate an EXC should
	 * foremost initialize the RTLib, which could be done as explained in @ref
	 * rtlib_sec03_plain.
	 *
	 * This is an example snippet showing how to properly instantiate an EXC:
	 * \code
	 *
	 * #include <bbque/bbque_exc.h>
	 *
	 * class ExampleEXC : public BbqueEXC {
	 * // definition of the application specific EXC
	 * };
	 *
	 * // The EXC handler
	 * pBbqueEXC_t pexc;
	 * pexc = pBbqueEXC_t(new ExampleEXC("YourExcName", "YourRecipeName", rtlib));
	 *
	 * // Checking registration was successful
	 * if (!pexc || !pexc->isRegistered()) {
	 * 	fprintf(stderr, "EXC registration FAILED\n");
	 * 	return EXIT_FAILURE;
	 * }
	 *
	 * \endcode
	 *
	 * @note To properly exploit the RTLib provided instrumentation for the
	 * profiling of run-time behaviors of the EXC, the application integrator
	 * should avoid to get resources, such as spawning threads (e.g. setup a
	 * thread pool), from within the EXC constructor.
	 * This method should be used just to pass the EXC a set of configuration
	 * parameters to be saved locally to the derived class, while the actual
	 * initialization code should be placed into the \ref onSetup method, which
	 * indeed it is called by the base class right after the constructor.
	 *
	 * @see onSetup
	 *
	 * @param name the name of the EXC
	 * @param recipe the recipe to run-time manage this EXC
	 * @param rtlib a reference to the RTLib
	 *
	 * @ingroup rtlib_sec02_aem_exc
	 */
	BbqueRTEXC(std::string const & name,
			 std::string const & recipe,
			 RTLIB_Services_t * rtlib,
			 RTLIB_RT_Level_t rt_level = RT_SOFT ) : BbqueEXC(name,
						recipe, rtlib, rt_level) {
		assert(rt_level > RT_NONE);
	}

	/**
	 * @brief Destory the EXC
	 */
	virtual ~BbqueRTEXC() {};

	/**
  	 * @brief Pre-fault the stack allocating and accessing local memory
	 *        of size `bytes`.
	 */
	RTLIB_ExitCode_t StackPreFault(size_t bytes) const noexcept;

	/**
  	 * @brief Check if the stack overfill our pre-fault allocation. In that
	 *	  case it produces a log error message. Enabled only in DEBUG
	 *	  mode, if not in debug mode, no action performed.
	 * @note Stack overflow may occur if stack near the limit. use with
	 *       precaution.
	 * @noet This is a probabilistic function. Don't trust on the result.
	 */
	void StackPreFaultPostCheck() const noexcept;


	/**
	 * @brief Try to enforce memory locking. This is usually not possible
	 * for non-root applications.
	 */
	RTLIB_ExitCode_t MemoryLock() const noexcept;

};

/**
 * @brief Pointer to an RT EXC which is a specialization of the BbqueEXC base
 * class
 */
typedef std::shared_ptr<BbqueRTEXC> pBbqueRTEXC_t;


} // namespace rtlib

} // namespace bbque

#endif // BBQUE_EXC_H_
