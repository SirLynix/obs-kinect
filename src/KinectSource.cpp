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
#include <algorithm>
#include <array>
#include <numeric>
#include <optional>

KinectSource::KinectSource(obs_source_t* source) :
m_running(false),
m_sourceType(SourceType::Color),
m_requiredMemory(0),
m_source(source),
m_stopOnHide(false)
{
}

KinectSource::~KinectSource()
{
	Stop();
}

void KinectSource::OnVisibilityUpdate(bool isVisible)
{
	if (isVisible)
		Start();
	else if (m_stopOnHide)
		Stop();
}

void KinectSource::SetSourceType(SourceType sourceType)
{
	m_sourceType.store(sourceType, std::memory_order_relaxed);
}

void KinectSource::UpdateDepthToColor(DepthToColorSettings depthToColor)
{
	m_depthToColorSettings = depthToColor;
}

void KinectSource::UpdateGreenScreen(GreenScreenSettings greenScreen)
{
	m_greenScreenSettings = greenScreen;
}

void KinectSource::UpdateInfraredToColor(InfraredToColorSettings infraredToColor)
{
	m_infraredToColorSettings = infraredToColor;
}

void KinectSource::ShouldStopOnHide(bool shouldStop)
{
	m_stopOnHide = shouldStop;
}

uint8_t* KinectSource::AllocateMemory(std::vector<uint8_t>& fallback, std::size_t size)
{
	std::size_t previousSize = m_memory.size();

	if (m_memory.capacity() >= previousSize + size)
	{
		// Resizing is safe, do it
		m_memory.resize(previousSize + size);
		return &m_memory[previousSize];
	}
	else
	{
		// Resizing would invalidate pointers, allocate to a fallback memory
		fallback.resize(size);

		// Ask for more memory next frame
		m_requiredMemory += size;

		return fallback.data();
	}
}

auto KinectSource::ConvertDepthToColor(const DepthToColorSettings& settings, const DepthFrameData& depthFrame) -> ColorFrameData
{
	constexpr float DepthInputMaxValue = 0xFFFF;
	constexpr float DepthInputMinValue = 0.f;
	constexpr float DepthOutputMax = 1.f;
	constexpr float DepthOutputMin = 0.01f;

	float invFactor;
	if (settings.dynamic)
	{
		const std::uint16_t* depthValues = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
		std::size_t depthValueCount = depthFrame.width * depthFrame.height;

		DynamicValues dynValues = ComputeDynamicValues(depthValues, depthValueCount);
		invFactor = float(1.0 / (dynValues.average * dynValues.standardDeviation));
	}
	else
		invFactor = 1.f / (settings.averageValue * settings.standardDeviation);

	ColorFrameData colorFrame;

	std::uint8_t* memPtr = AllocateMemory(colorFrame.fallbackMemory, depthFrame.width * depthFrame.height * 4);
	std::uint8_t* output = memPtr;
	const std::uint16_t* input = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());

	for (std::size_t y = 0; y < depthFrame.height; ++y)
	{
		for (std::size_t x = 0; x < depthFrame.width; ++x)
		{
			std::uint16_t value = input[y * depthFrame.width + x];

			float intensityFactor = float(value) / DepthInputMaxValue;
			intensityFactor *= invFactor;
			intensityFactor = std::clamp(intensityFactor, DepthOutputMin, DepthOutputMax);

			std::uint8_t intensity = static_cast<std::uint8_t>(intensityFactor * 255.f);

			*output++ = intensity;
			*output++ = intensity;
			*output++ = intensity;
			*output++ = 255;
		}
	}

	colorFrame.format = VIDEO_FORMAT_RGBA;
	colorFrame.height = depthFrame.height;
	colorFrame.pitch = depthFrame.width * 4;
	colorFrame.ptr.reset(memPtr);
	colorFrame.width = depthFrame.width;

	return colorFrame;
}

