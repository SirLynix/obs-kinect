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

class KinectDeviceAccess;

class OBSKINECT_API KinectDevice
{
	friend KinectDeviceAccess;

	public:
		struct DepthCoordinates;

		KinectDevice();
		KinectDevice(const KinectDevice&) = delete;
		KinectDevice(KinectDevice&&) = delete;
		virtual ~KinectDevice();

		KinectDeviceAccess AcquireAccess(EnabledSourceFlags enabledSources);

		KinectFrameConstPtr GetLastFrame();

		const std::string& GetUniqueName() const;

		void StartCapture();
		void StopCapture();

		struct DepthCoordinates
		{
			float x;
			float y;
		};

		KinectDevice& operator=(const KinectDevice&) = delete;
		KinectDevice& operator=(KinectDevice&&) = delete;

	protected:
		std::optional<EnabledSourceFlags> GetSourceFlagsUpdate();

		bool IsRunning() const;
		void SetUniqueName(std::string uniqueName);
		void UpdateFrame(KinectFramePtr kinectFrame);

		virtual void SetServicePriority(ProcessPriority priority) = 0;
		virtual void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr) = 0;

	private:
		struct AccessData
		{
			EnabledSourceFlags enabledSources;
			ProcessPriority servicePriority = ProcessPriority::Normal;
		};

		void ReleaseAccess(AccessData* access);
		void UpdateEnabledSources();
		void UpdateServicePriority();

		void SetEnabledSources(EnabledSourceFlags sourceFlags);

		EnabledSourceFlags m_deviceSources;
		KinectFramePtr m_lastFrame;
		ProcessPriority m_servicePriority;
		std::atomic_bool m_running;
		std::mutex m_deviceSourceLock;
		std::mutex m_lastFrameLock;
		std::string m_uniqueName;
		std::thread m_thread;
		std::vector<std::unique_ptr<AccessData>> m_accesses;
		bool m_deviceSourceUpdated;
};

#endif
