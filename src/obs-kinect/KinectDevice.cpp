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

#include "KinectDevice.hpp"
#include "KinectDeviceAccess.hpp"
#include <algorithm>
#include <type_traits>

template<typename T>
struct AlwaysFalse : std::false_type {};

KinectDevice::KinectDevice() :
m_deviceSourceUpdated(true),
m_running(false)
{
}

KinectDevice::~KinectDevice()
{
	assert(m_accesses.empty());
	StopCapture(); //< Just in case
}

KinectDeviceAccess KinectDevice::AcquireAccess(EnabledSourceFlags enabledSources)
{
	if (m_accesses.empty())
		StartCapture();

	auto& accessDataPtr = m_accesses.emplace_back(std::make_unique<AccessData>());
	accessDataPtr->enabledSources = enabledSources;

	for (auto&& [parameterName, parameterData] : m_parameters)
	{
		const std::string& name = parameterName; //< structured binding cannot be captured directly

		std::visit([&](auto&& arg)
		{
			accessDataPtr->parameters[name] = arg.defaultValue;
		}, parameterData);
	}

	UpdateEnabledSources();

	return KinectDeviceAccess(*this, accessDataPtr.get());
}

obs_properties_t* KinectDevice::CreateProperties() const
{
	return nullptr;
}

auto KinectDevice::GetLastFrame() -> KinectFrameConstPtr
{
	std::lock_guard<std::mutex> lock(m_lastFrameLock);
	return m_lastFrame;
}

void KinectDevice::ReleaseAccess(AccessData* accessData)
{
	auto it = std::find_if(m_accesses.begin(), m_accesses.end(), [=](const std::unique_ptr<AccessData>& data) { return data.get() == accessData; });
	assert(it != m_accesses.end());
	m_accesses.erase(it);

	UpdateEnabledSources();

	if (m_accesses.empty())
		StopCapture();
}

void KinectDevice::UpdateDeviceParameters(AccessData* access, obs_data_t* settings)
{
	for (auto&& [parameterName, parameterData] : m_parameters)
	{
		const std::string& name = parameterName; //< structured binding cannot be captured directly

		std::visit([&](auto&& arg)
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, BoolParameter>)
			{
				access->parameters[name] = obs_data_get_bool(settings, name.c_str());
			}
			else if constexpr (std::is_same_v<T, IntegerParameter>)
			{
				access->parameters[name] = obs_data_get_int(settings, name.c_str());
			}
			else
				static_assert(AlwaysFalse<T>(), "non-exhaustive visitor");
		}, parameterData);

		UpdateParameter(name);
	}
}

void KinectDevice::UpdateEnabledSources()
{
	EnabledSourceFlags sourceFlags = 0;
	for (auto& access : m_accesses)
		sourceFlags |= access->enabledSources;

	SetEnabledSources(sourceFlags);
}

void KinectDevice::UpdateParameter(const std::string& parameterName)
{
	auto it = m_parameters.find(parameterName);
	assert(it != m_parameters.end());

	std::visit([&](auto&& arg)
	{
		using T = std::decay_t<decltype(arg)>;

		auto value = arg.defaultValue;
		for (auto& access : m_accesses)
		{
			auto valIt = access->parameters.find(parameterName);
			assert(valIt != access->parameters.end());

			value = arg.combinator(value, std::get<decltype(value)>(valIt->second));
		}

		if (value != arg.value)
		{
			if constexpr (std::is_same_v<T, BoolParameter>)
			{
				HandleBoolParameterUpdate(parameterName, value);
			}
			else if constexpr (std::is_same_v<T, IntegerParameter>)
			{
				HandleIntParameterUpdate(parameterName, value);
			}
			else
				static_assert(AlwaysFalse<T>(), "non-exhaustive visitor");

			arg.value = value;
		}
	}, it->second);
}

void KinectDevice::SetEnabledSources(EnabledSourceFlags sourceFlags)
{
	if (m_deviceSources == sourceFlags)
		return;

	std::lock_guard<std::mutex> lock(m_deviceSourceLock);
	m_deviceSources = sourceFlags;
	m_deviceSourceUpdated = false;
}

const std::string& KinectDevice::GetUniqueName() const
{
	return m_uniqueName;
}

void KinectDevice::SetDefaultValues(obs_data_t* settings)
{
	for (auto&& [parameterName, parameterData] : m_parameters)
	{
		const std::string& name = parameterName; //< structured binding cannot be captured directly

		std::visit([&](auto&& arg)
		{
			using T = std::decay_t<decltype(arg)>;

			if constexpr (std::is_same_v<T, BoolParameter>)
			{
				obs_data_set_default_bool(settings, name.c_str(), arg.defaultValue);
			}
			else if constexpr (std::is_same_v<T, IntegerParameter>)
			{
				obs_data_set_default_int(settings, name.c_str(), arg.defaultValue);
			}
			else
				static_assert(AlwaysFalse<T>(), "non-exhaustive visitor");
		}, parameterData);
	}
}

void KinectDevice::StartCapture()
{
	if (m_running)
		return;

	std::mutex mutex;
	std::condition_variable cv;

	m_running = true;

	std::exception_ptr exceptionPtr;

	std::unique_lock<std::mutex> lock(mutex);
	m_thread = std::thread([&] { ThreadFunc(cv, mutex, exceptionPtr); });

	// Wait until thread has been activated
	cv.wait(lock);

	if (exceptionPtr)
		std::rethrow_exception(exceptionPtr);
}

void KinectDevice::StopCapture()
{
	if (!m_running)
		return;

	m_running = false;
	m_thread.join();
	m_lastFrame.reset();
}

std::optional<EnabledSourceFlags> KinectDevice::GetSourceFlagsUpdate()
{
	std::unique_lock<std::mutex> lock(m_deviceSourceLock);
	if (!m_deviceSourceUpdated)
	{
		m_deviceSourceUpdated = true;
		return m_deviceSources;
	}

	return {};
}

bool KinectDevice::IsRunning() const
{
	return m_running;
}

void KinectDevice::RegisterBoolParameter(std::string parameterName, bool defaultValue, std::function<bool(bool, bool)> combinator)
{
	BoolParameter parameter;
	parameter.combinator = std::move(combinator);
	parameter.defaultValue = defaultValue;
	parameter.value = defaultValue;

	assert(m_parameters.find(parameterName) == m_parameters.end());
	m_parameters.emplace(std::move(parameterName), std::move(parameter));
}

void KinectDevice::RegisterIntParameter(std::string parameterName, long long defaultValue, std::function<long long(long long, long long)> combinator)
{
	IntegerParameter parameter;
	parameter.combinator = std::move(combinator);
	parameter.defaultValue = defaultValue;
	parameter.value = defaultValue;

	assert(m_parameters.find(parameterName) == m_parameters.end());
	m_parameters.emplace(std::move(parameterName), std::move(parameter));
}

void KinectDevice::SetUniqueName(std::string uniqueName)
{
	assert(m_uniqueName.empty());
	m_uniqueName = std::move(uniqueName);
}

void KinectDevice::UpdateFrame(KinectFramePtr kinectFrame)
{
	std::lock_guard<std::mutex> lock(m_lastFrameLock);
	m_lastFrame = std::move(kinectFrame);
}

void KinectDevice::HandleBoolParameterUpdate(const std::string& parameterName, bool value)
{
}

void KinectDevice::HandleIntParameterUpdate(const std::string& parameterName, long long value)
{
}
