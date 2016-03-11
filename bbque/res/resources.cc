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

#include "bbque/res/resources.h"
#include "bbque/resource_accounter.h"

#define MODULE_NAMESPACE "bq.re"

namespace bu = bbque::utils;

namespace bbque { namespace res {

/*****************************************************************************
 * class Resource
 *****************************************************************************/

Resource::Resource(std::string const & res_path, uint64_t tot):
	br::ResourceIdentifier(UNDEFINED, 0),
	total(tot),
	reserved(0),
	offline(false) {
	path.assign(res_path);

	// Extract the name from the path
	size_t pos = res_path.find_last_of(".");
	if (pos != std::string::npos)
		name = res_path.substr(pos + 1);
	else
		name = res_path;

	// Initialize profiling data structures
	InitProfilingInfo();
}

Resource::Resource(br::ResourceIdentifier::Type_t type, br::ResID_t id, uint64_t tot):
	br::ResourceIdentifier(type, id),
	total(tot),
	reserved(0),
	offline(false) {

	// Initialize profiling data structures
	InitProfilingInfo();
}


void Resource::InitProfilingInfo() {
	av_profile.online_tmr.start();
	av_profile.lastOfflineTime = 0;
	av_profile.lastOnlineTime  = 0;
#ifdef CONFIG_BBQUE_PM
	pw_profile.values.resize(8);
#endif
}


Resource::ExitCode_t Resource::Reserve(uint64_t amount) {

	if (amount > total)
		return RS_FAILED;

	reserved = amount;
	DB(fprintf(stderr, FD("Resource {%s}: update reserved to [%" PRIu64 "] "
					"=> available [%" PRIu64 "]\n"),
				name.c_str(), reserved, total-reserved));
	return RS_SUCCESS;
}

void Resource::SetOffline() {
	if (offline)
		return;
	offline = true;

	// Keep track of last on-lining time
	av_profile.offline_tmr.start();
	av_profile.lastOnlineTime = av_profile.online_tmr.getElapsedTimeMs();

	fprintf(stderr, FI("Resource {%s} OFFLINED, last on-line %.3f[s]\n"),
			name.c_str(), (av_profile.lastOnlineTime / 1000.0));

}

void Resource::SetOnline() {
	if (!offline)
		return;
	offline = false;

	// Keep track of last on-lining time
	av_profile.online_tmr.start();
	av_profile.lastOfflineTime = av_profile.offline_tmr.getElapsedTimeMs();

	fprintf(stderr, FI("Resource {%s} ONLINED, last off-line %.3f[s]\n"),
			name.c_str(), (av_profile.lastOfflineTime / 1000.0));

}


uint64_t Resource::Used(RViewToken_t vtok) {
	// Retrieve the state view
	ResourceStatePtr_t view = GetStateView(vtok);
	if (!view)
		return 0;

	// Return the "used" value
	return view->used;
}

uint64_t Resource::Available(AppSPtr_t papp, RViewToken_t vtok) {
	uint64_t total_available = Unreserved();
	ResourceStatePtr_t view;

	// Offlined resources are considered not available
	if (IsOffline())
		return 0;

	// Retrieve the state view
	view = GetStateView(vtok);
	// If the view is not found, it means that nothing has been allocated.
	// Thus the availability value to return is the total amount of
	// resource
	if (!view)
		return total_available;

	// Remove resources already allocated in this vew
	total_available -= view->used;
	// Return the amount of available resource
	if (!papp)
		return total_available;

	// Add resources allocated by requesting applicatiion
	total_available += ApplicationUsage(papp, view->apps);
	return total_available;

}

uint64_t Resource::ApplicationUsage(AppSPtr_t const & papp, RViewToken_t vtok) {
	// Retrieve the state view
	ResourceStatePtr_t view = GetStateView(vtok);
	if (!view) {
		DB(fprintf(stderr, FW("Resource {%s}: cannot find view %" PRIu64 "\n"),
					name.c_str(), vtok));
		return 0;
	}

	// Call the "low-level" ApplicationUsage()
	return ApplicationUsage(papp, view->apps);
}

Resource::ExitCode_t Resource::UsedBy(AppUid_t & app_uid,
		uint64_t & amount,
		uint8_t idx,
		RViewToken_t vtok) {
	// Get the map of Apps/EXCs using the resource
	AppUseQtyMap_t apps_map;
	size_t mapsize = ApplicationsCount(apps_map, vtok);
	size_t count = 0;
	app_uid = 0;
	amount = 0;

	// Index overflow check
	if (idx >= mapsize)
		return RS_NO_APPS;

	// Search the idx-th App/EXC using the resource
	for (AppUseQtyMap_t::iterator apps_it = apps_map.begin();
			apps_it != apps_map.end(); ++apps_it, ++count) {

		// Skip until the required index has not been reached
		if (count < idx)
			continue;

		// Return the amount of resource used and the App/EXC Uid
		amount = apps_it->second;
		app_uid = apps_it->first;
		return RS_SUCCESS;
	}

	return RS_NO_APPS;
}

uint64_t Resource::Acquire(AppSPtr_t const & papp, uint64_t amount,
		RViewToken_t vtok) {
	// Retrieve the state view
	ResourceStatePtr_t view = GetStateView(vtok);
	if (!view) {
		view = ResourceStatePtr_t(new ResourceState());
		state_views[vtok] = view;
	}

	// Try to set the new "used" value
	uint64_t fut_used = view->used + amount;
	if (fut_used > total)
		return 0;

	// Set new used value and application that requested the resource
	view->used = fut_used;
	view->apps[papp->Uid()] = amount;
	return amount;
}

uint64_t Resource::Release(AppSPtr_t const & papp, RViewToken_t vtok) {
	// Retrieve the state view
	ResourceStatePtr_t view = GetStateView(vtok);
	if (!view) {
		DB(fprintf(stderr, FW("Resource {%s}: cannot find view %" PRIu64 "\n"),
					name.c_str(), vtok));
		return 0;
	}

	// Lookup the application using the resource
	AppUseQtyMap_t::iterator lkp = view->apps.find(papp->Uid());
	if (lkp == view->apps.end()) {
		DB(fprintf(stderr, FD("Resource {%s}: no resources allocated to [%s]\n"),
					name.c_str(), papp->StrId()));
		return 0;
	}

	// Decrease the used value and remove the application
	uint64_t used_by_app = lkp->second;
	view->used -= used_by_app;
	view->apps.erase(papp->Uid());

	// Return the amount of resource released
	return used_by_app;
}

void Resource::DeleteView(RViewToken_t vtok) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());
	// Avoid to delete the default view
	if (vtok == ra.GetSystemView())
		return;
	state_views.erase(vtok);
}

