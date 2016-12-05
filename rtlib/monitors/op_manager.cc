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

#include <rtlib/monitors/op_manager.h>

#ifdef CONFIG_TARGET_ANDROID
# include <stdint.h>
#else
# include <cstdint>
#endif

using namespace std;

namespace bbque { namespace rtlib { namespace as {

bool OPManager::operatingPointsComparator::operator()(const OperatingPoint &op1,
						       const OperatingPoint &op2) const{
	double val1;
	double val2;
	std::string mName;
	for (unsigned int i=0; i < metricsPriorities.size(); ++i){
		mName = metricsPriorities[i].metricName;
		val1 = op1.metrics.find(mName)->second;
		val2 = op2.metrics.find(mName)->second;
		if (val1 == val2)
			continue;
		return metricsPriorities[i].comparisonFunction(val1, val2);
	}
	return false;
}

bool OPManager::getCurrentOP(OperatingPoint &op) {
	op = operatingPoints[vectorId];
	return true;
}

bool OPManager::getLowerOP(OperatingPoint &op) {
	if (vectorId >= (operatingPoints.size()-1))
		return false;

	vectorId++;
	op = operatingPoints[vectorId];
	return true;
}

bool OPManager::getHigherOP(OperatingPoint &op) {
	if (vectorId == 0)
		return false;

	op = operatingPoints[vectorId];
	return true;
}

bool OPManager::isValidOP(const OperatingPoint &op,
			   const OPFilterList &opFilters) const {

	bool result = true;
	OPFilterList::const_iterator filter = opFilters.begin();
	std::map<std::string, double>::const_iterator mappedValue;

	while (result && filter!=opFilters.end()) {
		mappedValue = op.metrics.find(filter->name);
		if (mappedValue == op.metrics.end())
			return false;
		result = (filter->cFunction(mappedValue->second, filter->value));
		++filter;
	}
	return result;
}


bool OPManager::getCurrentOP(OperatingPoint &op, const OPFilterList &opFilters) {
	if (isValidOP(operatingPoints[vectorId], opFilters)){
		op = operatingPoints[vectorId];
		return true;
	}
	/*
	 * TODO Find a better search strategy. This one could be expensive even
	 * if it is just O(n)
	 */
	if (getLowerOP(op,opFilters))
		return true;
	return getHigherOP(op,opFilters);
}

bool OPManager::getLowerOP(OperatingPoint &op, const OPFilterList &opFilters) {
	for (unsigned int id = vectorId + 1;id < operatingPoints.size(); ++id) {
		if (isValidOP(operatingPoints[id],opFilters)){
			vectorId = id;
			op = operatingPoints[id];
			return true;
		}
	}
	return false;
}

bool OPManager::getHigherOP(OperatingPoint &op, const OPFilterList &opFilters) {
	for (int id = vectorId-1;id > 0; --id){
		if (isValidOP(operatingPoints[id], opFilters)){
			vectorId = id;
			op = operatingPoints[id];
			return true;
		}
	}
	return false;
}

bool OPManager::getNextOP(OperatingPoint &op, const OPFilterList &opFilters) {
	/*
	 * TODO Find a better search strategy. This one could be expensive
	 * even if it is just O(n)
	 */
	vectorId = 0;
	return getCurrentOP(op, opFilters);
}

void OPManager::setPolicy(PrioritiesList &orderingStrategy) {
	sort(operatingPoints.begin(),
	     operatingPoints.end(),
	     operatingPointsComparator(orderingStrategy));

	vectorId = 0;
}

} // namespace as

} // namespace rtlib

} // namespace bbque
