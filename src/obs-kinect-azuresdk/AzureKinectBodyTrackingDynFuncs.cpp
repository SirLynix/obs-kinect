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

#include "AzureKinectBodyTrackingDynFuncs.hpp"
#include "AzureHelper.hpp"
#include <util/platform.h>
#include <stdexcept>

namespace
{
	bool s_bodyTrackingSdkLoaded = false;
}

bool LoadBodyTrackingSdk(void* obsModule)
{
	auto LoadFunc = [&](auto& funcPtr, const char* name)
	{
		funcPtr = reinterpret_cast<std::remove_reference_t<decltype(funcPtr)>>(os_dlsym(obsModule, name));
		if (!funcPtr)
			throw std::runtime_error("failed to load " + std::string(name));
	};

	try
	{
#define OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC(Ret, Name, ...) LoadFunc(Name, #Name);
		OBS_KINECT_AZURE_SDK_BODY_TRACKING_FOREACH_FUNC(OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC)
#undef OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC
	}
	catch (const std::exception& e)
	{
		errorlog("failed to load Azure Kinect Body Tracking SDK: %s", e.what());
		UnloadBodyTrackingSdk(); //< reset every function pointer to nullptr
		return false;
	}

	s_bodyTrackingSdkLoaded = true;

	return true;
}

bool IsBodyTrackingSdkLoaded()
{
	return s_bodyTrackingSdkLoaded;
}

void UnloadBodyTrackingSdk()
{
#define OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC(Ret, Name, ...) Name = nullptr;
	OBS_KINECT_AZURE_SDK_BODY_TRACKING_FOREACH_FUNC(OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC)
#undef OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC

	s_bodyTrackingSdkLoaded = false;
}

#define OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC(Ret, Name, ...) Ret (*Name)(__VA_ARGS__) = nullptr;
OBS_KINECT_AZURE_SDK_BODY_TRACKING_FOREACH_FUNC(OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC)
#undef OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC
