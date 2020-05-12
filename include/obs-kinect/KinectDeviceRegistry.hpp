/******************************************************************************
	Copyright (C) 2020 by Jérôme Leclercq <lynix680@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#ifndef OBS_KINECT_PLUGIN_KINECTDEVICEREGISTRY
#define OBS_KINECT_PLUGIN_KINECTDEVICEREGISTRY

#include "Enums.hpp"
#include "KinectDevice.hpp"
#include "KinectPlugin.hpp"
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class KinectSource;

class OBSKINECT_API KinectDeviceRegistry
{
	friend KinectSource;

	public:
		using Callback = std::function<bool(const std::string& pluginName, const std::string& uniqueName, const KinectDevice& device)>;

		KinectDeviceRegistry() = default;
		KinectDeviceRegistry(const KinectDeviceRegistry&) = delete;
		KinectDeviceRegistry(KinectDeviceRegistry&&) = delete;
		~KinectDeviceRegistry() = default;

		void ForEachDevice(const Callback& callback) const;

		KinectDevice* GetDevice(const std::string& deviceName) const;

		void Refresh();

		bool RegisterPlugin(const char* path);

		KinectDeviceRegistry& operator=(const KinectDeviceRegistry&) = delete;
		KinectDeviceRegistry& operator=(KinectDeviceRegistry&&) = delete;

	private:
		void RegisterSource(KinectSource* source);
		void UnregisterSource(KinectSource* source);

		struct PluginData
		{
			struct Device
			{
				std::string uniqueName;
				std::unique_ptr<KinectDevice> device;
			};

			KinectPlugin plugin;
			std::vector<Device> devices; //< Order matters
		};

		std::unordered_map<std::string, KinectDevice*> m_deviceByName;
		std::unordered_set<KinectSource*> m_sources;
		std::vector<PluginData> m_plugins;
};

#endif
