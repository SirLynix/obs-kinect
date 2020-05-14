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

		KinectDeviceAccess AcquireAccess(SourceFlags enabledSources);

		virtual obs_properties_t* CreateProperties() const;

		KinectFrameConstPtr GetLastFrame();

		SourceFlags GetSupportedSources() const;
		const std::string& GetUniqueName() const;

		void SetDefaultValues(obs_data_t* settings);

		void StartCapture();
		void StopCapture();

		KinectDevice& operator=(const KinectDevice&) = delete;
		KinectDevice& operator=(KinectDevice&&) = delete;

		static constexpr std::uint64_t InvalidFrameIndex = std::numeric_limits<std::uint64_t>::max();

	protected:
		std::optional<SourceFlags> GetSourceFlagsUpdate();

		bool IsRunning() const;
		void RegisterBoolParameter(std::string parameterName, bool defaultValue, std::function<bool(bool, bool)> combinator);
		void RegisterDoubleParameter(std::string parameterName, double defaultValue, double epsilon, std::function<double(double, double)> combinator);
		void RegisterIntParameter(std::string parameterName, long long defaultValue, std::function<long long(long long, long long)> combinator);
		void SetSupportedSources(SourceFlags enabledSources);
		void SetUniqueName(std::string uniqueName);
		void TriggerSourceFlagsUpdate();
		void UpdateFrame(KinectFramePtr kinectFrame);

		virtual void HandleBoolParameterUpdate(const std::string& parameterName, bool value);
		virtual void HandleDoubleParameterUpdate(const std::string& parameterName, double value);
		virtual void HandleIntParameterUpdate(const std::string& parameterName, long long value);
		virtual void ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr) = 0;

	private:
		using ParameterValue = std::variant<bool, double, long long>;

		struct AccessData
		{
			SourceFlags enabledSources;
			std::unordered_map<std::string, ParameterValue> parameters;
		};

		template<typename T>
		struct DataParameter
		{
			T defaultValue;
			T value;
			std::function<T (T, T)> combinator;
		};

		struct DoubleParameter : DataParameter<double>
		{
			double epsilon;
		};

		using BoolParameter = DataParameter<bool>;
		using IntegerParameter = DataParameter<long long>;

		using ParameterData = std::variant<BoolParameter, DoubleParameter, IntegerParameter>;

		void RefreshParameters();
		void ReleaseAccess(AccessData* access);
		void UpdateDeviceParameters(AccessData* access, obs_data_t* settings);
		void UpdateEnabledSources();
		void UpdateParameter(const std::string& parameterName);

		void SetEnabledSources(SourceFlags sourceFlags);

		SourceFlags m_deviceSources;
		SourceFlags m_supportedSources;
		KinectFramePtr m_lastFrame;
		std::atomic_bool m_running;
		std::mutex m_deviceSourceLock;
		std::mutex m_lastFrameLock;
		std::string m_uniqueName;
		std::thread m_thread;
		std::unordered_map<std::string, ParameterData> m_parameters;
		std::vector<std::unique_ptr<AccessData>> m_accesses;
		std::uint64_t m_frameIndex;
		bool m_deviceSourceUpdated;
};

#endif
