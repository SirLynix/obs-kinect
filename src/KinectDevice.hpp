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

#include "Enums.hpp"
#include "Helper.hpp"
#include "KinectFrame.hpp"
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

class KinectDeviceAccess;

class KinectDevice
{
	friend KinectDeviceAccess;

	public:
		struct DepthCoordinates;

		KinectDevice();
		~KinectDevice();

		KinectDeviceAccess AcquireAccess(EnabledSourceFlags enabledSources);

		KinectFrameConstPtr GetLastFrame();

		bool MapColorToDepth(const std::uint16_t* depthValues, std::size_t valueCount, std::size_t colorPixelCount, DepthCoordinates* depthCoordinatesOut);

		void StartCapture();
		void StopCapture();

		struct DepthCoordinates
		{
			float x;
			float y;
		};

	private:
		struct AccessData
		{
			EnabledSourceFlags enabledSources;
			ProcessPriority servicePriority = ProcessPriority::Normal;
		};

		void ReleaseAccess(AccessData* access);
		void UpdateEnabledSources();
		void UpdateServicePriority();

		BodyIndexFrameData RetrieveBodyIndexFrame(IMultiSourceFrame* multiSourceFrame);
		ColorFrameData RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame);
		DepthFrameData RetrieveDepthFrame(IMultiSourceFrame* multiSourceFrame);
		DepthMappingFrameData RetrieveDepthMappingFrame(const ColorFrameData& colorFrame, const DepthFrameData& depthFrame);

		InfraredFrameData RetrieveInfraredFrame(IMultiSourceFrame* multiSourceFrame);

		void SetEnabledSources(EnabledSourceFlags sourceFlags);
		bool SetServicePriority(ProcessPriority priority);

		void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr);

		EnabledSourceFlags m_deviceSources;
		ReleasePtr<IKinectSensor> m_kinectSensor;
		ReleasePtr<ICoordinateMapper> m_coordinateMapper;
		KinectFramePtr m_lastFrame;
		ProcessPriority m_servicePriority;
		std::atomic_bool m_running;
		std::mutex m_deviceSourceLock;
		std::mutex m_lastFrameLock;
		std::thread m_thread;
		std::vector<std::unique_ptr<AccessData>> m_accesses;
		bool m_deviceSourceUpdated;
		bool m_hasRequestedPrivilege;
};

#endif
