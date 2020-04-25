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
#include <util/platform.h>
#include <algorithm>
#include <array>
#include <numeric>
#include <optional>
#include <tlhelp32.h>

#define blog(log_level, format, ...)                    \
	blog(log_level, "[obs-kinect] " format, ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

KinectDevice::KinectDevice() :
m_servicePriority(ProcessPriority::Normal),
m_running(false),
m_hasRequestedPrivilege(false),
m_captureCounter(0)
{
	IKinectSensor* pKinectSensor;
	if (FAILED(GetDefaultKinectSensor(&pKinectSensor)))
		throw std::runtime_error("failed to get Kinect sensor");

	m_kinectSensor.reset(pKinectSensor);

	ICoordinateMapper* pCoordinateMapper;
	if (FAILED(m_kinectSensor->get_CoordinateMapper(&pCoordinateMapper)))
		throw std::runtime_error("failed to retrieve coordinate mapper");

	m_coordinateMapper.reset(pCoordinateMapper);
}

KinectDevice::~KinectDevice()
{
	StopCapture(true);
	SetServicePriority(ProcessPriority::Normal);
}

auto KinectDevice::GetLastFrame() -> KinectFramePtr
{
	std::lock_guard<std::mutex> lock(m_lastFrameLock);
	return m_lastFrame;
}

bool KinectDevice::MapColorToDepth(const std::uint16_t* depthValues, std::size_t valueCount, std::size_t colorPixelCount, DepthCoordinates* depthCoordinatesOut)
{
	static_assert(sizeof(UINT16) == sizeof(std::uint16_t));
	static_assert(sizeof(DepthCoordinates) == sizeof(DepthSpacePoint));

	const UINT16* depthPtr = reinterpret_cast<const UINT16*>(depthValues);
	DepthSpacePoint* coordinatePtr = reinterpret_cast<DepthSpacePoint*>(depthCoordinatesOut);

	HRESULT r = m_coordinateMapper->MapColorFrameToDepthSpace(UINT(valueCount), depthPtr, UINT(colorPixelCount), coordinatePtr);
	if (FAILED(r))
		return false;

	return true;
}

bool KinectDevice::SetServicePriority(ProcessPriority priority)
{
	if (m_servicePriority == priority)
		return true;

	DWORD priorityClass;
	switch (priority)
	{
		case ProcessPriority::High:        priorityClass = HIGH_PRIORITY_CLASS; break;
		case ProcessPriority::AboveNormal: priorityClass = ABOVE_NORMAL_PRIORITY_CLASS; break;
		case ProcessPriority::Normal:      priorityClass = NORMAL_PRIORITY_CLASS; break;

		default:
			warn("unknown process priority %d", int(priority));
			return false;
	}

	if (!m_hasRequestedPrivilege)
	{
		TOKEN_PRIVILEGES tkp;

		LUID luid;
		if (!LookupPrivilegeValue(nullptr, SE_INC_BASE_PRIORITY_NAME, &luid))
		{
			warn("failed to get privilege SE_INC_BASE_PRIORITY_NAME");
			return false;
		}

		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Luid = luid;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		HANDLE token;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		{
			warn("failed to open processor token");
			return false;
		}
		HandlePtr tokenOwner(token);

		if (!AdjustTokenPrivileges(token, FALSE, &tkp, sizeof(tkp), nullptr, nullptr))
		{
			warn("failed to adjust token privileges");
			return false;
		}

		info("adjusted token privileges");
		m_hasRequestedPrivilege = true;
	}

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
	{
		warn("failed to retrieve processes snapshot");
		return false;
	}
	HandlePtr snapshotOwner(snapshot);

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot, &entry))
	{
		do
		{
#ifdef UNICODE
			if (wcscmp(entry.szExeFile, L"KinectService.exe") == 0)
#else
			if (strcmp(entry.szExeFile, "KinectService.exe") == 0)
#endif
			{
				info("found KinectService.exe, trying to update its priority...");

				HANDLE process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, entry.th32ProcessID);
				if (!process)
				{
					warn("failed to open process");
					return false;
				}
				HandlePtr processOwner(process);

				if (!SetPriorityClass(process, priorityClass))
				{
					warn("failed to update process priority");
					return false;
				}

				info("KinectService.exe priority updated successfully");
				m_servicePriority = priority;
				return true;
			}
		}
		while (Process32Next(snapshot, &entry));
	}

	warn("KinectService.exe not found");
	return false;
}

