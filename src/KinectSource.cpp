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

#include "KinectSource.hpp"
#include <util/platform.h>
#include <array>

template<typename Interface>
struct CloseDeleter
{
	void operator()(Interface* handle) const
	{
		handle->Close();
	}
};

template<typename Interface>
struct ReleaseDeleter
{
	void operator()(Interface* handle) const
	{
		handle->Release();
	}
};

template<typename T> using ClosePtr = std::unique_ptr<T, CloseDeleter<T>>;
template<typename T> using ReleasePtr = std::unique_ptr<T, ReleaseDeleter<T>>;

KinectSource::KinectSource(obs_source_t* source) :
m_running(true),
m_source(source)
{
	m_thread = std::thread(&KinectSource::ThreadFunc, this);
}

KinectSource::~KinectSource()
{
	m_running = false;
	m_thread.join();
}

auto KinectSource::RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame, std::vector<uint8_t>& memory, bool forceRGBA) -> std::optional<ColorFrameData>
{
	IColorFrameReference* pColorFrameReference;
	if (FAILED(multiSourceFrame->get_ColorFrameReference(&pColorFrameReference)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to get color frame ref");
		return std::nullopt;
	}
	ReleasePtr<IColorFrameReference> colorFrameReference(pColorFrameReference);

	IColorFrame* pColorFrame;
	if (FAILED(pColorFrameReference->AcquireFrame(&pColorFrame)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to acquire color frame");
		return std::nullopt;
	}
	ReleasePtr<IColorFrame> colorFrame(pColorFrame);

	IFrameDescription* pColorFrameDescription;
	if (FAILED(pColorFrame->get_FrameDescription(&pColorFrameDescription)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to get color frame description");
		return std::nullopt;
	}
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
		blog(LOG_ERROR, "[obs-kinect] Failed to retrieve color frame description values");
		return std::nullopt;
	}

	ColorFrameData frameData;
	frameData.width = width;
	frameData.height = height;

	if (!forceRGBA || imageFormat == ColorImageFormat_Rgba)
	{
		auto SetupRawAccess = [&](video_format format)
		{
			BYTE* ptr;
			UINT size; // not used
			if (FAILED(colorFrame->AccessRawUnderlyingBuffer(&size, &ptr)))
			{
				blog(LOG_ERROR, "[obs-kinect] Failed to access color buffer");
				os_sleep_ms(100);
				return false;
			}

			frameData.ptr = ptr;
			frameData.pitch = width * bytePerPixel;
			frameData.format = format;

			return true;
		};

		switch (imageFormat)
		{
			case ColorImageFormat_Rgba:
				if (!SetupRawAccess(VIDEO_FORMAT_RGBA))
					return std::nullopt;

				return frameData;

			case ColorImageFormat_Bgra:
				if (!SetupRawAccess(VIDEO_FORMAT_BGRA))
					return std::nullopt;

				return frameData;

			/*
			FIXME: Output a black screen for some reasons
			case ColorImageFormat_Yuy2:
				if (!SetupRawAccess(VIDEO_FORMAT_YUY2))
					continue;

				return frameData;
			*/

			default:
				break;
		}
	}

	// Convert to RGBA
	memory.resize(width * height * 4);
	if (FAILED(colorFrame->CopyConvertedFrameDataToArray(UINT(memory.size()), reinterpret_cast<BYTE*>(memory.data()), ColorImageFormat_Rgba)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to copy color buffer");
		return std::nullopt;
	}

	frameData.ptr = memory.data();
	frameData.pitch = width * 4;
	frameData.format = VIDEO_FORMAT_RGBA;

	return frameData;
}

void KinectSource::ThreadFunc()
{
	IKinectSensor* pKinectSensor;
	if (FAILED(GetDefaultKinectSensor(&pKinectSensor)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to get kinect sensor");
		return;
	}

	ReleasePtr<IKinectSensor> kinectSensor(pKinectSensor);

	if (FAILED(kinectSensor->Open()))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to open kinect sensor");
		return;
	}

	ClosePtr<IKinectSensor> openedKinectSensor(kinectSensor.get());

	std::array<wchar_t, 256> wideId = { L"<failed to get id>" };
	openedKinectSensor->get_UniqueKinectId(UINT(wideId.size()), wideId.data());

	std::array<char, wideId.size()> id = { "<failed to get id>" };
	WideCharToMultiByte(CP_UTF8, 0, wideId.data(), int(wideId.size()), id.data(), int(id.size()), nullptr, nullptr);

	blog(LOG_INFO, "[obs-kinect] Found kinect sensor (%s)", id.data());

	IMultiSourceFrameReader* pMultiSourceFrameReader;
	if (FAILED(openedKinectSensor->OpenMultiSourceFrameReader(FrameSourceTypes_Color, &pMultiSourceFrameReader)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to acquire source frame reader");
		return;
	}

	ReleasePtr<IMultiSourceFrameReader> multiSourceFrameReader(pMultiSourceFrameReader);

	std::vector<std::uint8_t> colorMemory;

	uint64_t now = os_gettime_ns();
	uint64_t delay = 1'000'000'000ULL / 30;

	while (m_running)
	{
		IMultiSourceFrame* pMultiSourceFrame;
		if (FAILED(multiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame)))
			continue; // Kinect is probably busy starting up

		ReleasePtr<IMultiSourceFrame> multiSourceFrame(pMultiSourceFrame);

		std::optional<ColorFrameData> colorFrameDataOpt = RetrieveColorFrame(pMultiSourceFrame, colorMemory);
		if (!colorFrameDataOpt)
		{
			// Force sleep to prevent log spamming
			os_sleep_ms(100);
			continue;
		}

		ColorFrameData& colorFrameData = colorFrameDataOpt.value();

		obs_source_frame frame = {};
		frame.width = colorFrameData.width;
		frame.height = colorFrameData.height;
		frame.format = colorFrameData.format;

		frame.data[0] = colorFrameData.ptr;
		frame.linesize[0] = colorFrameData.pitch;

		frame.timestamp = now;

		obs_source_output_video(m_source, &frame);

		os_sleepto_ns(now += delay);
	}

	blog(LOG_INFO, "[obs-kinect] Exiting thread");
}
