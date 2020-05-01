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

#pragma once

#ifndef OBS_KINECT_PLUGIN_KINECTDEVICESDK10
#define OBS_KINECT_PLUGIN_KINECTDEVICESDK10

#include "KinectDevice.hpp"
#include "Win32Helper.hpp"
#include <combaseapi.h>
#include <NuiApi.h>

class KinectDeviceSdk10 : public KinectDevice
{
	public:
		KinectDeviceSdk10(int sensorId);
		~KinectDeviceSdk10() = default;

	private:
		void SetServicePriority(ProcessPriority priority) override;
		void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr) override;

		static ColorFrameData RetrieveColorFrame(INuiSensor* sensor, HANDLE colorStream, std::int64_t* timestamp);
		static DepthFrameData RetrieveDepthFrame(INuiSensor* sensor, HANDLE depthStream, std::int64_t* timestamp);
		static DepthMappingFrameData RetrieveDepthMappingFrame(INuiSensor* sensor, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame, std::vector<LONG>& tempMemory);
		static void ExtractDepth(DepthFrameData& depthFrame);

		ReleasePtr<INuiSensor> m_kinectSensor;
		bool m_hasRequestedPrivilege;
};

#endif