auto KinectDevice::RetrieveBodyIndexFrame(IMultiSourceFrame* multiSourceFrame) -> BodyIndexFrameData
{
	IBodyIndexFrameReference* pBodyIndexFrameReference;
	if (FAILED(multiSourceFrame->get_BodyIndexFrameReference(&pBodyIndexFrameReference)))
		throw std::runtime_error("Failed to get body index frame reference");

	ReleasePtr<IBodyIndexFrameReference> bodyIndexFrameReference(pBodyIndexFrameReference);

	IBodyIndexFrame* pBodyIndexFrame;
	if (FAILED(pBodyIndexFrameReference->AcquireFrame(&pBodyIndexFrame)))
		throw std::runtime_error("Failed to acquire body index frame");

	ReleasePtr<IBodyIndexFrame> bodyIndexFrame(pBodyIndexFrame);

	IFrameDescription* pBodyIndexFrameDescription;
	if (FAILED(bodyIndexFrame->get_FrameDescription(&pBodyIndexFrameDescription)))
		throw std::runtime_error("Failed to get body index frame description");

	ReleasePtr<IFrameDescription> bodyIndexFrameDescription(pBodyIndexFrameDescription);

	int width;
	int height;
	unsigned int bytePerPixel;

	// srsly microsoft
	if (FAILED(bodyIndexFrameDescription->get_Width(&width)) ||
	    FAILED(bodyIndexFrameDescription->get_Height(&height)) ||
	    FAILED(bodyIndexFrameDescription->get_BytesPerPixel(&bytePerPixel)))
	{
		throw std::runtime_error("Failed to retrieve bodyIndex frame description values");
	}

	if (bytePerPixel != sizeof(BYTE))
		throw std::runtime_error("Unexpected BPP");

	BodyIndexFrameData frameData;
	frameData.memory.resize(width * height * sizeof(BYTE));
	BYTE* memPtr = reinterpret_cast<BYTE*>(frameData.memory.data());

	if (FAILED(bodyIndexFrame->CopyFrameDataToArray(UINT(width * height), memPtr)))
		throw std::runtime_error("Failed to access body index frame buffer");

	frameData.width = width;
	frameData.height = height;
	frameData.pitch = width * bytePerPixel;
	frameData.ptr.reset(frameData.memory.data());

	return frameData;
}

auto KinectDevice::RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame) -> ColorFrameData
{
	ColorFrameData frameData;

	IColorFrameReference* pColorFrameReference;
	if (FAILED(multiSourceFrame->get_ColorFrameReference(&pColorFrameReference)))
		throw std::runtime_error("Failed to get color frame reference");

	ReleasePtr<IColorFrameReference> colorFrameReference(pColorFrameReference);

	IColorFrame* pColorFrame;
	if (FAILED(pColorFrameReference->AcquireFrame(&pColorFrame)))
		throw std::runtime_error("Failed to acquire color frame");

	ReleasePtr<IColorFrame> colorFrame(pColorFrame);

	IFrameDescription* pColorFrameDescription;
	if (FAILED(pColorFrame->get_FrameDescription(&pColorFrameDescription)))
		throw std::runtime_error("Failed to get color frame description");

	ReleasePtr<IFrameDescription> colorFrameDescription(pColorFrameDescription);

	int width;
	int height;
	unsigned int bytePerPixel;
	ColorImageFormat imageFormat;
		
	// srsly microsoft
	if (FAILED(colorFrameDescription->get_Width(&width)) ||
	    FAILED(colorFrameDescription->get_Height(&height)) ||
	    FAILED(colorFrameDescription->get_BytesPerPixel(&bytePerPixel)) ||
	    FAILED(colorFrame->get_RawColorImageFormat(&imageFormat)))
	{
		throw std::runtime_error("Failed to retrieve color frame description values");
	}

	frameData.width = width;
	frameData.height = height;

	// Convert to RGBA
	std::size_t memSize = width * height * 4;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	if (FAILED(colorFrame->CopyConvertedFrameDataToArray(UINT(memSize), reinterpret_cast<BYTE*>(memPtr), ColorImageFormat_Rgba)))
		throw std::runtime_error("Failed to copy color buffer");

	frameData.ptr.reset(memPtr);
	frameData.pitch = width * 4;
	frameData.format = GS_RGBA;

	return frameData;
}

