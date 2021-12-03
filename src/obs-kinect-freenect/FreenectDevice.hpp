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

#pragma once

#ifndef OBS_KINECT_PLUGIN_KINECTFREENECTDEVICE
#define OBS_KINECT_PLUGIN_KINECTFREENECTDEVICE

#include "FreenectHelper.hpp"
#include <obs-kinect-core/KinectDevice.hpp>
#include <libfreenect/libfreenect.h>

class KinectFreenectDevice final : public KinectDevice
{
	public:
		KinectFreenectDevice(freenect_device* device, const char* serial);
		~KinectFreenectDevice();

	private:
		void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr) override;

		/*static ColorFrameData RetrieveColorFrame(const libfreenect2::Frame* frame);
		static DepthFrameData RetrieveDepthFrame(const libfreenect2::Frame* frame);
		static InfraredFrameData RetrieveInfraredFrame(const libfreenect2::Frame* frame);*/

		freenect_context* m_context;
		freenect_device* m_device;
};

#endif
