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

#include "Freenect2Plugin.hpp"
#include "Freenect2Device.hpp"

std::string KinectFreenect2Plugin::GetUniqueName() const
{
	return "KinectV2-Freenect2";
}

std::vector<std::unique_ptr<KinectDevice>> KinectFreenect2Plugin::Refresh() const
{
	std::vector<std::unique_ptr<KinectDevice>> devices;

	try
	{
		int deviceCount = m_freenect.enumerateDevices();
		for (int i = 0; i < deviceCount; ++i)
		{
			try
			{
				if (libfreenect2::Freenect2Device* device = m_freenect.openDevice(i))
					devices.emplace_back(std::make_unique<KinectFreenect2Device>(device));
				else
					warnlog("failed to open Kinect #%d", i);
			}
			catch (const std::exception& e)
			{
				warnlog("failed to open Kinect #%d: %s", i, e.what());
			}
		}
	}
	catch (const std::exception& e)
	{
		warnlog("%s", e.what());
	}

	return devices;
}
