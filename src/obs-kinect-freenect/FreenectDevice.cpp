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

#include "FreenectDevice.hpp"
#include <util/threading.h>
#include <array>
#include <sstream>

KinectFreenectDevice::KinectFreenectDevice(freenect_device* device, const char* serial) :
m_device(device)
{
	SetSupportedSources(Source_Color | Source_ColorMappedDepth | Source_Depth | Source_Infrared);
	SetUniqueName("Kinect " + std::string(serial));
}

KinectFreenectDevice::~KinectFreenectDevice()
{
	StopCapture(); //< Ensure thread has joined before closing the device

	freenect_close_device(m_device);
}

void KinectFreenectDevice::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& error)
{
	os_set_thread_name("KinectDeviceFreenect");

	try
	{
		//TODO
	}
	catch (const std::exception&)
	{
		error = std::current_exception();
	}

	{
		std::unique_lock<std::mutex> lk(m);
		cv.notify_all();
	} // m & cv no longer exists from here

	if (error)
		return;

	infolog("exiting thread");
}
