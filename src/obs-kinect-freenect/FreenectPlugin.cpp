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

#include "FreenectPlugin.hpp"
#include "FreenectDevice.hpp"
#include <stdexcept>

void ErrorCallback(freenect_context* /*device*/, freenect_loglevel level, const char* message)
{
	switch (level)
	{
		case FREENECT_LOG_FATAL:   errorlog("freenect fatal error: %s", message); break;
		case FREENECT_LOG_ERROR:   errorlog("freenect error: %s", message); break;
		case FREENECT_LOG_WARNING: warnlog("freenect warning: %s", message); break;
		case FREENECT_LOG_NOTICE:  infolog("freenect notice: %s", message); break;
		case FREENECT_LOG_INFO:    infolog("freenect info: %s", message); break;
		case FREENECT_LOG_DEBUG:   debuglog("freenect debug log: %s", message); break;
		case FREENECT_LOG_SPEW:    debuglog("freenect spew log: %s", message); break;
		case FREENECT_LOG_FLOOD:   debuglog("freenect flood log: %s", message); break;
	}
}

KinectFreenectPlugin::KinectFreenectPlugin() :
m_context(nullptr)
{
	if (freenect_init(&m_context, nullptr) != 0)
		throw std::runtime_error("failed to initialize freenect context");

#ifdef DEBUG
	freenect_loglevel logLevel = FREENECT_LOG_DEBUG;
#else
	freenect_loglevel logLevel = FREENECT_LOG_INFO;
#endif

	freenect_set_log_level(m_context, logLevel);
	freenect_set_log_callback(m_context, &ErrorCallback);

	// we're not supporting audio for now
	freenect_select_subdevices(m_context, static_cast<freenect_device_flags>(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));
}

KinectFreenectPlugin::~KinectFreenectPlugin()
{
	if (freenect_shutdown(m_context) < 0)
		warnlog("freenect shutdown failed");
}

std::string KinectFreenectPlugin::GetUniqueName() const
{
	return "KinectV1-Freenect";
}

std::vector<std::unique_ptr<KinectDevice>> KinectFreenectPlugin::Refresh() const
{
	std::vector<std::unique_ptr<KinectDevice>> devices;

	freenect_device_attributes* attributes;
	int deviceCount = freenect_list_device_attributes(m_context, &attributes);
	for (int i = 0; i < deviceCount; ++i)
	{
		try
		{
			freenect_device* device;
			if (freenect_open_device_by_camera_serial(m_context, &device, attributes->camera_serial) == 0)
				devices.emplace_back(std::make_unique<KinectFreenectDevice>(device, attributes->camera_serial));
			else
				warnlog("failed to open Kinect #%d", i);
		}
		catch (const std::exception& e)
		{
			warnlog("failed to open Kinect #%d: %s", i, e.what());
		}

		attributes = attributes->next;
	}

	return devices;
}
