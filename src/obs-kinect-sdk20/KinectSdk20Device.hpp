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

#ifndef OBS_KINECT_PLUGIN_KINECTDEVICESDK20
#define OBS_KINECT_PLUGIN_KINECTDEVICESDK20

#include "KinectDevice.hpp"
#include "Win32Helper.hpp"

#include <Kinect.h>

class KinectSdk20Device final : public KinectDevice
{
	public:
		KinectSdk20Device();
		~KinectSdk20Device();

		obs_properties_t* CreateProperties() const override;

		bool MapColorToDepth(const std::uint16_t* depthValues, std::size_t valueCount, std::size_t colorPixelCount, DepthMappingFrameData::DepthCoordinates* depthCoordinatesOut) const;

		static void SetServicePriority(ProcessPriority priority);

	private:
		void HandleIntParameterUpdate(const std::string& parameterName, long long value) override;
		void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr) override;

		static BodyIndexFrameData RetrieveBodyIndexFrame(IMultiSourceFrame* multiSourceFrame);
		static ColorFrameData RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame);
		static DepthFrameData RetrieveDepthFrame(IMultiSourceFrame* multiSourceFrame);
		static DepthMappingFrameData RetrieveDepthMappingFrame(const KinectSdk20Device& device, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame);
		static InfraredFrameData RetrieveInfraredFrame(IMultiSourceFrame* multiSourceFrame);

		ReleasePtr<IKinectSensor> m_kinectSensor;
		ReleasePtr<ICoordinateMapper> m_coordinateMapper;

		static ProcessPriority s_servicePriority;
};

#endif