auto KinectDevice::RetrieveDepthFrame(IMultiSourceFrame* multiSourceFrame) -> DepthFrameData
{
	IDepthFrameReference* pDepthFrameReference;
	if (FAILED(multiSourceFrame->get_DepthFrameReference(&pDepthFrameReference)))
		throw std::runtime_error("Failed to get depth frame reference");

	ReleasePtr<IDepthFrameReference> depthFrameReference(pDepthFrameReference);

	IDepthFrame* pDepthFrame;
	if (FAILED(pDepthFrameReference->AcquireFrame(&pDepthFrame)))
		throw std::runtime_error("Failed to acquire depth frame");

	ReleasePtr<IDepthFrame> depthFrame(pDepthFrame);

	IFrameDescription* pDepthFrameDescription;
	if (FAILED(depthFrame->get_FrameDescription(&pDepthFrameDescription)))
		throw std::runtime_error("Failed to get depth frame description");

	ReleasePtr<IFrameDescription> depthFrameDescription(pDepthFrameDescription);

	int width;
	int height;
	unsigned int bytePerPixel;

	// srsly microsoft
	if (FAILED(depthFrameDescription->get_Width(&width)) ||
	    FAILED(depthFrameDescription->get_Height(&height)) ||
	    FAILED(depthFrameDescription->get_BytesPerPixel(&bytePerPixel)))
	{
		throw std::runtime_error("Failed to retrieve depth frame description values");
	}

	if (bytePerPixel != sizeof(UINT16))
		throw std::runtime_error("Unexpected BPP");

	DepthFrameData frameData;
	frameData.memory.resize(width * height * sizeof(UINT16));
	UINT16* memPtr = reinterpret_cast<UINT16*>(frameData.memory.data());

	if (FAILED(depthFrame->CopyFrameDataToArray(UINT(width * height), memPtr)))
		throw std::runtime_error("Failed to access depth frame buffer");

	frameData.width = width;
	frameData.height = height;
	frameData.pitch = width * bytePerPixel;
	frameData.ptr.reset(frameData.memory.data());

	return frameData;
}

auto KinectDevice::RetrieveInfraredFrame(IMultiSourceFrame* multiSourceFrame) -> InfraredFrameData
{
	IInfraredFrameReference* pInfraredFrameReference;
	if (FAILED(multiSourceFrame->get_InfraredFrameReference(&pInfraredFrameReference)))
		throw std::runtime_error("Failed to get infrared frame reference");

	ReleasePtr<IInfraredFrameReference> infraredFrameReference(pInfraredFrameReference);

	IInfraredFrame* pInfraredFrame;
	if (FAILED(pInfraredFrameReference->AcquireFrame(&pInfraredFrame)))
		throw std::runtime_error("Failed to acquire infrared frame");

	ReleasePtr<IInfraredFrame> infraredFrame(pInfraredFrame);

	IFrameDescription* pInfraredFrameDescription;
	if (FAILED(infraredFrame->get_FrameDescription(&pInfraredFrameDescription)))
		throw std::runtime_error("Failed to get infrared frame description");

	ReleasePtr<IFrameDescription> infraredFrameDescription(pInfraredFrameDescription);

	int width;
	int height;
	unsigned int bytePerPixel;

	// srsly microsoft
	if (FAILED(infraredFrameDescription->get_Width(&width)) ||
	    FAILED(infraredFrameDescription->get_Height(&height)) ||
	    FAILED(infraredFrameDescription->get_BytesPerPixel(&bytePerPixel)))
	{
		throw std::runtime_error("Failed to retrieve infrared frame description values");
	}

	if (bytePerPixel != sizeof(UINT16))
		throw std::runtime_error("Unexpected BPP");

	InfraredFrameData frameData;
	frameData.memory.resize(width * height * sizeof(UINT16));
	UINT16* memPtr = reinterpret_cast<UINT16*>(frameData.memory.data());

	if (FAILED(infraredFrame->CopyFrameDataToArray(UINT(width * height), memPtr)))
		throw std::runtime_error("Failed to access depth frame buffer");

	frameData.width = width;
	frameData.height = height;
	frameData.pitch = width * bytePerPixel;
	frameData.ptr.reset(frameData.memory.data());

	return frameData;
}

