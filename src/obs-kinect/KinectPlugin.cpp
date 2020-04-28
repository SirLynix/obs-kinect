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

#include "KinectPlugin.hpp"
#include "KinectDevice.hpp"

void KinectPlugin::Close()
{
	m_lib.reset();
	m_getUniqueNameFunc = nullptr;
	m_refreshFunc = nullptr;
}

const std::string& KinectPlugin::GetUniqueName() const
{
	return m_uniqueName;
}

bool KinectPlugin::IsOpen() const
{
	return m_lib != nullptr;
}

bool KinectPlugin::Open(const char* path)
{
	ObsLibPtr lib(os_dlopen(path));
	if (!lib)
		return false;

	m_getUniqueNameFunc = static_cast<GetUniqueNameFunc>(os_dlsym(lib.get(), "ObsKinect_GetUniqueName"));
	if (!m_getUniqueNameFunc)
	{
		warn("failed to get ObsKinect_GetUniqueName symbol, dismissing %s", path);
		return false;
	}

	m_refreshFunc = static_cast<RefreshFunc>(os_dlsym(lib.get(), "ObsKinect_Refresh"));
	if (!m_refreshFunc)
	{
		warn("failed to get ObsKinect_Refresh symbol, dismissing %s", path);
		return false;
	}

	m_uniqueName = m_getUniqueNameFunc();

	m_lib = std::move(lib);
	return true;
}

std::vector<std::unique_ptr<KinectDevice>> KinectPlugin::Refresh()
{
	assert(m_lib);

	KinectPluginRefresh refreshData;
	m_refreshFunc(&refreshData);

	std::vector<std::unique_ptr<KinectDevice>> deviceList = std::move(refreshData.devices);
	return deviceList;
}
