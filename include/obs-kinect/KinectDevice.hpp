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
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

class KinectDeviceAccess;

class OBSKINECT_API KinectDevice
{
	friend KinectDeviceAccess;

	public:
		KinectDevice();
		KinectDevice(const KinectDevice&) = delete;
		KinectDevice(KinectDevice&&) = delete;
		virtual ~KinectDevice();

		KinectDeviceAccess AcquireAccess(EnabledSourceFlags enabledSources);

		virtual obs_properties_t* CreateProperties() const;

		KinectFrameConstPtr GetLastFrame();

		const std::string& GetUniqueName() const;

		void SetDefaultValues(obs_data_t* settings);

		void StartCapture();
		void StopCapture();

		KinectDevice& operator=(const KinectDevice&) = delete;
		KinectDevice& operator=(KinectDevice&&) = delete;

	protected:
		std::optional<EnabledSourceFlags> GetSourceFlagsUpdate();

		bool IsRunning() const;
		void RegisterBoolParameter(std::string parameterName, bool defaultValue, std::function<bool(bool, bool)> combinator);
		void RegisterIntParameter(std::string parameterName, long long defaultValue, std::function<long long(long long, long long)> combinator);
		void SetUniqueName(std::string uniqueName);
		void UpdateFrame(KinectFramePtr kinectFrame);

		virtual void HandleBoolParameterUpdate(const std::string& parameterName, bool value);
		virtual void HandleIntParameterUpdate(const std::string& parameterName, long long value);
		virtual void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr) = 0;

	private:
		using ParameterValue = std::variant<bool, long long>;

		struct AccessData
		{
			EnabledSourceFlags enabledSources;
			std::unordered_map<std::string, ParameterValue> parameters;
		};

		struct BoolParameter
		{
			bool defaultValue;
			bool value;
			std::function<bool(bool, bool)> combinator;
		};

		struct IntegerParameter
		{
			long long defaultValue;
			long long value;
			std::function<long long(long long, long long)> combinator;
		};

		using ParameterData = std::variant<BoolParameter, IntegerParameter>;

		void ReleaseAccess(AccessData* access);
		void UpdateDeviceParameters(AccessData* access, obs_data_t* settings);
		void UpdateEnabledSources();
		void UpdateParameter(const std::string& parameterName);

		void SetEnabledSources(EnabledSourceFlags sourceFlags);

		EnabledSourceFlags m_deviceSources;
		KinectFramePtr m_lastFrame;
		std::atomic_bool m_running;
		std::mutex m_deviceSourceLock;
		std::mutex m_lastFrameLock;
		std::string m_uniqueName;
		std::thread m_thread;
		std::unordered_map<std::string, ParameterData> m_parameters;
		std::vector<std::unique_ptr<AccessData>> m_accesses;
		bool m_deviceSourceUpdated;
};

#endif
