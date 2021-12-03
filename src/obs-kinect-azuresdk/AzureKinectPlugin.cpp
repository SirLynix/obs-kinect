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

#include "AzureKinectPlugin.hpp"
#include "AzureKinectDevice.hpp"
#include <k4a/k4a.hpp>

void ErrorCallback(void* /*context*/, k4a_log_level_t level, const char* file, const int line, const char* message)
{
	switch (level)
	{
		case K4A_LOG_LEVEL_CRITICAL:
		case K4A_LOG_LEVEL_ERROR:
			errorlog("SDK error: %s (in %s:%d)", message, file, line);
			break;

		case K4A_LOG_LEVEL_WARNING:
			warnlog("SDK warning: %s (in %s:%d)", message, file, line);
			break;

		case K4A_LOG_LEVEL_INFO:
		case K4A_LOG_LEVEL_TRACE:
			infolog("SDK info: %s (in %s:%d)", message, file, line);
			break;

		case K4A_LOG_LEVEL_OFF:
		default:
			break;
	}
}


AzureKinectPlugin::AzureKinectPlugin()
{
#ifdef DEBUG
	k4a_log_level_t logLevel = K4A_LOG_LEVEL_INFO;
#else
	k4a_log_level_t logLevel = K4A_LOG_LEVEL_WARNING;
#endif

	k4a_set_debug_message_handler(&ErrorCallback, nullptr, logLevel);

#if HAS_BODY_TRACKING
	ObsLibPtr bodyTrackingLib(os_dlopen("k4abt"));

	if (bodyTrackingLib)
	{
		if (LoadBodyTrackingSdk(bodyTrackingLib.get()))
			m_bodyTrackingLib = std::move(bodyTrackingLib);
	}
#endif
}

AzureKinectPlugin::~AzureKinectPlugin()
{
	k4a_set_debug_message_handler(nullptr, nullptr, K4A_LOG_LEVEL_OFF);
}

std::string AzureKinectPlugin::GetUniqueName() const
{
	return "Azure Kinect";
}

std::vector<std::unique_ptr<KinectDevice>> AzureKinectPlugin::Refresh() const
{
	std::vector<std::unique_ptr<KinectDevice>> devices;
	try
	{
		std::uint32_t deviceCount = k4a::device::get_installed_count();

		for (std::uint32_t i = 0; i < deviceCount; ++i)
		{
			try
			{
				devices.emplace_back(std::make_unique<AzureKinectDevice>(i));
			}
			catch (const std::exception& e)
			{
				warnlog("failed to open Azure Kinect #%d: %s", i, e.what());
			}
		}
	}
	catch (const std::exception& e)
	{
		warnlog("%s", e.what());
	}

	return devices;
}
