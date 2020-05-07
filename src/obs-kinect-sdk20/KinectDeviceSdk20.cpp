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

#include "KinectDeviceSdk20.hpp"
#include <array>
#include <tlhelp32.h>

KinectDeviceSdk20::KinectDeviceSdk20() :
m_hasRequestedPrivilege(false)
{
	IKinectSensor* pKinectSensor;
	if (FAILED(GetDefaultKinectSensor(&pKinectSensor)))
		throw std::runtime_error("failed to get Kinect sensor");

	m_kinectSensor.reset(pKinectSensor);

	ICoordinateMapper* pCoordinateMapper;
	if (FAILED(m_kinectSensor->get_CoordinateMapper(&pCoordinateMapper)))
		throw std::runtime_error("failed to retrieve coordinate mapper");

	m_coordinateMapper.reset(pCoordinateMapper);

	SetUniqueName("Default Kinect");
}

bool KinectDeviceSdk20::MapColorToDepth(const std::uint16_t* depthValues, std::size_t valueCount, std::size_t colorPixelCount, DepthMappingFrameData::DepthCoordinates* depthCoordinatesOut) const
{
	static_assert(sizeof(UINT16) == sizeof(std::uint16_t));
	static_assert(sizeof(DepthMappingFrameData::DepthCoordinates) == sizeof(DepthSpacePoint));

	const UINT16* depthPtr = reinterpret_cast<const UINT16*>(depthValues);
	DepthSpacePoint* coordinatePtr = reinterpret_cast<DepthSpacePoint*>(depthCoordinatesOut);

	HRESULT r = m_coordinateMapper->MapColorFrameToDepthSpace(UINT(valueCount), depthPtr, UINT(colorPixelCount), coordinatePtr);
	if (FAILED(r))
		return false;

	return true;
}

auto KinectDeviceSdk20::RetrieveBodyIndexFrame(IMultiSourceFrame* multiSourceFrame) -> BodyIndexFrameData
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

auto KinectDeviceSdk20::RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame) -> ColorFrameData
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

auto KinectDeviceSdk20::RetrieveDepthFrame(IMultiSourceFrame* multiSourceFrame) -> DepthFrameData
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
	frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));

	return frameData;
}

DepthMappingFrameData KinectDeviceSdk20::RetrieveDepthMappingFrame(const KinectDeviceSdk20& device, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame)
{
	DepthMappingFrameData outputFrameData;
	outputFrameData.width = colorFrame.width;
	outputFrameData.height = colorFrame.height;

	std::size_t colorPixelCount = outputFrameData.width * outputFrameData.height;

	const std::uint16_t* depthPtr = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
	std::size_t depthPixelCount = depthFrame.width * depthFrame.height;

	outputFrameData.memory.resize(colorPixelCount * sizeof(DepthMappingFrameData::DepthCoordinates));

	DepthMappingFrameData::DepthCoordinates* coordinatePtr = reinterpret_cast<DepthMappingFrameData::DepthCoordinates*>(outputFrameData.memory.data());

	if (!device.MapColorToDepth(depthPtr, depthPixelCount, colorPixelCount, coordinatePtr))
		throw std::runtime_error("failed to map color to depth");

	outputFrameData.ptr.reset(coordinatePtr);
	outputFrameData.pitch = colorFrame.width * sizeof(DepthMappingFrameData::DepthCoordinates);

	return outputFrameData;
}

auto KinectDeviceSdk20::RetrieveInfraredFrame(IMultiSourceFrame* multiSourceFrame) -> InfraredFrameData
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

void KinectDeviceSdk20::SetServicePriority(ProcessPriority priority)
{
	DWORD priorityClass;
	switch (priority)
	{
		case ProcessPriority::High:        priorityClass = HIGH_PRIORITY_CLASS; break;
		case ProcessPriority::AboveNormal: priorityClass = ABOVE_NORMAL_PRIORITY_CLASS; break;
		case ProcessPriority::Normal:      priorityClass = NORMAL_PRIORITY_CLASS; break;

		default:
			warn("unknown process priority %d", int(priority));
			return;
	}

	if (!m_hasRequestedPrivilege)
	{
		LUID luid;
		if (!LookupPrivilegeValue(nullptr, SE_INC_BASE_PRIORITY_NAME, &luid))
		{
			warn("failed to get privilege SE_INC_BASE_PRIORITY_NAME");
			return;
		}

		TOKEN_PRIVILEGES tkp;
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Luid = luid;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		HANDLE token;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		{
			warn("failed to open processor token");
			return;
		}
		HandlePtr tokenOwner(token);

		if (!AdjustTokenPrivileges(token, FALSE, &tkp, sizeof(tkp), nullptr, nullptr))
		{
			warn("failed to adjust token privileges");
			return;
		}

		info("adjusted token privileges successfully");
		m_hasRequestedPrivilege = true;
	}

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
	{
		warn("failed to retrieve processes snapshot");
		return;
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
					return;
				}
				HandlePtr processOwner(process);

				if (!SetPriorityClass(process, priorityClass))
				{
					warn("failed to update process priority");
					return;
				}

				info("KinectService.exe priority updated successfully to %s", ProcessPriorityToString(priority));
				return;
			}
		}
		while (Process32Next(snapshot, &entry));
	}

	warn("KinectService.exe not found");
}

