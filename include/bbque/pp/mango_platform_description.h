#ifndef BBQUE_MANGO_PLATFORM_DESCRIPTION_H_
#define BBQUE_MANGO_PLATFORM_DESCRIPTION_H_

#include <chrono>
#include <cstdint>
#include <vector>
#include <boost/thread/shared_mutex.hpp>

#include "bbque/pp/platform_description.h"

namespace bbque {
namespace pp {

class MangoTile : public PlatformDescription::MulticoreProcessor {

public:
	/**
	 * @brief This enumeration represents the unit family in the Mango platform.
	 * This values must agree with the Mango specification and low-level
	 * runtime
	 */
	typedef enum MangoUnitFamily_e {
		UNIT_FAMILY_PEAK = 0,
		UNIT_FAMILY_NUPLUS,
		UNIT_FAMILY_DCT,
		UNIT_FAMILY_TETRAPOD,
		UNIT_FAMILY_GN,
		UNIT_FAMILY_NONE = 255
	} MangoUnitFamily_t;

	/**
	 * @brief This enumeration represents the unit model in the Mango platform.
	 * This values must agree with the Mango specification and low-level
	 * runtime
	 */
	typedef enum MangoUnitModel_e {
		UNIT_MODEL_PEAK_CONF_0 = 0,
		UNIT_MODEL_PEAK_CONF_1,
		UNIT_MODEL_PEAK_CONF_2,
		UNIT_MODEL_PEAK_CONF_3,
		UNIT_MODEL_PEAK_CONF_4,
		UNIT_MODEL_PEAK_CONF_5,
		UNIT_MODEL_PEAK_CONF_6,
		// NUPLUS range is from 50 - 99
		UNIT_MODEL_NUPLUS_CONF_0 = 50,
		// DCT range is from 100 - 149
		UNIT_MODEL_DCT_CONF_0 = 100,
		// TETRAPOD range is from 150 - 199
		UNIT_MODEL_TETRAPOD_CONF_0 = 150,
		UNIT_MODEL_GN_CONF_0 = 255
	} MangoUnitModel_t;


	/**
	 * @brief Constructor for a MangoTile object.
	 */
	MangoTile(int id, MangoUnitFamily_t family, MangoUnitModel_t model) 
		: MulticoreProcessor(id), family(family), model(model) {
		this->SetArchitecture("MangoTile");
	}

	/**
	 * @brief Returns the family of the unit in this tile
	 */
	inline MangoUnitFamily_t GetFamily() const {
		return this->family;
	}

	/**
	 * @brief Sets the family for the unit in this tile
	 */
	inline void SetFamily(MangoUnitFamily_t family) {
		this->family = family;
	}

	/**
	 * @brief Returns the model of the unit in this tile
	 */
	inline MangoUnitModel_t GetModel() const {
		return this->model;
	}

	/**
	 * @brief Sets the model for the unit in this tile
	 */
	inline void SetModel(MangoUnitModel_t model) {
		this->model = model;
	}

	static int GetCoreNr(MangoTile::MangoUnitFamily_t family, MangoTile::MangoUnitModel_t model) noexcept {
		if (family == UNIT_FAMILY_PEAK) {
			switch(model) {
			case UNIT_MODEL_PEAK_CONF_0:
				return 2;
			case UNIT_MODEL_PEAK_CONF_1:
				return 4;
			case UNIT_MODEL_PEAK_CONF_2:
				return 8;
			case UNIT_MODEL_PEAK_CONF_3:
				return 8;
			case UNIT_MODEL_PEAK_CONF_4:
				return 8;
			case UNIT_MODEL_PEAK_CONF_5:
				return 8;
			case UNIT_MODEL_PEAK_CONF_6:
				return 8;
			default:
				return 0;
			}
		} else if (family == UNIT_FAMILY_NUPLUS) {
			// TODO Clasify according to NUPLUS provided models
			return 1;
		} else if (family == UNIT_FAMILY_DCT) {
			// TODO clasify according to DCT provided models
			return 1;
		} else if (family == UNIT_FAMILY_TETRAPOD) {
			// There is a single model, which is able to run a single kernel concurrently
			return 1;
		} else if (family == UNIT_FAMILY_GN) {
			return 4;
		}

		return 0;
	}

private:
	MangoUnitFamily_t family;
	MangoUnitModel_t  model;

};


}   // namespace pp
}   // namespace bbque

#endif // BBQUE_PLATFORM_DESCRIPTION_H_
