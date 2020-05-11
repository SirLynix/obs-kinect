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

class KinectSdk10Device : public KinectDevice
{
	public:
		KinectSdk10Device(int sensorId);
		~KinectSdk10Device();

		obs_properties_t* CreateProperties() const override;

		INuiSensor* GetSensor() const;

	private:
		void ElevationThreadFunc();
		void HandleBoolParameterUpdate(const std::string& parameterName, bool value) override;
		void HandleDoubleParameterUpdate(const std::string& parameterName, double value) override;
		void HandleIntParameterUpdate(const std::string& parameterName, long long value) override;
		void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr) override;

		DepthMappingFrameData BuildDepthMappingFrame(INuiSensor* sensor, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame, std::vector<std::uint8_t>& tempMemory);

		static BodyIndexFrameData BuildBodyFrame(const DepthFrameData& depthFrame);
		static ColorFrameData RetrieveColorFrame(INuiSensor* sensor, HANDLE colorStream, std::int64_t* timestamp);
		static DepthFrameData RetrieveDepthFrame(INuiSensor* sensor, HANDLE depthStream, std::int64_t* timestamp);
		static InfraredFrameData RetrieveInfraredFrame(INuiSensor* sensor, HANDLE irStream, std::int64_t* timestamp);
		static void ExtractDepth(DepthFrameData& depthFrame);

		ReleasePtr<INuiColorCameraSettings> m_cameraSettings;
		ReleasePtr<INuiCoordinateMapper> m_coordinateMapper;
		ReleasePtr<INuiSensor> m_kinectSensor;
		HandlePtr m_elevationUpdateEvent;
		HandlePtr m_exitElevationThreadEvent;
		std::atomic<LONG> m_kinectElevation;
		std::thread m_elevationThread;
		bool m_hasRequestedPrivilege;
};

#endif
