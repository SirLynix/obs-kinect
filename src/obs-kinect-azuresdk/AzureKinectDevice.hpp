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

#ifndef OBS_KINECT_PLUGIN_AZUREKINECTDEVICE
#define OBS_KINECT_PLUGIN_AZUREKINECTDEVICE

#include "KinectDevice.hpp"
#include "Win32Helper.hpp"
#include <k4a/k4a.hpp>

enum class ColorResolution
{
	R1280x720  = 0,
	R1920x1080 = 1,
	R2560x1440 = 2,
	R2048x1536 = 3,
	R3840x2160 = 4,
	R4096x3072 = 5
};

enum class DepthMode
{
	Passive        = 0,
	NFOVUnbinned   = 1,
	NFOV2x2Binned  = 2,
	WFOVUnbinned   = 3,
	WFOV2x2Binned  = 4
};

class AzureKinectDevice final : public KinectDevice
{
	public:
		AzureKinectDevice(std::uint32_t deviceIndex);
		~AzureKinectDevice();

		obs_properties_t* AzureKinectDevice::CreateProperties() const;

	private:
		void HandleIntParameterUpdate(const std::string& parameterName, long long value);
		void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr) override;

		static BodyIndexFrameData ToBodyIndexFrame(const k4a::image& image);
		static ColorFrameData ToColorFrame(const k4a::image& image);
		static DepthFrameData ToDepthFrame(const k4a::image& image);
		static InfraredFrameData ToInfraredFrame(const k4a::image& image);

		k4a::device m_device;
		std::atomic<ColorResolution> m_colorResolution;
		std::atomic<DepthMode> m_depthMode;
};

#endif