auto KinectSource::ConvertInfraredToColor(const InfraredToColorSettings& settings, const InfraredFrameData& infraredFrame) -> ColorFrameData
{
	// Values from InfraredBasics example from Kinect SDK
	constexpr float InfraredInputMaxValue = 0xFFFF;
	constexpr float InfraredOutputMax = 1.f;
	constexpr float InfraredOutputMin = 0.01f;

	float invFactor;
	if (settings.dynamic)
	{
		const std::uint16_t* infraredValues = reinterpret_cast<const std::uint16_t*>(infraredFrame.ptr.get());
		std::size_t infraredValueCount = infraredFrame.width * infraredFrame.height;

		DynamicValues dynValues = ComputeDynamicValues(infraredValues, infraredValueCount);
		invFactor = float(1.0 / (dynValues.average * dynValues.standardDeviation));
	}
	else
		invFactor = 1.f / (settings.averageValue * settings.standardDeviation);

	ColorFrameData colorFrame;

	std::uint8_t* memPtr = AllocateMemory(colorFrame.fallbackMemory, infraredFrame.width * infraredFrame.height * 4);
	std::uint8_t* output = memPtr;
	const std::uint16_t* input = reinterpret_cast<const std::uint16_t*>(infraredFrame.ptr.get());

	for (std::size_t y = 0; y < infraredFrame.height; ++y)
	{
		for (std::size_t x = 0; x < infraredFrame.width; ++x)
		{
			std::uint16_t value = input[y * infraredFrame.width + x];

			float intensityFactor = float(value) / InfraredInputMaxValue;
			intensityFactor *= invFactor;
			intensityFactor = std::clamp(intensityFactor, InfraredOutputMin, InfraredOutputMax);

			std::uint8_t intensity = static_cast<std::uint8_t>(intensityFactor * 255.f);

			*output++ = intensity;
			*output++ = intensity;
			*output++ = intensity;
			*output++ = 255;
		}
	}

	colorFrame.format = VIDEO_FORMAT_RGBA;
	colorFrame.height = infraredFrame.height;
	colorFrame.pitch = infraredFrame.width * 4;
	colorFrame.ptr.reset(memPtr);
	colorFrame.width = infraredFrame.width;

	return colorFrame;
}

auto KinectSource::RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame, bool forceRGBA) -> ColorFrameData
{
	ColorFrameData frameData;

	IColorFrameReference* pColorFrameReference;
	if (FAILED(multiSourceFrame->get_ColorFrameReference(&pColorFrameReference)))
		throw std::runtime_error("Failed to get color frame reference");

	ReleasePtr<IColorFrameReference> colorFrameReference(pColorFrameReference);

	IColorFrame* pColorFrame;
	if (FAILED(pColorFrameReference->AcquireFrame(&pColorFrame)))
		throw std::runtime_error("Failed to acquire color frame");

	frameData.colorFrame.reset(pColorFrame);

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
	    FAILED(pColorFrame->get_RawColorImageFormat(&imageFormat)))
	{
		throw std::runtime_error("Failed to retrieve color frame description values");
	}

	frameData.width = width;
	frameData.height = height;

	if (!forceRGBA || imageFormat == ColorImageFormat_Rgba)
	{
		auto SetupRawAccess = [&](video_format format) -> ColorFrameData
		{
			BYTE* ptr;
			UINT pixelCount;
			if (FAILED(pColorFrame->AccessRawUnderlyingBuffer(&pixelCount, &ptr)) || pixelCount != width * height)
				throw std::runtime_error("Failed to access color buffer");

			frameData.ptr.reset(ptr);
			frameData.pitch = width * bytePerPixel;
			frameData.format = format;

			return std::move(frameData);
		};

		switch (imageFormat)
		{
			case ColorImageFormat_Rgba: return SetupRawAccess(VIDEO_FORMAT_RGBA);
			case ColorImageFormat_Bgra: return SetupRawAccess(VIDEO_FORMAT_BGRA);

			/*
			FIXME: Output a black screen for some reasons
			case ColorImageFormat_Yuy2: return SetupRawAccess(VIDEO_FORMAT_YUY2);
			*/

			default:
				break;
		}
	}

	// Convert to RGBA
	std::size_t memSize = width * height * 4;
	std::uint8_t* memPtr = AllocateMemory(frameData.fallbackMemory, memSize);

	if (FAILED(pColorFrame->CopyConvertedFrameDataToArray(UINT(memSize), reinterpret_cast<BYTE*>(memPtr), ColorImageFormat_Rgba)))
		throw std::runtime_error("Failed to copy color buffer");

	frameData.ptr.reset(memPtr);
	frameData.pitch = width * 4;
	frameData.format = VIDEO_FORMAT_RGBA;

	return frameData;
}

