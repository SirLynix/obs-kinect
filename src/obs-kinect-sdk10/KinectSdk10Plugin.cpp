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

#include "KinectSdk10Plugin.hpp"
#include "KinectSdk10Device.hpp"

std::string KinectSdk10Plugin::GetUniqueName() const
{
	return "KinectSDK1.0";
}

std::vector<std::unique_ptr<KinectDevice>> KinectSdk10Plugin::Refresh() const
{
	std::vector<std::unique_ptr<KinectDevice>> devices;
	try
	{
		int count;
		if (FAILED(NuiGetSensorCount(&count)))
			throw std::runtime_error("NuiGetSensorCount failed");

		for (int i = 0; i < count; ++i)
		{
			try
			{
				devices.emplace_back(std::make_unique<KinectSdk10Device>(i));
			}
			catch (const std::exception& e)
			{
				warn("failed to open Kinect #%d: %s", i, e.what());
			}
		}
	}
	catch (const std::exception& e)
	{
		warn("%s", e.what());
	}

	return devices;
}
