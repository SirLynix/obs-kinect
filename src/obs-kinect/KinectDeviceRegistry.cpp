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

#include "KinectDeviceRegistry.hpp"
#include "KinectSource.hpp"
#include <cassert>

void KinectDeviceRegistry::ForEachDevice(const Callback& callback) const
{
	for (const auto& pluginData : m_plugins)
	{
		const std::string& pluginName = pluginData.plugin.GetUniqueName();
		for (const auto& deviceData : pluginData.devices)
		{
			if (!callback(pluginName, deviceData.uniqueName, *deviceData.device))
				return;
		}
	}
}

KinectDevice* KinectDeviceRegistry::GetDevice(const std::string& deviceName) const
{
	auto it = m_deviceByName.find(deviceName);
	if (it == m_deviceByName.end())
		return nullptr;

	return it->second;
}

void KinectDeviceRegistry::Refresh()
{
	for (KinectSource* source : m_sources)
		source->ClearDeviceAccess();

	m_deviceByName.clear();

	for (auto& pluginData : m_plugins)
	{
		pluginData.devices.clear();

		for (auto& devicePtr : pluginData.plugin.Refresh())
		{
			auto& deviceData = pluginData.devices.emplace_back();
			deviceData.device = std::move(devicePtr);
			deviceData.uniqueName = pluginData.plugin.GetUniqueName() + "_" + deviceData.device->GetUniqueName();

			assert(m_deviceByName.find(deviceData.uniqueName) == m_deviceByName.end());
			m_deviceByName.emplace(deviceData.uniqueName, deviceData.device.get());
		}
	}

	for (KinectSource* source : m_sources)
		source->RefreshDeviceAccess();
}

bool KinectDeviceRegistry::RegisterPlugin(const char* path)
{
	KinectPlugin newPlugin;
	if (!newPlugin.Open(path))
		return false;

	auto& pluginEntry = m_plugins.emplace_back();
	pluginEntry.plugin = std::move(newPlugin);

	return true;
}

void KinectDeviceRegistry::RegisterSource(KinectSource* source)
{
	assert(m_sources.find(source) == m_sources.end());
	m_sources.insert(source);
}

void KinectDeviceRegistry::UnregisterSource(KinectSource* source)
{
	assert(m_sources.find(source) != m_sources.end());
	m_sources.erase(source);
}