auto KinectSource::RetrieveDepthFrame(IMultiSourceFrame* multiSourceFrame) -> DepthFrameData
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
	if (FAILED(pDepthFrame->get_FrameDescription(&pDepthFrameDescription)))
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

	UINT16* ptr;
	UINT pixelCount;
	if (FAILED(pDepthFrame->AccessUnderlyingBuffer(&pixelCount, &ptr)) || pixelCount != width * height)
		throw std::runtime_error("Failed to access depth frame buffer");

	DepthFrameData frameData;
	frameData.width = width;
	frameData.height = height;
	frameData.pitch = width * bytePerPixel;
	frameData.ptr.reset(reinterpret_cast<std::uint8_t*>(ptr));
	frameData.depthFrame = std::move(depthFrame);

	return frameData;
}

auto KinectSource::RetrieveInfraredFrame(IMultiSourceFrame* multiSourceFrame) -> InfraredFrameData
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
	if (FAILED(pInfraredFrame->get_FrameDescription(&pInfraredFrameDescription)))
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

	UINT16* ptr;
	UINT pixelCount;
	if (FAILED(pInfraredFrame->AccessUnderlyingBuffer(&pixelCount, &ptr)) || pixelCount != width * height)
		throw std::runtime_error("Failed to access infrared frame buffer");

	InfraredFrameData frameData;
	frameData.width = width;
	frameData.height = height;
	frameData.pitch = width * bytePerPixel;
	frameData.ptr.reset(reinterpret_cast<std::uint8_t*>(ptr));
	frameData.infraredFrame = std::move(infraredFrame);

	return frameData;
}

void KinectSource::Start()
{
	if (m_running)
		return;

	std::mutex mutex;
	std::condition_variable cv;

	m_running = true;

	std::unique_lock<std::mutex> lock(mutex);
	m_thread = std::thread(&KinectSource::ThreadFunc, this, std::ref(cv), std::ref(mutex));

	// Wait until thread has been activated
	cv.wait(lock);
}

void KinectSource::Stop()
{
	if (!m_running)
		return;

	m_running = false;
	m_thread.join();
}

