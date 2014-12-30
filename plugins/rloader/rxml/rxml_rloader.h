/*
 * Copyright (C) 2014  Politecnico di Milano
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

#ifndef BBQUE_RXML_RLOADER_H_
#define BBQUE_RXML_RLOADER_H_

#include "bbque/plugins/recipe_loader.h"

#include <rapidxml.hpp>

#include "bbque/modules_factory.h"
#include "bbque/plugin_manager.h"
#include "bbque/plugins/plugin.h"
#include "bbque/utils/logging/logger.h"
#include "bbque/utils/attributes_container.h"

#define MODULE_NAMESPACE RECIPE_LOADER_NAMESPACE".rxml"
#define MODULE_CONFIG RECIPE_LOADER_CONFIG".rxml"

namespace bu = bbque::utils;

using bbque::app::Application;
using bbque::app::AppPtr_t;
using bbque::app::AwmPtr_t;
using bbque::app::WorkingMode;
using bbque::utils::AttributesContainer;

// Parameters received by the PluginManager on create calls
struct PF_ObjectParams;

namespace bbque { namespace plugins {

// Return code for internal purpose
#define __RSRC_SUCCESS 		0x0
#define __RSRC_WEAK_LOAD 	0x1
#define __RSRC_FORMAT_ERR 	0x2

// Max string length for the plugins data
#define PDATA_MAX_LEN 	20

/**
 * @brief RXML recipe loader plugin
 *
 */
class RXMLRecipeLoader: public RecipeLoaderIF {

public:

	/**
	 * @brief Method for creating the static plugin
	 */
	static void * Create(PF_ObjectParams *);

	/**
	 * @brief Method for destroying the static plugin
	 */
	static int32_t Destroy(void *);

	/**
	 * Default destructor
	 */
	virtual ~RXMLRecipeLoader();

	/**
	 * @see RecipeLoaderIF
	 */
	ExitCode_t LoadRecipe(std::string const & recipe_name, RecipePtr_t recipe);

	/**
	 * @see RecipeLoaderIF
	 */
	std::time_t LastModifiedTime(std::string const & recipe_name);

private:

	/**
	 * @brief System logger instance
	 */
	std::unique_ptr<bu::Logger> logger;

	/**
	 * Set true when the recipe loader has been configured.
	 * This is done by parsing a configuration file the first time a
	 * recipe loader is created.
	 */
	static bool configured;

	/**
	 * The directory path containing all the application recipes.
	 * Each recipe is an XML file named with suffix <tt>.recipe</tt>
	 */
	static std::string recipe_dir;

	/**
	 * Shared pointer to the recipe object
	 */
	RecipePtr_t recipe_ptr;

	/**
	 * The constructor
	 */
	RXMLRecipeLoader();

	/**
	 * @brief Load RecipeLoader configuration
	 *
	 * @param params @see PF_ObjectParams
	 * @return True if the configuration has been properly loaded and object
	 * could be built, false otherwise
	 */
	static bool Configure(PF_ObjectParams * params);

	/**
	 * @brief Lookup the platform section
	 *
	 * @param _xml_elem The XML element from which start the lookup of the
	 * section
	 * @return A pointer to the XML element from which start the platform
	 * section to consider
	 */
	rapidxml::xml_node<> * LoadPlatform(rapidxml::xml_node<> * xml_elem);

	/**
	 * @brief Parse the section containing working modes data
	 *
	 * @param xml_elem The XML element from which start searching the
	 * expected section tag
	 */
	ExitCode_t LoadWorkingModes(rapidxml::xml_node<> * xml_elem);

	/**
	 * @brief Parse the section containing resource usages data.
	 * The method has structured for recursive calls
	 *
	 * @param xml_elem The XML element from which start loading
	 * @param wm The working mode including this resource usages
	 * @param res_path Resource path (i.e. "arch.clusters.mem0")
	 * expected section tag
	 */
	uint8_t LoadResources(
			rapidxml::xml_node<> * xml_elem,
			AwmPtr_t & wm,
			std::string const & res_path);

	/**
	 * @brief Insert the resource in the working mode after checking if
	 * resource path have some matches between system resources
	 *
	 * @param wm The working mode to which add the resource usage
	 * @param res_path Resource path
	 * @param res_usage Resource usage value
	 * @return An internal error code
	 */
	uint8_t AppendToWorkingMode(
			AwmPtr_t & wm,
			std::string const & res_path,
			uint32_t res_usage);

	/**
	 * @brief Parse the resource data from the xml element and add the
	 * resource usage request to the working mode
	 *
	 * @param res_elem The XML element
	 * @param wm The working mode to which add the resource usage
	 * @param res_path Resource path
	 */
	uint8_t GetResourceAttributes(
			rapidxml::xml_node<> * res_elem,
			AwmPtr_t & wm,
			std::string & res_path);

	/**
	 * @brief Parse the section containing plugins specific data for the
	 * application or for a working mode
	 *
	 * @param container The object (usually Application or WorkingMode to
	 * which add the plugin specific data
	 * @param xml_node The RXML node from which start searching the
	 * expected section tag
	 */
	template<class T>
	void LoadPluginsData(T _container, rapidxml::xml_node<> * _xml_node);

	/**
	 * @brief Parse data from <plugin> element
	 *
	 * @param container The object (usually Application or WorkingMode to
	 * @param plugin_elem The XML element to parse for getting data
	 * which add the plugin specific data)
	 */
	template<class T>
	void ParsePluginTag(
			T container,
			rapidxml::xml_node<> * _plug_node);

	/**
	 * @brief Parse the data nested under <plugin>
	 *
	 * @param container The object to which append the plugins data (usually
	 * an Application or a WorkingMode)
	 * @param plugdata_node The XML Node to check for data
	 * @param plugin_name The name of the plugin
	 */
	template<class T>
	void GetPluginData(
			T _container,
			rapidxml::xml_node<> * plugdata_node,
			std::string const & _plug_name);

	/**
	 * @brief Parse the section containing constraints assertions
	 *
	 * @param xml_node The XML element from which start searching the
	 * expected section tag
	 */
	void LoadConstraints(rapidxml::xml_node<> * xml_node);

	/**
	* @brief Check if the first passed node exists or is a 0. This check
	* is done because some node are mandatory in the recipes.
	*
	* @param nodeToCheck The XML node to check existence
	* @param nodeToCheckName The name of the node to check the existence
	* @param nodeFather The father of the node to check the existence
	* This is useful to understand the missing node position
	* @throw parse_error when the node is missing
	*/
	void CheckMandatoryNode(
			rapidxml::xml_node<> * nodeToCheck,
			const char * nodeToCheckName,
			rapidxml::xml_node<> * nodeFather);

	/**
	* @brief Function used to load an attribute value from a node
	*
	* @param _nameAttribute The name of the attribute to load
	* @param mandatory True if the attribute is mandatory
	* @param node The node from which load the attribute
	*/
	std::string loadAttribute(
			const char * _nameAttribute,
			bool mandatory,
			rapidxml::xml_node<> * node);
};

} // namespace plugins

} // namespace bbque

#endif // BBQUE_RXML_RLOADER_H_