void KinectDevice::StartCapture()
{
	m_captureCounter++;
	if (m_running)
		return;

	std::mutex mutex;
	std::condition_variable cv;

	m_running = true;

	std::exception_ptr exceptionPtr;

	std::unique_lock<std::mutex> lock(mutex);
	m_thread = std::thread(&KinectDevice::ThreadFunc, this, std::ref(cv), std::ref(mutex), std::ref(exceptionPtr));

	// Wait until thread has been activated
	cv.wait(lock);

	if (exceptionPtr)
		std::rethrow_exception(exceptionPtr);
}

void KinectDevice::StopCapture(bool force)
{
	if (!m_running)
		return;

	assert(m_captureCounter > 0);
	if (--m_captureCounter > 0 && !force)
		return;

	m_captureCounter = 0;
	m_running = false;
	m_thread.join();
	m_lastFrame.reset();
}

void KinectDevice::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& error)
{
	ReleasePtr<IMultiSourceFrameReader> multiSourceFrameReader;
	ClosePtr<IKinectSensor> openedKinectSensor;

	try
	{
		if (FAILED(m_kinectSensor->Open()))
			throw std::runtime_error("failed to open Kinect sensor");

		openedKinectSensor.reset(m_kinectSensor.get());

		std::array<wchar_t, 256> wideId = { L"<failed to get id>" };
		openedKinectSensor->get_UniqueKinectId(UINT(wideId.size()), wideId.data());

		std::array<char, wideId.size()> id = { "<failed to get id>" };
		WideCharToMultiByte(CP_UTF8, 0, wideId.data(), int(wideId.size()), id.data(), int(id.size()), nullptr, nullptr);

		blog(LOG_INFO, "found kinect sensor (%s)", id.data());

		DWORD enabledSources = 0;
		// Is the cost of enabling color/depth/infrared sources that high? Should we add an option to only enable thoses in use?
		/*switch (m_sourceType)
		{
			case SourceType::Color: enabledSources |= FrameSourceTypes_Color; break;
			case SourceType::Depth: enabledSources |= FrameSourceTypes_Depth; break;
			case SourceType::Infrared: enabledSources |= FrameSourceTypes_Infrared; break;
			default: break;
		}*/

		enabledSources |= FrameSourceTypes_Color | FrameSourceTypes_Depth | FrameSourceTypes_Infrared | FrameSourceTypes_BodyIndex;

		IMultiSourceFrameReader* pMultiSourceFrameReader;
		if (FAILED(openedKinectSensor->OpenMultiSourceFrameReader(enabledSources, &pMultiSourceFrameReader)))
			throw std::runtime_error("failed to acquire source frame reader");

		multiSourceFrameReader.reset(pMultiSourceFrameReader);
	}
	catch (const std::exception&)
	{
		error = std::current_exception();
	}

	{
		std::unique_lock<std::mutex> lk(m);
		cv.notify_all();
	} // m & cv no longer exists from here

	if (error)
		return;

	uint64_t now = os_gettime_ns();
	uint64_t delay = 1'000'000'000ULL / 30; // Target 30 FPS (Kinect doesn't run higher afaik)

	while (m_running)
	{
		IMultiSourceFrame* pMultiSourceFrame;
		HRESULT acquireResult = multiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame);
			
		if (FAILED(acquireResult))
		{
			if (acquireResult == E_PENDING)
			{
				os_sleep_ms(10);
				continue;
			}

			warn("failed to acquire latest frame: %d", HRESULT_CODE(acquireResult));
			continue;
		}

		ReleasePtr<IMultiSourceFrame> multiSourceFrame(pMultiSourceFrame);

		try
		{
			KinectFramePtr framePtr = std::make_shared<KinectFrame>();
			framePtr->bodyIndexFrame = RetrieveBodyIndexFrame(multiSourceFrame.get());
			framePtr->colorFrame = RetrieveColorFrame(multiSourceFrame.get());
			framePtr->depthFrame = RetrieveDepthFrame(multiSourceFrame.get());
			framePtr->infraredFrame = RetrieveInfraredFrame(multiSourceFrame.get());
			{
				std::lock_guard<std::mutex> lock(m_lastFrameLock);
				m_lastFrame = std::move(framePtr);
			}

			os_sleepto_ns(now += delay);
		}
		catch (const std::exception& e)
		{
			blog(LOG_ERROR, "%s", e.what());

			// Force sleep to prevent log spamming
			os_sleep_ms(100);
		}
	}

	blog(LOG_INFO, "exiting thread");
}