void KinectSource::ThreadFunc(std::condition_variable& cv, std::mutex& m)
{
	IKinectSensor* pKinectSensor;
	if (FAILED(GetDefaultKinectSensor(&pKinectSensor)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to get kinect sensor");
		return;
	}

	ReleasePtr<IKinectSensor> kinectSensor(pKinectSensor);

	ICoordinateMapper* pCoordinateMapper;
	if (FAILED(kinectSensor->get_CoordinateMapper(&pCoordinateMapper)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to retrieve coordinate mapper");
		return;
	}
	ReleasePtr<ICoordinateMapper> coordinateMapper(pCoordinateMapper);

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

	DWORD enabledSources = 0;
	// Is the cost of enabling color/depth/infrared sources that high? Should we add an option to only enable thoses in use?
	/*switch (m_sourceType)
	{
		case SourceType::Color: enabledSources |= FrameSourceTypes_Color; break;
		case SourceType::Depth: enabledSources |= FrameSourceTypes_Depth; break;
		case SourceType::Infrared: enabledSources |= FrameSourceTypes_Infrared; break;
		default: break;
	}*/

	enabledSources |= FrameSourceTypes_Color | FrameSourceTypes_Depth | FrameSourceTypes_Infrared;

	IMultiSourceFrameReader* pMultiSourceFrameReader;
	if (FAILED(openedKinectSensor->OpenMultiSourceFrameReader(enabledSources, &pMultiSourceFrameReader)))
	{
		blog(LOG_ERROR, "[obs-kinect] Failed to acquire source frame reader");
		return;
	}
	ReleasePtr<IMultiSourceFrameReader> multiSourceFrameReader(pMultiSourceFrameReader);

	{
		std::unique_lock<std::mutex> lk(m);
		cv.notify_all();
	} // m & cv no longer exists from here

	uint64_t now = os_gettime_ns();
	uint64_t delay = 1'000'000'000ULL / 30; // Target 30 FPS (Kinect doesn't run higher afaik)

	while (m_running)
	{
		IMultiSourceFrame* pMultiSourceFrame;
		if (FAILED(multiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame)))
			continue; // Kinect is probably busy starting up

		ReleasePtr<IMultiSourceFrame> multiSourceFrame(pMultiSourceFrame);

		try
		{
			m_memory.clear(); //< Does not affect capacity
			m_memory.reserve(m_requiredMemory);

			auto GetColorFrame = [&, frameDataOpt = std::optional<ColorFrameData>()]() mutable -> ColorFrameData&
			{
				if (!frameDataOpt.has_value())
					frameDataOpt = RetrieveColorFrame(pMultiSourceFrame);

				return frameDataOpt.value();
			};

			auto GetDepthFrame = [&, frameDataOpt = std::optional<DepthFrameData>()]() mutable -> DepthFrameData&
			{
				if (!frameDataOpt.has_value())
					frameDataOpt = RetrieveDepthFrame(pMultiSourceFrame);

				return frameDataOpt.value();
			};

			auto GetInfraredFrame = [&, frameDataOpt = std::optional<InfraredFrameData>()]() mutable-> InfraredFrameData&
			{
				if (!frameDataOpt.has_value())
					frameDataOpt = RetrieveInfraredFrame(pMultiSourceFrame);

				return frameDataOpt.value();
			};

			ColorFrameData outputFrameData;

			switch (m_sourceType.load(std::memory_order_relaxed))
			{
				case SourceType::Color:
					outputFrameData = std::move(GetColorFrame());
					break;

				case SourceType::Depth:
					outputFrameData = ConvertDepthToColor(m_depthToColorSettings.load(std::memory_order_relaxed), GetDepthFrame());
					break;

				case SourceType::Infrared:
					outputFrameData = ConvertInfraredToColor(m_infraredToColorSettings.load(std::memory_order_relaxed), GetInfraredFrame());
					break;

				default:
					throw std::runtime_error("invalid source type");
			}

			auto GetDepthCoordinates = [&, depthCoordinatesPtr = std::optional<DepthSpacePoint*>(), fallbackMem = std::vector<std::uint8_t>()]() mutable->DepthSpacePoint*
			{
				if (!depthCoordinatesPtr)
				{
					std::size_t colorPixelCount = outputFrameData.width * outputFrameData.height;

					DepthFrameData& depthFrame = GetDepthFrame();
					if (outputFrameData.width != depthFrame.width || outputFrameData.height != depthFrame.height)
					{
						const UINT16* depthPtr = reinterpret_cast<const UINT16*>(depthFrame.ptr.get());
						std::size_t depthPixelCount = depthFrame.width * depthFrame.height;

						DepthSpacePoint* coordinatePtr = reinterpret_cast<DepthSpacePoint*>(AllocateMemory(fallbackMem, colorPixelCount * sizeof(DepthSpacePoint)));

						if (FAILED(coordinateMapper->MapColorFrameToDepthSpace(UINT(depthPixelCount), depthPtr, UINT(colorPixelCount), coordinatePtr)))
							throw std::runtime_error("failed to map color to depth");

						depthCoordinatesPtr = coordinatePtr;
					}
					else
					{
						// Don't map depth to depth/infrared
						depthCoordinatesPtr = nullptr;
					}
				}

				return depthCoordinatesPtr.value();
			};

			if (GreenScreenSettings greenScreenConfig = m_greenScreenSettings.load(std::memory_order_relaxed); greenScreenConfig.enabled)
				outputFrameData = VirtualGreenScreen(greenScreenConfig, outputFrameData, GetDepthFrame(), GetDepthCoordinates());

			obs_source_frame frame = {};
			frame.width = outputFrameData.width;
			frame.height = outputFrameData.height;
			frame.format = outputFrameData.format;

			frame.data[0] = outputFrameData.ptr.get();
			frame.linesize[0] = outputFrameData.pitch;

			frame.timestamp = now;

			obs_source_output_video(m_source, &frame);

			os_sleepto_ns(now += delay);
		}
		catch (const std::exception& e)
		{
			blog(LOG_ERROR, "[obs-kinect] %s", e.what());

			// Force sleep to prevent log spamming
			os_sleep_ms(100);
		}
	}

	blog(LOG_INFO, "[obs-kinect] Exiting thread");
}