void KinectDeviceSdk20::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& error)
{
	ReleasePtr<IMultiSourceFrameReader> multiSourceFrameReader;
	ClosePtr<IKinectSensor> openedKinectSensor;

	EnabledSourceFlags enabledSourceFlags = 0;
	DWORD enabledFrameSourceTypes = 0;

	auto UpdateMultiSourceFrameReader = [&](EnabledSourceFlags enabledSources)
	{
		DWORD newFrameSourcesTypes = 0;
		if (enabledSources & Source_Body)
			newFrameSourcesTypes |= FrameSourceTypes_BodyIndex;

		if (enabledSources & (Source_Color | Source_ColorToDepthMapping))
			newFrameSourcesTypes |= FrameSourceTypes_Color;

		if (enabledSources & (Source_Depth | Source_ColorToDepthMapping))
			newFrameSourcesTypes |= FrameSourceTypes_Depth;

		if (enabledSources & Source_Infrared)
			newFrameSourcesTypes |= FrameSourceTypes_Infrared;

		if (!multiSourceFrameReader || newFrameSourcesTypes != enabledFrameSourceTypes)
		{
			IMultiSourceFrameReader* pMultiSourceFrameReader;
			if (FAILED(openedKinectSensor->OpenMultiSourceFrameReader(newFrameSourcesTypes, &pMultiSourceFrameReader)))
				throw std::runtime_error("failed to acquire source frame reader");

			multiSourceFrameReader.reset(pMultiSourceFrameReader);
		}

		enabledFrameSourceTypes = newFrameSourcesTypes;
		enabledSourceFlags = enabledSources;

		info("Kinect active sources: %s", EnabledSourceToString(enabledSourceFlags).c_str());
	};

	try
	{
		if (FAILED(m_kinectSensor->Open()))
			throw std::runtime_error("failed to open Kinect sensor");

		openedKinectSensor.reset(m_kinectSensor.get());

		std::array<wchar_t, 256> wideId = { L"<failed to get id>" };
		openedKinectSensor->get_UniqueKinectId(UINT(wideId.size()), wideId.data());

		std::array<char, wideId.size()> id = { "<failed to get id>" };
		WideCharToMultiByte(CP_UTF8, 0, wideId.data(), int(wideId.size()), id.data(), int(id.size()), nullptr, nullptr);

		info("found kinect sensor (%s)", id.data());
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

	constexpr std::uint64_t MaxKinectFPS = 30;

	uint64_t now = os_gettime_ns();
	uint64_t delay = 1'000'000'000ULL / MaxKinectFPS;

	while (IsRunning())
	{
		{
			if (auto sourceFlagUpdate = GetSourceFlagsUpdate())
			{
				try
				{
					UpdateMultiSourceFrameReader(sourceFlagUpdate.value());
				}
				catch (const std::exception& e)
				{
					blog(LOG_ERROR, "%s", e.what());

					os_sleep_ms(10);
					continue;
				}
			}
		}

		if (!multiSourceFrameReader)
		{
			os_sleep_ms(100);
			continue;
		}

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
			if (enabledSourceFlags & Source_Body)
				framePtr->bodyIndexFrame = RetrieveBodyIndexFrame(multiSourceFrame.get());

			if (enabledSourceFlags & (Source_Color | Source_ColorToDepthMapping))
				framePtr->colorFrame = RetrieveColorFrame(multiSourceFrame.get());

			if (enabledSourceFlags & (Source_Depth | Source_ColorToDepthMapping))
				framePtr->depthFrame = RetrieveDepthFrame(multiSourceFrame.get());

			if (enabledSourceFlags & Source_Infrared)
				framePtr->infraredFrame = RetrieveInfraredFrame(multiSourceFrame.get());

			if (enabledSourceFlags & Source_ColorToDepthMapping)
				framePtr->depthMappingFrame = RetrieveDepthMappingFrame(*this, *framePtr->colorFrame, *framePtr->depthFrame);

			UpdateFrame(std::move(framePtr));
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
