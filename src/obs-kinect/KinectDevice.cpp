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

KinectDevice::KinectDevice() :
m_servicePriority(ProcessPriority::Normal),
m_deviceSourceUpdated(true),
m_running(false)
{
}

KinectDevice::~KinectDevice()
{
	m_accesses.clear();
}

KinectDeviceAccess KinectDevice::AcquireAccess(EnabledSourceFlags enabledSources)
{
	if (m_accesses.empty())
		StartCapture();

	auto& accessDataPtr = m_accesses.emplace_back(std::make_unique<AccessData>());
	accessDataPtr->enabledSources = enabledSources;

	UpdateEnabledSources();

	return KinectDeviceAccess(*this, accessDataPtr.get());
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
	UpdateServicePriority();

	if (m_accesses.empty())
		StopCapture();
}

void KinectDevice::UpdateEnabledSources()
{
	EnabledSourceFlags sourceFlags = 0;
	for (auto& access : m_accesses)
		sourceFlags |= access->enabledSources;

	SetEnabledSources(sourceFlags);
}

void KinectDevice::UpdateServicePriority()
{
	ProcessPriority highestPriority = ProcessPriority::Normal;
	for (auto& access : m_accesses)
	{
		if (access->servicePriority > highestPriority)
			highestPriority = access->servicePriority;
	}

	if (m_servicePriority != highestPriority)
	{
		SetServicePriority(highestPriority);
		m_servicePriority = highestPriority;
	}
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
