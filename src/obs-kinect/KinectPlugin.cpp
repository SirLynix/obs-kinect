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
#include "KinectPluginImpl.hpp"
#include "KinectDevice.hpp"

KinectPlugin::~KinectPlugin()
{
	Close();
}

void KinectPlugin::Close()
{
	m_impl.reset();
	m_lib.reset();
}

const std::string& KinectPlugin::GetUniqueName() const
{
	return m_uniqueName;
}

bool KinectPlugin::IsOpen() const
{
	return m_impl != nullptr;
}

bool KinectPlugin::Open(const char* path)
{
	ObsLibPtr lib(os_dlopen(path));
	if (!lib)
		return false;

	using CreatePlugin = KinectPluginImpl * (*)(std::uint32_t version);

	CreatePlugin createImpl = static_cast<CreatePlugin>(os_dlsym(lib.get(), "ObsKinect_CreatePlugin"));
	if (!createImpl)
	{
		warn("failed to get ObsKinect_CreatePlugin symbol, dismissing %s", path);
		return false;
	}

	m_impl.reset(createImpl(OBSKINECT_VERSION));
	if (!m_impl)
	{
		warn("failed to get plugin implementation for %s, dismissing", path);
		return false;
	}

	m_lib = std::move(lib);
	m_uniqueName = m_impl->GetUniqueName();

	return true;
}

std::vector<std::unique_ptr<KinectDevice>> KinectPlugin::Refresh() const
{
	assert(IsOpen());

	return m_impl->Refresh();
}
