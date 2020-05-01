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

#include "Helper.hpp"
#include "KinectPlugin.hpp"
#include "KinectDeviceSdk10.hpp"
#include <memory>
#include <vector>

extern "C"
{
	OBSKINECT_EXPORT const char* ObsKinect_GetUniqueName()
	{
		return "KinectSDK1.0";
	}

	OBSKINECT_EXPORT void ObsKinect_Refresh(KinectPluginRefresh* refreshData)
	{
		try
		{
			int count;
			if (FAILED(NuiGetSensorCount(&count)))
				throw std::runtime_error("NuiGetSensorCount failed");

			for (int i = 0; i < count; ++i)
			{
				try
				{
					refreshData->devices.emplace_back(std::make_unique<KinectDeviceSdk10>(i));
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
	}
}