uint16_t Resource::ApplicationsCount(
		AppUseQtyMap_t & apps_map,
		RViewToken_t vtok) {
	// Retrieve the state view
	ResourceStatePtr_t view = GetStateView(vtok);
	if (!view)
		return 0;

	// Return the size and a reference to the map
	apps_map = view->apps;
	return apps_map.size();
}

uint64_t Resource::ApplicationUsage(
		AppSPtr_t const & papp,
		AppUseQtyMap_t & apps_map) {
	// Sanity check
	if (!papp) {
		DB(fprintf(stderr, FW("Resource {%s}: App/EXC null pointer\n"),
					name.c_str()));
		return 0;
	}

	// Retrieve the application from the map
	AppUseQtyMap_t::iterator app_using_it(apps_map.find(papp->Uid()));
	if (app_using_it == apps_map.end()) {
		DB(fprintf(stderr, FD("Resource {%s}: no usage value for [%s]\n"),
					name.c_str(), papp->StrId()));
		return 0;
	}

	// Return the amount of resource used
	return app_using_it->second;
}

ResourceStatePtr_t Resource::GetStateView(RViewToken_t vtok) {
	ResourceAccounter &ra(ResourceAccounter::GetInstance());

	// Default view if token = 0
	if (vtok == 0)
		vtok = ra.GetSystemView();

	// Retrieve the view from hash map otherwise
	RSHashMap_t::iterator it = state_views.find(vtok);
	if (it != state_views.end())
		return it->second;

	// State view not found
	return ResourceStatePtr_t();
}

#ifdef CONFIG_BBQUE_PM

void Resource::EnablePowerProfile(
		PowerManager::SamplesArray_t const & samples_window) {
	pw_profile.enabled_count = 0;

	// Check each power profiling information
	for (uint i = 0; i < samples_window.size(); ++i) {
		if (samples_window[i] <= 0)
			continue;
		++pw_profile.enabled_count;

		// Is the sample window size changed?
		if (pw_profile.values[i] &&
				pw_profile.samples_window[i] == samples_window[i])
			continue;

		// New sample window size
		pw_profile.values[i] =
				bu::pEma_t(new bu::EMA(samples_window[i]));
	}
	pw_profile.samples_window = samples_window;
}


void Resource::EnablePowerProfile() {
	EnablePowerProfile(default_samples_window);
}

double Resource::GetPowerInfo(PowerManager::InfoType i_type, ValueType v_type) {
	if (!pw_profile.values[int(i_type)])
		return 0.0;
	// Instant or mean value?
	switch (v_type) {
	case INSTANT:
		return pw_profile.values[int(i_type)]->last_value();
	case MEAN:
		return pw_profile.values[int(i_type)]->get();
	}
	return 0.0;
}

#endif // CONFIG_BBQUE_PM

}}
