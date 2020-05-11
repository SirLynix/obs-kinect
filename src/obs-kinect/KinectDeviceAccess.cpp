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

#include "KinectDeviceAccess.hpp"

KinectDeviceAccess::KinectDeviceAccess(KinectDevice& owner, KinectDevice::AccessData* accessData) :
m_owner(&owner),
m_data(accessData)
{
}

KinectDeviceAccess::KinectDeviceAccess(KinectDeviceAccess&& access) noexcept
{
	m_owner = access.m_owner;
	m_data = access.m_data;

	access.m_owner = nullptr;
	access.m_data = nullptr;
}

KinectDeviceAccess::~KinectDeviceAccess()
{
	if (m_owner)
		m_owner->ReleaseAccess(m_data);
}

SourceFlags KinectDeviceAccess::GetEnabledSourceFlags() const
{
	return m_data->enabledSources;
}

KinectFrameConstPtr KinectDeviceAccess::GetLastFrame()
{
	return m_owner->GetLastFrame();
}

void KinectDeviceAccess::SetEnabledSourceFlags(SourceFlags enabledSources)
{
	m_data->enabledSources = enabledSources;
	m_owner->UpdateEnabledSources();
}

void KinectDeviceAccess::UpdateDeviceParameters(obs_data_t* settings)
{
	m_owner->UpdateDeviceParameters(m_data, settings);
}

KinectDeviceAccess& KinectDeviceAccess::operator=(KinectDeviceAccess&& access) noexcept
{
	std::swap(m_owner, access.m_owner);
	std::swap(m_data, access.m_data);

	return *this;
}
