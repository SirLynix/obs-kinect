/******************************************************************************
	Copyright (C) 2021 by Jérôme Leclercq <lynix680@gmail.com>

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

#include <obs-kinect/KinectPlugin.hpp>
#include <obs-kinect-core/KinectPluginImpl.hpp>
#include <obs-kinect-core/KinectDevice.hpp>

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

bool KinectPlugin::Open(const std::string& path)
{
#ifdef _WIN32
	ObsLibPtr lib(os_dlopen(path.c_str()));
#else
	ObsLibPtr lib(os_dlopen(("/app/plugins/lib/obs-plugins/" + path).c_str()));
#endif

	if (!lib)
		return false;

	using CreatePlugin = KinectPluginImpl * (*)(std::uint32_t version);

	CreatePlugin createImpl = reinterpret_cast<CreatePlugin>(os_dlsym(lib.get(), "ObsKinect_CreatePlugin"));
	if (!createImpl)
	{
		warnlog("failed to get ObsKinect_CreatePlugin symbol, dismissing %s", path.c_str());
		return false;
	}

	m_impl.reset(createImpl(OBSKINECT_VERSION));
	if (!m_impl)
	{
		warnlog("failed to get plugin implementation for %s, dismissing", path.c_str());
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
