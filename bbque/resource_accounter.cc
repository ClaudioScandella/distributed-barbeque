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

#include "bbque/resource_accounter.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <map>
#include <string>
#include <sstream>

#include "bbque/app/working_mode.h"
#include "bbque/res/resource_path.h"
#include "bbque/application_manager.h"


#undef  MODULE_CONFIG
#define MODULE_CONFIG "ResourceAccounter"

#define RP_DIV1 " ========================================================================="
#define RP_DIV2 "|-------------------------------+-------------+---------------------------|"
#define RP_DIV3 "|                               :             |             |             |"
#define RP_HEAD "|   RESOURCES                   |     USED    |  UNRESERVED |     TOTAL   |"


#define PRINT_NOTICE_IF_VERBOSE(verbose, text)\
	if (verbose)\
		logger->Notice(text);\
	else\
		DB(\
		logger->Debug(text);\
		);

namespace ba = bbque::app;
namespace br = bbque::res;
namespace bu = bbque::utils;
namespace po = boost::program_options;

namespace bbque {

ResourceAccounter & ResourceAccounter::GetInstance() {
	static ResourceAccounter instance;
	return instance;
}

ResourceAccounter::ResourceAccounter() :
		am(ApplicationManager::GetInstance()),
		cm(CommandManager::GetInstance()),
		fm(ConfigurationManager::GetInstance()),
		status(State::NOT_READY) {

	// Get a logger
	logger = bu::Logger::GetLogger(RESOURCE_ACCOUNTER_NAMESPACE);
	assert(logger);

	// Init the system resources state view
	sys_usages_view = AppUsagesMapPtr_t(new AppUsagesMap_t);
	sys_view_token  = 0;
	usages_per_views[sys_view_token] = sys_usages_view;
	rsrc_per_views[sys_view_token]   = ResourceSetPtr_t(new ResourceSet_t);

	// Init sync session info
	sync_ssn.count = 0;

	// Init prefix path object
	r_prefix_path = br::ResourcePathPtr_t(new br::ResourcePath(PREFIX_PATH));

	// Register set quota command
#define CMD_SET_TOTAL "set_total"
	cm.RegisterCommand(RESOURCE_ACCOUNTER_NAMESPACE "." CMD_SET_TOTAL,
		static_cast<CommandHandler*>(this),
		"Set a new amount of resource that can be allocated");
	// Notify the degradation of a resource
#define CMD_NOTIFY_DEGRADATION "notify_degradation"
	cm.RegisterCommand(RESOURCE_ACCOUNTER_NAMESPACE "." CMD_NOTIFY_DEGRADATION,
		static_cast<CommandHandler*>(this),
		"Performance degradation affecting the resource [percentage]");

	// Init
	InitBindingOptions();
}

void ResourceAccounter::InitBindingOptions() {
	size_t end_pos = 0;
	size_t beg_pos = 0;
	std::string domains;
	std::string binding_str;
	br::Resource::Type_t binding_type;

	// Binding domain resource path
	po::options_description opts_desc("Resource Accounter parameters");
	opts_desc.add_options()
		(MODULE_CONFIG ".binding.domains",
		 po::value<std::string>
		 (&domains)->default_value("cpu"),
		"Resource binding domain");
	po::variables_map opts_vm;
	fm.ParseConfigurationFile(opts_desc, opts_vm);
	logger->Info("Binding options: %s", domains.c_str());

	// Parse each binding domain string
	while (end_pos != std::string::npos) {
		end_pos     = domains.find(',', beg_pos);
		binding_str = domains.substr(beg_pos, end_pos);

		// Binding domain resource path
		br::ResourcePathPtr_t binding_rp(
				new br::ResourcePath(*(r_prefix_path.get())));
		binding_rp->Concat(binding_str);

		// Binding domain resource type check
		binding_type = binding_rp->Type();
		if (binding_type == br::Resource::UNDEFINED ||
				binding_type == br::Resource::TYPE_COUNT) {
			logger->Error("Binding: Invalid domain type <%s>",
					binding_str.c_str());
			beg_pos = end_pos + 1;
			continue;
		}

#ifndef CONFIG_BBQUE_OPENCL
		if (binding_type == br::Resource::GPU) {
			logger->Warn("Binding: OpenCL support disabled."
					" Discarding <GPU> binding type");
			continue;
		}
#endif

		// New binding info structure
		binding_options.insert(
				BindingPair_t(binding_type, new BindingInfo_t));
		binding_options[binding_type]->d_path = binding_rp;
		logger->Info("Resource binding domain: '%s' Type:<%s>",
				binding_options[binding_type]->d_path->ToString().c_str(),
				br::ResourceIdentifier::TypeStr[binding_type]);

		// Next binding domain...
		beg_pos  = end_pos + 1;
	}
}

void ResourceAccounter::SetPlatformReady() {
	std::unique_lock<std::mutex> status_ul(status_mtx);
	while (status == State::SYNC) {
		status_cv.wait(status_ul);
	}
	LoadBindingOptions();
	status = State::READY;
}

void ResourceAccounter::SetPlatformNotReady() {
	std::unique_lock<std::mutex> status_ul(status_mtx);
	while (status == State::SYNC) {
		status_cv.wait(status_ul);
	}
	status = State::NOT_READY;
}

inline void ResourceAccounter::SetReady() {
	status_mtx.lock();
	status = State::READY;
	status_mtx.unlock();
	status_cv.notify_all();
}

void ResourceAccounter::LoadBindingOptions() {
	// Set information for each binding domain
	for (auto & bd_entry: binding_options) {
		BindingInfo & binding(*(bd_entry.second));
		binding.rsrcs = GetResources(binding.d_path);
		binding.count = binding.rsrcs.size();
		binding.ids.clear();

		// Skip missing resource bindings
		if (binding.count == 0) {
			logger->Warn("Init: No bindings R<%s> available",
					binding.d_path->ToString().c_str());
		}

		// Get all the possible resource binding IDs
		for (br::ResourcePtr_t & rsrc: binding.rsrcs) {
			binding.ids.push_back(rsrc->ID());
			logger->Info("Init: R<%s> ID: %d",
					binding.d_path->ToString().c_str(), rsrc->ID());
		}
		logger->Info("Init: R<%s>: %d possible bindings",
				binding.d_path->ToString().c_str(), binding.count);
	}
}

ResourceAccounter::~ResourceAccounter() {
	resources.clear();
	usages_per_views.clear();
	rsrc_per_views.clear();
	r_prefix_path.reset();
}

/************************************************************************
 *                   LOGGER REPORTS                                     *
 ************************************************************************/

// This is just a local utility function to format in a human readable
// format resources amounts.
static char *PrettyFormat(float value) {
	char radix[] = {'0', '3', '6', '9'};
	static char str[] = "1024.000e+0";
	uint8_t i = 0;
	while (value > 1023 && i < 3) {
		value /= 1024;
		++i;
	}
	snprintf(str, sizeof(str), "%8.3fe+%c", value, radix[i]);
	return str;
}

void ResourceAccounter::PrintStatusReport(
		br::RViewToken_t vtok, bool verbose) const {
	//                        +--------- 22 ------------+     +-- 11 ---+   +-- 11 ---+   +-- 11 ---+
	char rsrc_text_row[] = "| sys0.cpu0.mem0              I : 1234.123e+1 | 1234.123e+1 | 1234.123e+1 |";
	uint64_t rsrc_used = 0;

	// Print the head of the report table
	if (verbose) {
		logger->Notice("Report on state view: %d", vtok);
		logger->Notice(RP_DIV1);
		logger->Notice(RP_HEAD);
		logger->Notice(RP_DIV2);
	}
	else {
		DB(
		logger->Debug("Report on state view: %d", vtok);
		logger->Debug(RP_DIV1);
		logger->Debug(RP_HEAD);
		logger->Debug(RP_DIV2);
		);
	}

	// For each resource get the used amount
	for (auto const & path_entry: r_paths) {
		ResourcePathPtr_t const & ppath(path_entry.second);
		rsrc_used = Used(ppath, EXACT, vtok);

		// Build the resource text row
		uint8_t len = 0;
		char online = 'I';
		if (IsOfflineResource(ppath))
			online = 'O';
		len += sprintf(rsrc_text_row + len, "| %-27s %c : %11s | ",
				ppath->ToString().c_str(), online,
				PrettyFormat(rsrc_used));
		len += sprintf(rsrc_text_row + len, "%11s | ",
				PrettyFormat(Unreserved(ppath)));
		len += sprintf(rsrc_text_row + len, "%11s |",
				PrettyFormat(Total(ppath)));
		PRINT_NOTICE_IF_VERBOSE(verbose, rsrc_text_row);

		// Print details about how usage is partitioned among applications
		if (rsrc_used > 0)
			PrintAppDetails(ppath, vtok, verbose);
	}
	PRINT_NOTICE_IF_VERBOSE(verbose, RP_DIV1);
}

void ResourceAccounter::PrintAppDetails(
		ResourcePathPtr_t ppath,
		br::RViewToken_t vtok,
		bool verbose) const {
	br::Resource::ExitCode_t res_result;
	ba::AppSPtr_t papp;
	AppUid_t app_uid;
	uint64_t rsrc_amount;
	uint8_t app_index = 0;
	//                           +----- 15 ----+             +-- 11 ---+  +--- 13 ----+ +--- 13 ----+
	char app_text_row[] = "|     12345:exc_01:01,P01,AWM01 : 1234.123e+1 |             |             |";

	// Get the resource descriptor
	br::ResourcePtr_t rsrc(GetResource(ppath));
	if (!rsrc || rsrc->ApplicationsCount(vtok) == 0)
		return;

	do {
		// How much does the application/EXC use?
		res_result = rsrc->UsedBy(app_uid, rsrc_amount, app_index, vtok);
		if (res_result != br::Resource::RS_SUCCESS)
			break;

		// Get the App/EXC descriptor
		papp = am.GetApplication(app_uid);
		if (!papp || !papp->CurrentAWM())
			break;

		// Build the row to print
		sprintf(app_text_row, "| %19s,P%02d,AWM%02d : %11s |%13s|%13s|",
				papp->StrId(),
				papp->Priority(),
				papp->CurrentAWM()->Id(),
				PrettyFormat(rsrc_amount),
				"", "");
		PRINT_NOTICE_IF_VERBOSE(verbose, app_text_row);

		// Next application/EXC
		++app_index;
	} while (papp);

	// Print a separator line
	PRINT_NOTICE_IF_VERBOSE(verbose, RP_DIV3);
}

/************************************************************************
 *             RESOURCE DESCRIPTORS ACCESS                              *
 ************************************************************************/

br::ResourcePtr_t ResourceAccounter::GetResource(std::string const & path) const {
	// Build a resource path object.
	// It can be a MIXED path(not inserted in r_paths)
	ResourcePathPtr_t ppath(new br::ResourcePath(path));
	if (ppath == nullptr)
		return br::ResourcePtr_t();
	return GetResource(ppath);
}

br::ResourcePtr_t ResourceAccounter::GetResource(ResourcePathPtr_t ppath) const {
	br::ResourcePtrList_t matchings(
			resources.findList(*(ppath.get()), RT_MATCH_FIRST | RT_MATCH_MIXED));
	if (matchings.empty())
		return br::ResourcePtr_t();
	return matchings.front();
}


br::ResourcePtrList_t ResourceAccounter::GetResources(
		std::string const & path) const {
	ResourcePathPtr_t ppath(new br::ResourcePath(path));
	if (!ppath)
		return br::ResourcePtrList_t();
	return GetResources(ppath);
}

br::ResourcePtrList_t ResourceAccounter::GetResources(
		ResourcePathPtr_t ppath) const {
	// If the path is a template find all the resources matching the
	// template. Otherwise perform a "mixed path" based search.
	if (ppath->IsTemplate()) {
		logger->Debug("GetResources: path <%s> is a template",
				ppath->ToString().c_str());
		return resources.findList(*(ppath.get()), RT_MATCH_TYPE);
	}
	return resources.findList(*(ppath.get()), RT_MATCH_MIXED);
}


bool ResourceAccounter::ExistResource(std::string const & path) const {
	ResourcePathPtr_t ppath(new br::ResourcePath(path));
	return ExistResource(ppath);
}

bool ResourceAccounter::ExistResource(ResourcePathPtr_t ppath) const {
	br::ResourcePtrList_t matchings(
		resources.findList(*(ppath.get()), RT_MATCH_TYPE | RT_MATCH_FIRST));
	return !matchings.empty();
}

ResourcePathPtr_t const ResourceAccounter::GetPath(
		std::string const & strpath) const {
	std::map<std::string, ResourcePathPtr_t>::const_iterator rp_it;
	// Retrieve the resource path object
	rp_it = r_paths.find(strpath);
	if (rp_it == r_paths.end()) {
		logger->Warn("GetPath: No resource path object for [%s]",
			strpath.c_str());
		return ResourcePathPtr_t();
	}
	return (*rp_it).second;
}

/************************************************************************
 *                   QUERY METHODS                                      *
 ************************************************************************/

inline uint64_t ResourceAccounter::Total(
		std::string const & path) const {
	br::ResourcePtrList_t matchings(GetResources(path));
	return QueryStatus(matchings, RA_TOTAL, 0);
}

inline uint64_t ResourceAccounter::Total(
		br::ResourcePtrList_t & rsrc_list) const {
	if (rsrc_list.empty())
		return 0;
	return QueryStatus(rsrc_list, RA_TOTAL);
}

inline uint64_t ResourceAccounter::Total(
		ResourcePathPtr_t ppath,
		PathClass_t rpc) const {
	br::ResourcePtrList_t matchings(GetList(ppath, rpc));
	return QueryStatus(matchings, RA_TOTAL, 0);
}


inline uint64_t ResourceAccounter::Used(
		std::string const & path,
		br::RViewToken_t vtok) const {
	br::ResourcePtrList_t matchings(GetResources(path));
	return QueryStatus(matchings, RA_USED, vtok);
}

inline uint64_t ResourceAccounter::Used(
		br::ResourcePtrList_t & rsrc_list,
		br::RViewToken_t vtok) const {
	if (rsrc_list.empty())
		return 0;
	return QueryStatus(rsrc_list, RA_USED, vtok);
}

inline uint64_t ResourceAccounter::Used(
		ResourcePathPtr_t ppath,
		PathClass_t rpc,
		br::RViewToken_t vtok) const {
	br::ResourcePtrList_t matchings(GetList(ppath, rpc));
	return QueryStatus(matchings, RA_USED, vtok);
}


inline uint64_t ResourceAccounter::Available(
		std::string const & path,
		br::RViewToken_t vtok,
		ba::AppSPtr_t papp) const {
	br::ResourcePtrList_t matchings(GetResources(path));
	return QueryStatus(matchings, RA_AVAIL, vtok, papp);
}

inline uint64_t ResourceAccounter::Available(
		br::ResourcePtrList_t & rsrc_list,
		br::RViewToken_t vtok,
		ba::AppSPtr_t papp) const {
	if (rsrc_list.empty())
		return 0;
	return QueryStatus(rsrc_list, RA_AVAIL, vtok, papp);
}

inline uint64_t ResourceAccounter::Available(
		ResourcePathPtr_t ppath,
		PathClass_t rpc,
		br::RViewToken_t vtok,
		ba::AppSPtr_t papp) const {
	br::ResourcePtrList_t matchings(GetList(ppath, rpc));
	return QueryStatus(matchings, RA_AVAIL, vtok, papp);
}

inline uint64_t ResourceAccounter::Unreserved(
		std::string const & path) const {
	br::ResourcePtrList_t matchings(GetResources(path));
	return QueryStatus(matchings, RA_UNRESERVED, 0);
}

inline uint64_t ResourceAccounter::Unreserved(
		br::ResourcePtrList_t & rsrc_list) const {
	if (rsrc_list.empty())
		return 0;
	return QueryStatus(rsrc_list, RA_UNRESERVED);
}

inline uint64_t ResourceAccounter::Unreserved(
		ResourcePathPtr_t ppath) const {
	br::ResourcePtrList_t matchings(GetList(ppath, MIXED));
	return QueryStatus(matchings, RA_UNRESERVED, 0);
}


inline uint16_t ResourceAccounter::Count(
		ResourcePathPtr_t ppath) const {
	br::ResourcePtrList_t matchings(GetResources(ppath));
	return matchings.size();
}

inline uint16_t ResourceAccounter::CountPerType(
		br::ResourceIdentifier::Type_t type) const {
	std::map<br::Resource::Type_t, uint16_t>::const_iterator it;
	it =  r_count.find(type);
	if (it == r_count.end())
		return 0;
	return it->second;
}


br::ResourcePtrList_t ResourceAccounter::GetList(
		ResourcePathPtr_t ppath,
		PathClass_t rpc) const {
	if (rpc == UNDEFINED)
		return GetResources(ppath);
	return resources.findList(*(ppath.get()), RTFlags(rpc));
}


inline uint64_t ResourceAccounter::QueryStatus(
		br::ResourcePtrList_t const & rsrc_list,
		QueryOption_t _att,
		br::RViewToken_t vtok,
		ba::AppSPtr_t papp) const {
	uint64_t value = 0;

	// For all the descriptors in the list add the quantity of resource in the
	// specified state (available, used, total)
	for (br::ResourcePtr_t const & rsrc: rsrc_list) {
		switch(_att) {
		case RA_AVAIL:
			value += rsrc->Available(papp, vtok);
			break;
		case RA_USED:
			value += rsrc->Used(vtok);
			break;
		case RA_UNRESERVED:
			value += rsrc->Unreserved();
			break;
		case RA_TOTAL:
			value += rsrc->Total();
			break;
		}
	}
	return value;
}

uint64_t ResourceAccounter::GetUsageAmount(
		br::UsagesMapPtr_t const & pum,
		ba::AppSPtr_t papp,
		br::RViewToken_t vtok,
		br::ResourceIdentifier::Type_t r_type,
		br::ResourceIdentifier::Type_t r_scope_type,
		BBQUE_RID_TYPE r_scope_id) const {

	if (pum == nullptr) {
		logger->Fatal("GetUsageAmount: empty map");
		return 0;
	}

	br::UsagesMap_t::const_iterator b_it(pum->begin());
	br::UsagesMap_t::const_iterator e_it(pum->end());
	return  GetAmountFromUsagesMap(
			b_it, e_it, r_type, r_scope_type, r_scope_id, papp, vtok);
}

uint64_t ResourceAccounter::GetUsageAmount(
		br::UsagesMap_t const & um,
		ba::AppSPtr_t papp,
		br::RViewToken_t vtok,
		br::ResourceIdentifier::Type_t r_type,
		br::ResourceIdentifier::Type_t r_scope_type,
		BBQUE_RID_TYPE r_scope_id) const {

	br::UsagesMap_t::const_iterator b_it(um.begin());
	br::UsagesMap_t::const_iterator e_it(um.end());
	return  GetAmountFromUsagesMap(
			b_it, e_it, r_type, r_scope_type, r_scope_id, papp, vtok);
}

inline uint64_t ResourceAccounter::GetAmountFromUsagesMap(
		br::UsagesMap_t::const_iterator & begin,
		br::UsagesMap_t::const_iterator & end,
		br::ResourceIdentifier::Type_t r_type,
		br::ResourceIdentifier::Type_t r_scope_type,
		BBQUE_RID_TYPE r_scope_id,
		ba::AppSPtr_t papp,
		br::RViewToken_t vtok) const {
	uint64_t amount = 0;
	logger->Debug("GetUsageAmount: Getting usage amount from view [%d]", vtok);

	br::UsagesMap_t::const_iterator uit(begin);
	for ( ; uit != end; ++uit) {
		br::ResourcePathPtr_t const & ppath((*uit).first);
		br::UsagePtr_t const & pusage((*uit).second);

		logger->Debug("GetUsageAmount: type:<%-3s> scope:<%-3s>",
			br::ResourceIdentifier::TypeStr[r_type],
			br::ResourceIdentifier::TypeStr[r_scope_type]);
		// Scope resource type
		if ((r_scope_type != br::Resource::UNDEFINED)
			&& (ppath->GetIdentifier(r_scope_type) == nullptr))
			continue;
		for (br::ResourcePtr_t const & rsrc: pusage->GetResourcesList()) {
			br::ResourcePathPtr_t r_path(GetPath(rsrc->Path()));
			logger->Debug("GetUsageAmount: path:<%s>", r_path->ToString().c_str());
			// Scope resource ID
			if ((r_scope_id >= 0) && (r_scope_id != r_path->GetID(r_scope_type)))
				continue;
			// Resource type
			if (r_path->Type() != r_type)
				continue;
			amount += rsrc->ApplicationUsage(papp, vtok);
		}
	}
	logger->Debug("GetUsageAmount: EXC:[%s] R:<%-3s> U:%" PRIu64 "",
			papp->StrId(), br::ResourceIdentifier::TypeStr[r_type], amount);
	return amount;
}

uint64_t ResourceAccounter::GetUsageAmount(
		br::UsagesMap_t const & um,
		br::ResourceIdentifier::Type_t r_type,
		br::ResourceIdentifier::Type_t r_scope_type,
		BBQUE_RID_TYPE r_scope_id) const {
	uint64_t amount = 0;
	for (auto & ru_entry: um) {
		br::ResourcePathPtr_t const & ppath(ru_entry.first);
		br::UsagePtr_t const & pusage(ru_entry.second);

		logger->Debug("GetUsageAmount: type:<%-3s> scope:<%-3s>",
			br::ResourceIdentifier::TypeStr[r_type],
			br::ResourceIdentifier::TypeStr[r_scope_type]);
		// Scope resource type
		if ((r_scope_type != br::Resource::UNDEFINED)
			&& (ppath->GetIdentifier(r_scope_type) == nullptr))
			continue;
		// Scope resource ID
		if ((r_scope_id >= 0) && (r_scope_id != ppath->GetID(r_scope_type)))
			continue;
		// Resource type
		if (ppath->Type() != r_type)
				continue;
		amount += pusage->GetAmount();
	}
	return amount;
}


inline ResourceAccounter::ExitCode_t ResourceAccounter::CheckAvailability(
		br::UsagesMapPtr_t const & usages,
		br::RViewToken_t vtok,
		ba::AppSPtr_t papp) const {
	uint64_t avail = 0;

	// Check availability for each Usage object
	for (auto const & ru_entry: *(usages.get())) {
		br::ResourcePathPtr_t const & rsrc_path(ru_entry.first);
		br::UsagePtr_t const & pusage(ru_entry.second);

		// Query the availability of the resources in the list
		avail = QueryStatus(pusage->GetResourcesList(), RA_AVAIL, vtok, papp);

		// If the availability is less than the amount required...
		if (avail < pusage->GetAmount()) {
			logger->Debug("Check availability: Exceeding request for <%s>"
					"[USG:%" PRIu64 " | AV:%" PRIu64 " | TOT:%" PRIu64 "] ",
					rsrc_path->ToString().c_str(), pusage->GetAmount(), avail,
					QueryStatus(pusage->GetResourcesList(), RA_TOTAL));
			return RA_ERR_USAGE_EXC;
		}
	}

	return RA_SUCCESS;
}

inline ResourceAccounter::ExitCode_t ResourceAccounter::GetAppUsagesByView(
		br::RViewToken_t vtok,
		AppUsagesMapPtr_t & apps_usages) {
	// Get the map of all the Apps/EXCs resource usages
	// (default system resource state view)
	AppUsagesViewsMap_t::iterator view_it;
	if (vtok == 0) {
		assert(sys_usages_view);
		apps_usages = sys_usages_view;
		return RA_SUCCESS;
	}

	// "Alternate" state view
	view_it = usages_per_views.find(vtok);
	if (view_it == usages_per_views.end()) {
		logger->Error("Application usages:"
				"Cannot find the resource state view referenced by %d",	vtok);
		return RA_ERR_MISS_VIEW;
	}

	// Set the the map
	apps_usages = view_it->second;
	return RA_SUCCESS;
}

/************************************************************************
 *                   RESOURCE MANAGEMENT                                *
 ************************************************************************/

br::ResourcePath const & ResourceAccounter::GetPrefixPath() const {
	return *(r_prefix_path.get());
}

br::ResourcePtr_t ResourceAccounter::RegisterResource(
		std::string const & strpath,
		std::string const & units,
		uint64_t amount) {

	// Build a resource path object (from the string)
	ResourcePathPtr_t ppath = std::make_shared<br::ResourcePath>(strpath);
	if (!ppath) {
		logger->Fatal("Register R<%s>: Invalid resource path",
				strpath.c_str());
		return nullptr;
	}

	// Insert a new resource in the tree
	br::ResourcePtr_t pres(resources.insert(*(ppath.get())));
	if (!pres) {
		logger->Crit("Register R<%s>: "
				"Unable to allocate a new resource descriptor",
				strpath.c_str());
		return nullptr;
	}
	pres->SetTotal(br::ConvertValue(amount, units));
	pres->SetPath(strpath);
	logger->Debug("Register R<%s>: Total = %llu %s",
			strpath.c_str(), pres->Total(), units.c_str());

	// Insert the path in the paths set
	r_paths.emplace(strpath, ppath);
	path_max_len = std::max((int) path_max_len, (int) strpath.length());

	// Track the number of resources per type
	br::ResourceIdentifier::Type_t type = ppath->Type();
	if (r_count.find(type) == r_count.end()) {
		r_count.insert(std::pair<br::Resource::Type_t, uint16_t>(type, 1));
		r_types.push_back(type);
	}
	else
		++r_count[type];

	logger->Debug("Register R<%s>: Total = %llu %s DONE (c[%d]=%d)",
			strpath.c_str(), Total(strpath), units.c_str(),
			type, r_count[type]);

	return pres;
}

ResourceAccounter::ExitCode_t ResourceAccounter::UpdateResource(
		std::string const & _path,
		std::string const & _units,
		uint64_t _amount) {
	uint64_t availability;
	uint64_t reserved;
	std::unique_lock<std::mutex> status_ul(status_mtx, std::defer_lock);

	// Lookup for the resource to be updated
	br::ResourcePathPtr_t ppath(GetPath(_path));
	if (ppath == nullptr) {
		logger->Fatal("Updating resource FAILED "
			"(Error: path [%s] does not reference a specific resource",
			_path.c_str());
		return RA_ERR_INVALID_PATH;
	}

	// Get the path of the resource to update
	br::ResourcePtr_t pres(GetResource(ppath));
	if (pres == nullptr) {
		logger->Fatal("Updating resource FAILED "
			"(Error: resource [%s] not found",
			ppath->ToString().c_str());
		return RA_ERR_NOT_REGISTERED;
	}

	// Resource accounter is not ready now
	status_ul.lock();
	while (status != State::READY)
		status_cv.wait(status_ul);
	status = State::NOT_READY;
	status_ul.unlock();
	status_cv.notify_all();

	// If the required amount is <= 1, the resource is off-lined
	if (_amount == 0)
		pres->SetOffline();

	// Check if the required amount is compliant with the total defined at
	// registration time
	availability = br::ConvertValue(_amount, _units);
	if (pres->Total() < availability) {
		logger->Error("Updating resource FAILED "
				"(Error: availability [%d] exceeding registered amount [%d]",
				availability, pres->Total());
		SetReady();
		return RA_ERR_OVERFLOW;
	}

	// Setup reserved amount of resource, considering the units
	reserved = pres->Total() - availability;
	ReserveResources(ppath, reserved);
	pres->SetOnline();

	// Back to READY
	SetReady();

	return RA_SUCCESS;
}


inline ResourceAccounter::ExitCode_t ResourceAccounter::_BookResources(
		ba::AppSPtr_t papp,
		br::UsagesMapPtr_t const & rsrc_usages,
		br::RViewToken_t vtok) {
	return IncBookingCounts(rsrc_usages, papp, vtok);
}

ResourceAccounter::ExitCode_t ResourceAccounter::BookResources(
		ba::AppSPtr_t papp,
		br::UsagesMapPtr_t const & rsrc_usages,
		br::RViewToken_t vtok) {

	// Check to avoid null pointer segmentation fault
	if (!papp) {
		logger->Fatal("Booking: Null pointer to the application descriptor");
		return RA_ERR_MISS_APP;
	}

	// Check that the set of resource usages is not null
	if ((!rsrc_usages) || (rsrc_usages->empty())) {
		logger->Fatal("Booking: Empty resource usages set");
		return RA_ERR_MISS_USAGES;
	}

	// Check resource availability (if this is not a sync session)
	// TODO refine this check to consider possible corner cases:
	// 1. scheduler running while in sync
	// 2. resource availability decrease while in sync
	if (!Synching()) {
		if (CheckAvailability(rsrc_usages, vtok) == RA_ERR_USAGE_EXC) {
			logger->Debug("Booking: Cannot allocate the resource set");
			return RA_ERR_USAGE_EXC;
		}
	}

	// Increment the booking counts and save the reference to the resource set
	// used by the application
	return IncBookingCounts(rsrc_usages, papp, vtok);
}

void ResourceAccounter::ReleaseResources(
		ba::AppSPtr_t papp,
		br::RViewToken_t vtok) {
	std::unique_lock<std::mutex> sync_ul(status_mtx);
	// Sanity check
	if (!papp) {
		logger->Fatal("Release: Null pointer to the application descriptor");
		return;
	}

	if (rsrc_per_views.find(vtok) == rsrc_per_views.end()) {
		logger->Debug("Release: Resource state view already cleared");
		return;
	}

	// Decrease resources in the sync view
	if (vtok == 0 && Synching())
		_ReleaseResources(papp, sync_ssn.view);

	// Decrease resources in the required view
	if (vtok != sync_ssn.view)
		_ReleaseResources(papp, vtok);
}

void ResourceAccounter::_ReleaseResources(
		ba::AppSPtr_t papp,
		br::RViewToken_t vtok) {
	// Get the map of applications resource usages related to the state view
	// referenced by 'vtok'
	AppUsagesMapPtr_t apps_usages;
	if (GetAppUsagesByView(vtok, apps_usages) == RA_ERR_MISS_VIEW) {
		logger->Fatal("Release: Resource view unavailable");
		return;
	}

	// Get the map of resource usages of the application
	AppUsagesMap_t::iterator usemap_it(apps_usages->find(papp->Uid()));
	if (usemap_it == apps_usages->end()) {
		logger->Debug("Release: resource set not assigned");
		return;
	}

	// Decrement resources counts and remove the usages map
	DecBookingCounts(usemap_it->second, papp, vtok);
	apps_usages->erase(papp->Uid());
	logger->Debug("Release: [%s] resource release terminated", papp->StrId());
}


ResourceAccounter::ExitCode_t  ResourceAccounter::ReserveResources(
		ResourcePathPtr_t ppath,
		uint64_t amount) {
	br::Resource::ExitCode_t rresult;
	br::ResourcePtrList_t const & rlist(
		resources.findList(*(ppath.get()), RT_MATCH_MIXED));
	logger->Info("Reserving [%" PRIu64 "] for [%s] resources...",
			amount, ppath->ToString().c_str());

	if (rlist.empty()) {
		logger->Error("Resource reservation FAILED "
				"(Error: resource [%s] not matching)",
				ppath->ToString().c_str());
		return RA_FAILED;
	}

	for (br::ResourcePtr_t r: rlist) {
		rresult = r->Reserve(amount);
		if (rresult != br::Resource::RS_SUCCESS) {
			logger->Warn("Reservation: Exceeding value [%" PRIu64 "] for [%s]",
				amount, ppath->ToString().c_str());
			return RA_FAILED;
		}
	}

	return RA_SUCCESS;
}

ResourceAccounter::ExitCode_t  ResourceAccounter::ReserveResources(
		std::string const & path,
		uint64_t amount) {
	br::ResourcePathPtr_t ppath(GetPath(path));
	logger->Info("Reserve: built %d from %s", ppath.get(), path.c_str());

	if (ppath == nullptr) {
		logger->Fatal("Reserve resource FAILED "
			"(Error: path [%s] does not reference a specific resource.",
			path.c_str());
		return RA_ERR_INVALID_PATH;
	}

	return ReserveResources(ppath, amount);
}


bool  ResourceAccounter::IsOfflineResource(ResourcePathPtr_t ppath) const {
	br::ResourcePtrList_t rlist;
	rlist = resources.findList(*(ppath.get()), RT_MATCH_MIXED);
	br::ResourcePtrListIterator_t rit = rlist.begin();

	logger->Debug("Check offline status for resources [%s]...",
			ppath->ToString().c_str());
	if (rit == rlist.end()) {
		logger->Error("Check offline FAILED "
				"(Error: resource [%s] not matching)",
				ppath->ToString().c_str());
		return true;
	}
	for ( ; rit != rlist.end(); ++rit) {
		if (!(*rit)->IsOffline())
			return false;
	}

	return true;
}

ResourceAccounter::ExitCode_t  ResourceAccounter::OfflineResources(
		std::string const & path) {
	br::ResourcePtrList_t rlist(GetResources(path));
	br::ResourcePtrListIterator_t rit = rlist.begin();

	logger->Info("Offlining resources [%s]...", path.c_str());
	if (rit == rlist.end()) {
		logger->Error("Resource offlining FAILED "
				"(Error: resource [%s] not matching)",
				path.c_str());
		return RA_FAILED;
	}
	for ( ; rit != rlist.end(); ++rit) {
		(*rit)->SetOffline();
	}

	return RA_SUCCESS;
}

ResourceAccounter::ExitCode_t  ResourceAccounter::OnlineResources(
		std::string const & path) {
	br::ResourcePtrList_t rlist(GetResources(path));
	br::ResourcePtrListIterator_t rit = rlist.begin();

	logger->Info("Onlining resources [%s]...", path.c_str());
	if (rit == rlist.end()) {
		logger->Error("Resource offlining FAILED "
				"(Error: resource [%s] not matching)");
		return RA_FAILED;
	}
	for ( ; rit != rlist.end(); ++rit) {
		(*rit)->SetOnline();
	}

	return RA_SUCCESS;
}

/************************************************************************
 *                   STATE VIEWS MANAGEMENT                             *
 ************************************************************************/

ResourceAccounter::ExitCode_t ResourceAccounter::GetView(
		std::string req_path,
		br::RViewToken_t & token) {
	std::unique_lock<std::mutex> status_ul(status_mtx);
	while (status != State::READY) {
		status_cv.wait(status_ul);
	}
	return _GetView(req_path, token);
}

ResourceAccounter::ExitCode_t ResourceAccounter::_GetView(
		std::string req_path,
		br::RViewToken_t & token) {
	// Null-string check
	if (req_path.empty()) {
		logger->Error("GetView: Missing a valid string");
		return RA_ERR_MISS_PATH;
	}

	// Token
	token = std::hash<std::string>()(req_path);
	logger->Debug("GetView: New resource state view. Token = %d", token);

	// Allocate a new view for the applications resource usages
	usages_per_views.insert(std::pair<br::RViewToken_t, AppUsagesMapPtr_t>(token,
				AppUsagesMapPtr_t(new AppUsagesMap_t)));

	//Allocate a new view for the set of resources allocated
	rsrc_per_views.insert(std::pair<br::RViewToken_t, ResourceSetPtr_t>(token,
				ResourceSetPtr_t(new ResourceSet_t)));

	return RA_SUCCESS;
}

ResourceAccounter::ExitCode_t ResourceAccounter::PutView(br::RViewToken_t vtok) {
	std::unique_lock<std::mutex> status_ul(status_mtx);
	while (status != State::READY) {
		status_cv.wait(status_ul);
	}
	return _PutView(vtok);
}

ResourceAccounter::ExitCode_t ResourceAccounter::_PutView(
		br::RViewToken_t vtok) {
	// Do nothing if the token references the system state view
	if (vtok == sys_view_token) {
		logger->Warn("PutView: Cannot release the system resources view");
		return RA_ERR_UNAUTH_VIEW;
	}

	// Get the resource set using the referenced view
	ResourceViewsMap_t::iterator rviews_it(rsrc_per_views.find(vtok));
	if (rviews_it == rsrc_per_views.end()) {
		logger->Error("PutView: Cannot find resource view token %d", vtok);
		return RA_ERR_MISS_VIEW;
	}

	// For each resource delete the view
	ResourceSet_t::iterator rsrc_set_it(rviews_it->second->begin());
	ResourceSet_t::iterator rsrc_set_end(rviews_it->second->end());
	for (; rsrc_set_it != rsrc_set_end; ++rsrc_set_it)
		(*rsrc_set_it)->DeleteView(vtok);

	// Remove the map of Apps/EXCs resource usages and the resource reference
	// set of this view
	usages_per_views.erase(vtok);
	rsrc_per_views.erase(vtok);

	logger->Debug("PutView: view %d cleared", vtok);
	logger->Debug("PutView: %d resource set and %d usages per view currently managed",
			rsrc_per_views.size(), usages_per_views.erase(vtok));

	return RA_SUCCESS;
}

br::RViewToken_t ResourceAccounter::SetView(br::RViewToken_t vtok) {
	std::unique_lock<std::mutex> status_ul(status_mtx);
	while (status != State::READY) {
		status_cv.wait(status_ul);
	}
	return _SetView(vtok);
}

br::RViewToken_t ResourceAccounter::_SetView(br::RViewToken_t vtok) {
	br::RViewToken_t old_sys_vtok;

	// Do nothing if the token references the system state view
	if (vtok == sys_view_token) {
		logger->Debug("SetView: View %d is already the system state!", vtok);
		return sys_view_token;
	}

	// Set the system state view pointer to the map of applications resource
	// usages of this view and point to
	AppUsagesViewsMap_t::iterator us_view_it(usages_per_views.find(vtok));
	if (us_view_it == usages_per_views.end()) {
		logger->Fatal("SetView: View %d unknown", vtok);
		return sys_view_token;
	}

	// Save the old view token, update the system state view token and the map
	// of Apps/EXCs resource usages
	old_sys_vtok    = sys_view_token;
	sys_view_token  = vtok;
	sys_usages_view = us_view_it->second;

	// Put the old view
	_PutView(old_sys_vtok);

	logger->Info("SetView: View %d is the new system state view.",
			sys_view_token);
	logger->Debug("SetView: %d resource set and %d usages per view currently managed",
			rsrc_per_views.size(), usages_per_views.erase(vtok));
	return sys_view_token;
}

void ResourceAccounter::SetScheduledView(br::RViewToken_t svt) {
	// Update the new scheduled view
	br::RViewToken_t old_svt = sch_view_token;
	sch_view_token = svt;

	// Release the old scheduled view if it is not the current system view
	if (old_svt != sys_view_token)
		_PutView(old_svt);
}


/************************************************************************
 *                   SYNCHRONIZATION SUPPORT                            *
 ************************************************************************/

ResourceAccounter::ExitCode_t ResourceAccounter::SyncStart() {
	std::unique_lock<std::mutex> status_ul(status_mtx);
	ResourceAccounter::ExitCode_t result;
	char tk_path[TOKEN_PATH_MAX_LEN];

	// Wait for a READY status
	while (status != State::READY) {
		status_cv.wait(status_ul);
	}
	// Synchronization has started
	status = State::SYNC;
	status_cv.notify_all();
	logger->Info("SyncMode: Start");

	// Build the path for getting the resource view token
	++sync_ssn.count;
	snprintf(tk_path, TOKEN_PATH_MAX_LEN, "%s%d", SYNC_RVIEW_PATH, sync_ssn.count);
	logger->Debug("SyncMode [%d]: Requiring resource state view for %s",
			sync_ssn.count, tk_path);

	// Get a resource state view for the synchronization
	result = _GetView(tk_path, sync_ssn.view);
	if (result != RA_SUCCESS) {
		logger->Fatal("SyncMode [%d]: Cannot get a resource state view",
				sync_ssn.count);
		_SyncAbort();
		return RA_ERR_SYNC_VIEW;
	}
	logger->Debug("SyncMode [%d]: Resource state view token = %d",
			sync_ssn.count, sync_ssn.view);

	// Init the view with the resource accounting of running applications
	return SyncInit();
}

// NOTE this method should be called while holding the sync session mutex
ResourceAccounter::ExitCode_t ResourceAccounter::SyncInit() {
	ResourceAccounter::ExitCode_t result;
	AppsUidMapIt apps_it;
	ba::AppSPtr_t papp;

	// Running Applications/ExC
	papp = am.GetFirst(ApplicationStatusIF::RUNNING, apps_it);
	for ( ; papp; papp = am.GetNext(ApplicationStatusIF::RUNNING, apps_it)) {
		logger->Info("SyncInit: [%s] current AWM: %d",
				papp->StrId(),
				papp->CurrentAWM()->Id());

		// Re-acquire the resources (these should not have a "Next AWM"!)
		result = _BookResources(
				papp, papp->CurrentAWM()->GetResourceBinding(), sync_ssn.view);
		if (result != RA_SUCCESS) {
			logger->Fatal("SyncInit [%d]: Resource booking failed for %s."
					" Aborting sync session...", sync_ssn.count, papp->StrId());
			_SyncAbort();
			return RA_ERR_SYNC_INIT;
		}
	}

	logger->Info("SyncInit [%d]: Initialization finished", sync_ssn.count);
	return RA_SUCCESS;
}

ResourceAccounter::ExitCode_t ResourceAccounter::SyncAcquireResources(
		ba::AppSPtr_t const & papp) {
	ResourceAccounter::ExitCode_t result = RA_SUCCESS;

	// Check that we are in a synchronized session
	if (!Synching()) {
		logger->Error("SyncMode [%d]: Session not open", sync_ssn.count);
		return RA_ERR_SYNC_START;
	}

	// Check next AWM
	if (!papp->NextAWM()) {
		logger->Fatal("SyncMode [%d]: [%s] missing the next AWM",
				sync_ssn.count, papp->StrId());
		_SyncAbort();
		return RA_ERR_MISS_AWM;
	}

	// Acquire resources
	br::UsagesMapPtr_t const &usages(papp->NextAWM()->GetResourceBinding());
	result = _BookResources(papp, usages, sync_ssn.view);
	if (result != RA_SUCCESS) {
		logger->Fatal("SyncMode [%d]: [%s] resource booking failed",
				sync_ssn.count, papp->StrId());
		_SyncAbort();
		return result;
	}

	// Update AWM binding info (resource bitsets)
	logger->Debug("SyncMode [%d]: [%s] updating binding information",
				sync_ssn.count, papp->StrId());
	papp->NextAWM()->UpdateBindingInfo(sync_ssn.view);
	return result;
}

void ResourceAccounter::SyncAbort() {
	std::unique_lock<std::mutex> sync_ul(status_mtx);
	_SyncAbort();
}

// NOTE this method should be called while holding the sync session mutex
void ResourceAccounter::_SyncAbort() {
	_PutView(sync_ssn.view);
	SyncFinalize();
	logger->Error("SyncMode [%d]: Session aborted", sync_ssn.count);
}

ResourceAccounter::ExitCode_t ResourceAccounter::SyncCommit() {
	ResourceAccounter::ExitCode_t result = RA_SUCCESS;
	br::RViewToken_t view;

	if (!Synching()) {
		logger->Error("SynCommit: Synchronization not active");
		return RA_ERR_SYNC_START;
	}

	// Set the synchronization view as the new system one
	view = _SetView(sync_ssn.view);
	if (view != sync_ssn.view) {
		logger->Fatal("SyncCommit [%d]: "
				"Unable to set the new system resource state view",
				sync_ssn.count);
		_SyncAbort();
		return RA_ERR_SYNC_VIEW;
	}

	// Release the last scheduled view, by setting it to the system view
	SetScheduledView(sys_view_token);
	SyncFinalize();
	logger->Info("SyncCommit [%d]: Session closed", sync_ssn.count);

	// Log the status report
	PrintStatusReport();
	return result;
}

ResourceAccounter::ExitCode_t ResourceAccounter::SyncFinalize() {
	if (!Synching()) {
		logger->Error("SyncFinalize: Synchronization not active");
		return RA_ERR_SYNC_START;
	}

	std::unique_lock<std::mutex> status_ul(status_mtx);
	status = State::READY;
	status_cv.notify_all();
	return RA_SUCCESS;
}

void ResourceAccounter::SyncWait() {
	std::unique_lock<std::mutex> status_ul(status_mtx);
	while (status != State::READY) {
		status_cv.wait(status_ul);
	}
}

/************************************************************************
 *                   RESOURCE ACCOUNTING                                *
 ************************************************************************/

ResourceAccounter::ExitCode_t
ResourceAccounter::IncBookingCounts(
		br::UsagesMapPtr_t const & rsrc_usages,
		ba::AppSPtr_t const & papp,
		br::RViewToken_t vtok) {
	ResourceAccounter::ExitCode_t result;

	// Get the set of resources referenced in the view
	ResourceViewsMap_t::iterator rsrc_view(rsrc_per_views.find(vtok));
	assert(rsrc_view != rsrc_per_views.end());
	if (rsrc_view == rsrc_per_views.end()) {
		logger->Fatal("Booking: Invalid resource state view token [%ld]", vtok);
		return RA_ERR_MISS_VIEW;
	}
	ResourceSetPtr_t & rsrc_set(rsrc_view->second);

	// Get the map of resources used by the application (from the state view
	// referenced by 'vtok').
	AppUsagesMapPtr_t apps_usages;
	if (GetAppUsagesByView(vtok, apps_usages) == RA_ERR_MISS_VIEW) {
		logger->Fatal("Booking: No applications using resource in state view "
				"[%ld]", vtok);
		return RA_ERR_MISS_APP;
	}

	// Each application can hold just one resource usages set
	AppUsagesMap_t::iterator usemap_it(apps_usages->find(papp->Uid()));
	if (usemap_it != apps_usages->end()) {
		logger->Warn("Booking: [%s] currently using a resource set yet",
				papp->StrId());
		return RA_ERR_APP_USAGES;
	}

	// Book resources for the application
	for (auto & ru_entry: *(rsrc_usages.get())) {
		br::ResourcePathPtr_t const & rsrc_path(ru_entry.first);
		br::UsagePtr_t & pusage(ru_entry.second);
		logger->Debug("Booking: [%s] requires resource <%s>: [% " PRIu64 "] ",
				papp->StrId(), rsrc_path->ToString().c_str(), pusage->GetAmount());

		// Do booking for the current resource request
		result = DoResourceBooking(papp, pusage, vtok, rsrc_set);
		if (result != RA_SUCCESS)  {
			logger->Crit("Booking: unexpected fail! <%s> "
					"[USG:%" PRIu64 " | AV:%" PRIu64 " | TOT:%" PRIu64 "]",
				rsrc_path->ToString().c_str(), pusage->GetAmount(),
				Available(rsrc_path, MIXED, vtok, papp),
				Total(rsrc_path, MIXED));
			// Print the report table of the resource assignments
			PrintStatusReport();
		}

		assert(result == RA_SUCCESS);
		logger->Info("Booking: R<%s> SUCCESS "
				"[U:%" PRIu64 " | A:%" PRIu64 " | T:%" PRIu64 "]",
				rsrc_path->ToString().c_str(), pusage->GetAmount(),
				Available(rsrc_path, MIXED, vtok, papp),
				Total(rsrc_path, MIXED));
	}

	apps_usages->emplace(papp->Uid(), rsrc_usages);
	logger->Debug("Booking: [%s] now holds %d resources",
			papp->StrId(), rsrc_usages->size());

	return RA_SUCCESS;
}

ResourceAccounter::ExitCode_t ResourceAccounter::DoResourceBooking(
		ba::AppSPtr_t const & papp,
		br::UsagePtr_t & pusage,
		br::RViewToken_t vtok,
		ResourceSetPtr_t & rsrc_set) {
	bool first_resource = false;

	// Amount of resource to book and list of resource descriptors
	uint64_t requested = pusage->GetAmount();
	br::ResourcePtrListIterator_t it_bind(pusage->GetResourcesList().begin());
	br::ResourcePtrListIterator_t end_it(pusage->GetResourcesList().end());
	size_t num_rsrcs_left = pusage->GetResourcesList().size();
	uint64_t per_rsrc_allocated = 0;

	// Get the list of the bound resources
	for (; it_bind != end_it; ++it_bind) {
		// Break if the required resource has been completely allocated
		if (requested == 0)
			break;
		// Add the current resource binding to the set of resources used in
		// the view referenced by 'vtok'
		br::ResourcePtr_t & rsrc(*it_bind);
		rsrc_set->insert(rsrc);

		// Synchronization: booking according to scheduling decisions
		if (Synching()) {
			SyncResourceBooking(papp, rsrc, requested);
			continue;
		}

		// In case of "balanced" filling policy, spread the requested amount
		// of resource over all the resources of the given binding
		if (pusage->GetPolicy() == br::Usage::Policy::BALANCED)
			per_rsrc_allocated = requested / num_rsrcs_left;

		// Scheduling: allocate required resource among its bindings
		SchedResourceBooking(papp, rsrc, vtok, requested, per_rsrc_allocated);
		if ((requested != pusage->GetAmount()) && !first_resource) {
			// Keep track of the first resource granted from the bindings
			pusage->TrackFirstResource(papp, it_bind, vtok);
			first_resource = true;
		}
		--num_rsrcs_left;
	}

	// Keep track of the last resource granted from the bindings (only if we
	// are in the scheduling case)
	if (!Synching())
		pusage->TrackLastResource(papp, it_bind, vtok);

	// Critical error: The availability of resources mismatches the one
	// checked in the scheduling phase. This should never happen!
	if (requested != 0) {
		logger->Crit("DRBooking: Resource assignment mismatch");
		assert(requested != 0);
		return RA_ERR_USAGE_EXC;
	}

	return RA_SUCCESS;
}

bool ResourceAccounter::IsReshuffling(
		br::UsagesMapPtr_t const & pum_current,
		br::UsagesMapPtr_t const & pum_next) {
	br::ResourcePtrListIterator_t presa_it, presc_it;
	br::UsagesMap_t::iterator auit, cuit;
	br::ResourcePtr_t presa, presc;
	br::UsagePtr_t pua, puc;

	// Loop on resources
	for (cuit = pum_current->begin(), auit = pum_next->begin();
			cuit != pum_current->end() && auit != pum_next->end();
			++cuit, ++auit) {
		// Get the resource usages
		puc = (*cuit).second;
		pua = (*auit).second;

		// Loop on bindings
		presc = puc->GetFirstResource(presc_it);
		presa = pua->GetFirstResource(presa_it);
		while (presc && presa) {
			logger->Debug("Checking: curr [%s:%d] vs next [%s:%d]",
				presc->Name().c_str(),
				presc->ApplicationUsage(puc->owner_app, 0),
				presa->Name().c_str(),
				presc->ApplicationUsage(puc->owner_app, pua->status_view));
			// Check for resource binding differences
			if (presc->ApplicationUsage(puc->owner_app, 0) !=
					presc->ApplicationUsage(puc->owner_app,	pua->status_view)) {
				logger->Debug("AWM Shuffling detected");
				return true;
			}
			// Check next resource
			presc = puc->GetNextResource(presc_it);
			presa = pua->GetNextResource(presa_it);
		}
	}

	return false;
}

inline void ResourceAccounter::SchedResourceBooking(
		ba::AppSPtr_t const & papp,
		br::ResourcePtr_t & rsrc,
		br::RViewToken_t vtok,
		uint64_t & requested,
		uint64_t per_rsrc_allocated) {
	// Check the available amount in the current resource binding
	uint64_t available = rsrc->Available(papp, vtok);

	// If it is greater than the required amount, acquire the whole
	// quantity from the current resource binding, otherwise split
	// it among sibling resource bindings
	if ((per_rsrc_allocated >0) && (per_rsrc_allocated <= available))
		requested -= rsrc->Acquire(papp, per_rsrc_allocated, vtok);
	else if (requested < available)
		requested -= rsrc->Acquire(papp, requested, vtok);
	else
		requested -= rsrc->Acquire(papp, available, vtok);

	logger->Debug("DRBooking (sched): [%s] scheduled to use {%s}",
			papp->StrId(), rsrc->Name().c_str());
}

inline void ResourceAccounter::SyncResourceBooking(
		ba::AppSPtr_t const & papp,
		br::ResourcePtr_t & rsrc,
		uint64_t & requested) {
	// Skip the resource binding if the not assigned by the scheduler
	uint64_t sched_usage = rsrc->ApplicationUsage(papp, sch_view_token);
	if (sched_usage == 0) {
		logger->Debug("DRBooking (sync): no usage of {%s} scheduled for [%s]",
				rsrc->Name().c_str(), papp->StrId());
		return;
	}

	// Acquire the resource according to the amount assigned by the
	// scheduler
	requested -= rsrc->Acquire(papp, sched_usage, sync_ssn.view);
	logger->Debug("DRBooking (sync): %s acquires %s (%d left)",
			papp->StrId(), rsrc->Name().c_str(), requested);
}

void ResourceAccounter::DecBookingCounts(
		br::UsagesMapPtr_t const & app_usages,
		ba::AppSPtr_t const & papp,
		br::RViewToken_t vtok) {
	ExitCode_t ra_result;
	logger->Debug("DecCount: [%s] holds %d resources",
			papp->StrId(), app_usages->size());

	// Get the set of resources referenced in the view
	ResourceViewsMap_t::iterator rsrc_view(rsrc_per_views.find(vtok));
	if (rsrc_view == rsrc_per_views.end()) {
		logger->Fatal("DecCount: Invalid resource state view token [%ld]", vtok);
		return;
	}
	ResourceSetPtr_t & rsrc_set(rsrc_view->second);

	// Release the all the resources hold by the Application/EXC
	for (auto & ru_entry: *(app_usages.get())) {
		br::ResourcePathPtr_t const & rsrc_path(ru_entry.first);
		br::UsagePtr_t & pusage(ru_entry.second);
		// Release the resources bound to the current request
		ra_result = UndoResourceBooking(papp, pusage, vtok, rsrc_set);
		if (ra_result == RA_ERR_MISS_VIEW)
			return;
		logger->Debug("DecCount: [%s] has freed {%s} of %" PRIu64 "",
				papp->StrId(), rsrc_path->ToString().c_str(),
				pusage->GetAmount());
	}
}

ResourceAccounter::ExitCode_t ResourceAccounter::UndoResourceBooking(
		ba::AppSPtr_t const & papp,
		br::UsagePtr_t & pusage,
		br::RViewToken_t vtok,
		ResourceSetPtr_t & rsrc_set) {
	// Keep track of the amount of resource freed
	uint64_t usage_freed = 0;

	// For each resource binding release the amount allocated to the App/EXC
	assert(!pusage->GetResourcesList().empty());
	for (br::ResourcePtr_t & rsrc: pusage->GetResourcesList()) {
		if (usage_freed == pusage->GetAmount())
			break;

		// Release the quantity hold by the Application/EXC
		usage_freed += rsrc->Release(papp, vtok);

		// If no more applications are using this resource, remove it from
		// the set of resources referenced in the resource state view
		if ((rsrc_set) && (rsrc->ApplicationsCount() == 0))
			rsrc_set->erase(rsrc);
	}
	assert(usage_freed == pusage->GetAmount());
	return RA_SUCCESS;
}

/************************************************************************
 *                   COMMANDS HANDLING                                  *
 ************************************************************************/

int ResourceAccounter::CommandsCb(int argc, char *argv[]) {
	uint8_t cmd_offset = ::strlen(RESOURCE_ACCOUNTER_NAMESPACE) + 1;
	char * command_id  = argv[0] + cmd_offset;
	logger->Info("Processing command [%s]", command_id);

	// Set a new resource total quota
	if (!strncmp(CMD_SET_TOTAL, command_id, strlen(CMD_SET_TOTAL))) {
		if (argc != 3) {
			logger->Error("'%s' expecting 2 parameters.", CMD_SET_TOTAL);
			logger->Error("Ex: 'bq.ra.%s <resource_path> (e.g., sys0.cpu0.pe0)"
				" <new_total_value> (e.g. 90)'", CMD_SET_TOTAL);
			return 1;
		}
		return SetResourceTotalHandler(argv[1], argv[2]);
	}

	// Set the degradation value of the given resources
	if (!strncmp(CMD_NOTIFY_DEGRADATION, command_id, strlen(CMD_NOTIFY_DEGRADATION))) {
		if (!(argc % 2)) {
			logger->Error("'bq.ra.%s' expecting {resource path, value} pairs.",
				CMD_NOTIFY_DEGRADATION);
			logger->Error("Example: 'bq.ra.%s <resource_path> (e.g., sys0.cpu0.pe0)"
				" <degradation_percentage> (e.g. 10) ...'",
				CMD_NOTIFY_DEGRADATION);
			return 2;
		}
		return ResourceDegradationHandler(argc, argv);
	}

	logger->Error("Unexpected command: %s", command_id);

	return 0;
}

int ResourceAccounter::SetResourceTotalHandler(char * r_path, char * value) {
	uint64_t amount = atoi(value);

	ExitCode_t ra_result = UpdateResource(r_path, "", amount);
	if (ra_result != RA_SUCCESS) {
		logger->Error("SetResourceTotalHandler: "
			"cannot set quota %" PRIu64 " to [%s]", amount, r_path);
		return 2;
	}

	logger->Info("SetResourceTotalHandler: "
			"set quota %" PRIu64 " to [%s]", amount, r_path);
	PrintStatusReport(0, true);

	return 0;
}

int ResourceAccounter::ResourceDegradationHandler(int argc, char * argv[]) {
	int idx = 1;
	argc--;

	// Parsing the "<resource> <degradation_value>" pairs
	while (argc) {
		br::ResourcePtr_t rsrc(GetResource(argv[idx]));
		if (rsrc == nullptr) {
			logger->Error("Resource degradation: <%s> not a valid resource",
				argv[idx]);
			goto next_arg;
		}

		if (IsNumber(argv[idx+1])) {
			rsrc->UpdateDegradationPerc(atoi(argv[idx+1]));
			logger->Warn("Resource degradation: <%s> = %2d%% [mean=%.2f]",
				argv[idx],
				rsrc->CurrentDegradationPerc(),
				rsrc->MeanDegradationPerc());
		}
		else
			logger->Error("Resource degradation: <%s> not a valid value",
				argv[idx+1]);
next_arg:
		idx  += 2;
		argc -= 2;
	}

	return 0;
}


}   // namespace bbque