auto KinectSource::VirtualGreenScreen(const GreenScreenSettings& settings, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame, const DepthSpacePoint* depthMapping) -> ColorFrameData
{
	//TODO: Update color frame directly?

	// A lot of this function's code is based on optimization I know compilers do and on processor branch prediction

	constexpr float InvalidDepth = -std::numeric_limits<float>::infinity();

	if (colorFrame.format != VIDEO_FORMAT_RGBA)
		throw std::runtime_error("unexpected color format");

	ColorFrameData virtualGreenScreen;
	virtualGreenScreen.width = colorFrame.width;
	virtualGreenScreen.height = colorFrame.height;
	virtualGreenScreen.pitch = colorFrame.pitch;
	virtualGreenScreen.format = colorFrame.format;
	virtualGreenScreen.ptr.reset(AllocateMemory(virtualGreenScreen.fallbackMemory, colorFrame.width * colorFrame.height * 4));

	const std::uint16_t* depthPixels = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());

	// You're entering the "I hope compiler-senpai will be able to optimize this"
	// also known as "oh that's why it runs at 2 fps on debug"
	auto DepthTest = [&](std::uint16_t depthValue)
	{
		if (depthValue < settings.depthMin || depthValue > settings.depthMax)
			return false;

		return true;
	};

	float invDepthProgressive = 1.f / settings.fadeDist;

	auto AlphaValue = [&](std::uint16_t depthValue)
	{
		if (depthValue < settings.depthMin)
			return 0.f;

		if (depthValue > settings.depthMax)
		{
			if (depthValue - settings.depthMax < settings.fadeDist)
				return 1.f - float(depthValue - settings.depthMax) * invDepthProgressive;

			return 0.f;
		}

		return 1.f;
	};

	std::uint8_t* output = virtualGreenScreen.ptr.get();
	auto NoPixel = [&]()
	{
		*output++ = 0;
		*output++ = 0;
		*output++ = 0;
		*output++ = 0;
	};

	auto PartialPixel = [&](std::uint8_t* rgba, float alphaFactor)
	{
		*output++ = *rgba++; //< r
		*output++ = *rgba++; //< g
		*output++ = *rgba++; //< b
		*output++ = static_cast<std::uint8_t>(*rgba++ * alphaFactor); //< a
	};

	auto FullPixel = [&](std::uint8_t* rgba)
	{
		*output++ = *rgba++; //< r
		*output++ = *rgba++; //< g
		*output++ = *rgba++; //< b
		*output++ = *rgba++; //< a
	};

	auto ColorBasedOnDepth = [&](std::size_t x, std::size_t y, std::uint16_t depthValue)
	{
		if (settings.fadeDist == 0) //< Costs nothing, thanks to branch-predictor senpai
		{
			if (DepthTest(depthValue))
			{
				std::uint8_t* colorPtr = &colorFrame.ptr[colorFrame.pitch * y + x * 4];
				FullPixel(colorPtr);
			}
			else
				NoPixel();
		}
		else
		{
			float alphaFactor = AlphaValue(depthValue);
			std::uint8_t* colorPtr = &colorFrame.ptr[colorFrame.pitch * y + x * 4];
			PartialPixel(colorPtr, alphaFactor);
		}
	};

	for (std::size_t y = 0; y < virtualGreenScreen.height; ++y)
	{
		for (std::size_t x = 0; x < virtualGreenScreen.width; ++x)
		{
			if (depthMapping)
			{
				const DepthSpacePoint& depthCoordinates = depthMapping[y * virtualGreenScreen.width + x];
				if (depthCoordinates.X == InvalidDepth || depthCoordinates.Y == InvalidDepth)
				{
					NoPixel();
					continue;
				}

				switch (settings.filtering)
				{
					case DepthFiltering::BilinearFiltering:
					{
						auto DepthAt = [&](std::uint32_t x, std::uint32_t y)
						{
							x = std::min(x, depthFrame.width);
							y = std::min(y, depthFrame.height);
							return depthPixels[depthFrame.width * y + x];
						};

						auto AlphaFactor = [&](std::uint32_t x, std::uint32_t y) -> float
						{
							return AlphaValue(DepthAt(x, y));
						};

						std::uint32_t dX = static_cast<std::uint32_t>(depthCoordinates.X);
						std::uint32_t dY = static_cast<std::uint32_t>(depthCoordinates.Y);

						float tx = depthCoordinates.X - dX;
						float ty = depthCoordinates.Y - dY;

						std::uint16_t d00 = DepthAt(dX, dY);
						std::uint16_t d10 = DepthAt(dX + 1, dY);
						std::uint16_t d01 = DepthAt(dX, dY + 1);
						std::uint16_t d11 = DepthAt(dX + 1, dY + 1);
						auto Lerp = [](float from, float to, float factor)
						{
							// 0 means no depth value, take the neighbor's one
							if (from == 0)
								return to;
							else if (to == 0)
								return from;

							return from * (1.f - factor) + to * factor;
						};

						std::uint16_t depthValue = static_cast<std::uint16_t>(Lerp(Lerp(d00, d10, tx), Lerp(d10, d11, tx), ty));
						ColorBasedOnDepth(x, y, depthValue);
						/*float v00 = AlphaValue(DepthAt(dX, dY));
						float v10 = AlphaValue(DepthAt(dX + 1, dY));
						float v01 = AlphaValue(DepthAt(dX, dY + 1));
						float v11 = AlphaValue(DepthAt(dX + 1, dY + 1));

						auto Lerp = [](float from, float to, float factor)
						{
							return from * (1.f - factor) + to * factor;
						};

						// Apply bilinear filtering (lerp horizontally then vertically)
						float alphaFactor = Lerp(Lerp(v00, v10, tx), Lerp(v10, v11, tx), ty);

						std::uint8_t* colorPtr = &colorFrame.ptr[colorFrame.pitch * y + x * 4];
						PartialPixel(colorPtr, alphaFactor);*/
						break;
					}

					case DepthFiltering::NoFiltering:
					{
						int dX = static_cast<int>(depthCoordinates.X + 0.5f);
						int dY = static_cast<int>(depthCoordinates.Y + 0.5f);

						if (dX < 0 || dX >= int(depthFrame.width) ||
						    dY < 0 || dY >= int(depthFrame.height))
						{
							NoPixel();
							continue;
						}

						std::uint16_t depthValue = depthPixels[depthFrame.width * dY + dX];
						ColorBasedOnDepth(x, y, depthValue);
						break;
					}

					default:
						NoPixel();
						break;
				}
			}
			else
			{
				std::uint16_t depthValue = depthPixels[depthFrame.width * y + x];
				ColorBasedOnDepth(x, y, depthValue);
			}
		}
	}

	return virtualGreenScreen;
}

auto KinectSource::ComputeDynamicValues(const std::uint16_t* values, std::size_t valueCount) -> DynamicValues
{
	constexpr std::uint16_t MaxValue = std::numeric_limits<std::uint16_t>::max();

	unsigned long long average = std::accumulate(values, values + valueCount, 0LL) / valueCount;
	unsigned long long varianceAcc = std::accumulate(values, values + valueCount, 0LL, [average](unsigned long long init, unsigned long long delta)
	{
		return init + (delta - average) * (delta - average); // underflow allowed (will overflow back to the right value)
	});

	double variance = double(varianceAcc) / valueCount;

	double averageValue = double(average) / MaxValue;
	double standardDeviation = std::sqrt(variance / MaxValue);

	return { averageValue, standardDeviation };
}
