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

#ifndef OBS_KINECT_PLUGIN_KINECTDEVICE
#define OBS_KINECT_PLUGIN_KINECTDEVICE

#include "Helper.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Kinect.h>
#include <windows.h>

class KinectDevice
{
	public:
		struct DepthCoordinates;
		struct KinectFrame;
		using KinectFramePtr = std::shared_ptr<KinectFrame>;

		KinectDevice();
		~KinectDevice();

		KinectFramePtr GetLastFrame();

		bool MapColorToDepth(const std::uint16_t* depthValues, std::size_t valueCount, std::size_t colorPixelCount, DepthCoordinates* depthCoordinatesOut);

		void StartCapture();
		void StopCapture();

		struct DepthCoordinates
		{
			float x;
			float y;
		};

		struct FrameData
		{
			std::uint32_t width;
			std::uint32_t height;
			std::uint32_t pitch;
			ObserverPtr<std::uint8_t[]> ptr;
			std::vector<std::uint8_t> memory; //< TODO: Reuse memory
		};

		struct BodyIndexFrameData : FrameData
		{
		};

		struct ColorFrameData : FrameData
		{
			gs_color_format format;
		};

		struct DepthFrameData : FrameData
		{
		};

		struct InfraredFrameData : FrameData
		{
		};

		struct KinectFrame
		{
			std::optional<BodyIndexFrameData> bodyIndexFrame;
			std::optional<ColorFrameData> colorFrame;
			std::optional<DepthFrameData> depthFrame;
			std::optional<InfraredFrameData> infraredFrame;
		};

	private:
		BodyIndexFrameData RetrieveBodyIndexFrame(IMultiSourceFrame* multiSourceFrame);
		ColorFrameData RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame);
		DepthFrameData RetrieveDepthFrame(IMultiSourceFrame* multiSourceFrame);
		InfraredFrameData RetrieveInfraredFrame(IMultiSourceFrame* multiSourceFrame);

		void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr);

		ReleasePtr<IKinectSensor> m_kinectSensor;
		ReleasePtr<ICoordinateMapper> m_coordinateMapper;
		KinectFramePtr m_lastFrame;
		std::mutex m_lastFrameLock;
		std::atomic_bool m_running;
		std::thread m_thread;
};

#endif
