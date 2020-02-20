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

#define blog(log_level, format, ...)                    \
	blog(log_level, "[obs-kinect] " format, ##__VA_ARGS__)

#define debug(format, ...) blog(LOG_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define warn(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

KinectSource::KinectSource(obs_source_t* source) :
m_sourceType(SourceType::Color),
m_source(source),
m_stopOnHide(false)
{
	m_device.StartCapture();

	ObsGraphics obsGfx;

	m_bodyIndexTexture = gs_texture_create(512, 424, GS_R8, 1, nullptr, GS_DYNAMIC);
	m_colorTexture = gs_texture_create(1920, 1080, GS_RGBA, 1, nullptr, GS_DYNAMIC);
	m_depthMappingTexture = gs_texture_create(1920, 1080, GS_RG32F, 1, nullptr, GS_DYNAMIC);
	m_depthTexture = gs_texture_create(512, 424, GS_R16, 1, nullptr, GS_DYNAMIC);
	m_infraredTexture = gs_texture_create(512, 424, GS_R16, 1, nullptr, GS_DYNAMIC);

	m_alphaMaskFilter.emplace();
	m_bodyIndexFilterEffect.emplace();
	m_depthIRConvertEffect.emplace();
	m_gaussianBlur.emplace(GS_RGBA);
	m_depthFilter.emplace();
}

KinectSource::~KinectSource()
{
	ObsGraphics obsGfx;
	gs_texture_destroy(m_bodyIndexTexture);
	gs_texture_destroy(m_colorTexture);
	gs_texture_destroy(m_depthMappingTexture);
	gs_texture_destroy(m_depthTexture);
	gs_texture_destroy(m_infraredTexture);
}

void KinectSource::OnVisibilityUpdate(bool isVisible)
{
	if (isVisible)
		m_device.StartCapture(); //< Does nothing if already capturing
	else if (m_stopOnHide)
	{
		m_device.StopCapture();
		m_finalTexture.reset();
	}
}

void KinectSource::SetSourceType(SourceType sourceType)
{
	if (m_sourceType != sourceType)
	{
		m_finalTexture.reset();
		m_sourceType = sourceType;
	}
}

void KinectSource::UpdateDepthToColor(DepthToColorSettings depthToColor)
{
	m_depthToColorSettings = depthToColor;
}

void KinectSource::UpdateGreenScreen(GreenScreenSettings greenScreen)
{
	if (greenScreen.enabled != m_greenScreenSettings.enabled)
		m_finalTexture.reset();

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

void KinectSource::Render()
{
	if (!m_finalTexture)
		return;

	gs_effect_t* defaultEffect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t* image = gs_effect_get_param_by_name(defaultEffect, "image");
	gs_technique_t* tech = gs_effect_get_technique(defaultEffect, "Draw");

	gs_effect_set_texture(image, m_finalTexture.get());

	if (m_greenScreenSettings.enabled)
	{
		gs_blend_state_push();
		gs_reset_blend_state();
	}

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_draw_sprite(m_finalTexture.get(), 0, 0, 0);
	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	if (m_greenScreenSettings.enabled)
		gs_blend_state_pop();

}

void KinectSource::Update(float /*seconds*/)
{
	auto UpdateTexture = [](gs_texture_t*& texture, std::uint32_t width, std::uint32_t height, std::uint32_t pitch, const std::uint8_t* content)
	{
		if (width != gs_texture_get_width(texture) || height != gs_texture_get_height(texture))
		{
			gs_color_format format = gs_texture_get_color_format(texture);

			gs_texture_destroy(texture);
			texture = gs_texture_create(width, height, format, 1, &content, GS_DYNAMIC);
		}
		else
		{
			uint8_t* ptr;
			uint32_t pitch;
			if (gs_texture_map(texture, &ptr, &pitch))
			{
				if (pitch == pitch)
					std::memcpy(ptr, content, pitch * height);
				else
				{
					std::uint32_t bestPitch = std::min(pitch, pitch);
					for (std::size_t y = 0; y < height; ++y)
					{
						const std::uint8_t* input = &content[y * pitch];
						std::uint8_t* output = ptr + y * pitch;

						std::memcpy(output, input, bestPitch);
					}
				}

				gs_texture_unmap(texture);
			}
		}
	};

	try
	{
		auto frameData = m_device.GetLastFrame();
		if (!frameData)
			return;

		ObsGraphics obsGfx;

		if (m_greenScreenSettings.enabled || m_sourceType == SourceType::Depth)
		{
			KinectDevice::DepthFrameData& depthFrame = frameData->depthFrame.value();
			UpdateTexture(m_depthTexture, depthFrame.width, depthFrame.height, depthFrame.pitch, depthFrame.ptr.get());
		}

		gs_texture_t* sourceTexture;
		switch (m_sourceType)
		{
			case SourceType::Color:
			{
				if (!frameData->colorFrame.has_value())
					return;

				KinectDevice::ColorFrameData& colorFrame = frameData->colorFrame.value();

				UpdateTexture(m_colorTexture, colorFrame.width, colorFrame.height, colorFrame.pitch, colorFrame.ptr.get());
				sourceTexture = m_colorTexture;
				break;
			}

			case SourceType::Depth:
			{
				float averageValue;
				float standardDeviation;
				if (m_depthToColorSettings.dynamic)
				{
					KinectDevice::DepthFrameData& depthFrame = frameData->depthFrame.value();

					const std::uint16_t* depthValues = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
					std::size_t depthValueCount = depthFrame.width * depthFrame.height;

					DynamicValues dynValues = ComputeDynamicValues(depthValues, depthValueCount);
					averageValue = float(dynValues.average);
					standardDeviation = float(dynValues.standardDeviation);
				}
				else
				{
					averageValue = m_depthToColorSettings.averageValue;
					standardDeviation = m_depthToColorSettings.standardDeviation;
				}

				sourceTexture = m_depthIRConvertEffect->Convert(1920, 1080, m_depthTexture, averageValue, standardDeviation);
				break;
			}

			case SourceType::Infrared:
			{
				if (!frameData->infraredFrame.has_value())
					return;

				KinectDevice::InfraredFrameData& irFrame = frameData->infraredFrame.value();

				float averageValue;
				float standardDeviation;
				if (m_infraredToColorSettings.dynamic)
				{
					const std::uint16_t* irValues = reinterpret_cast<const std::uint16_t*>(irFrame.ptr.get());
					std::size_t irValueCount = irFrame.width * irFrame.height;

					DynamicValues dynValues = ComputeDynamicValues(irValues, irValueCount);
					averageValue = float(dynValues.average);
					standardDeviation = float(dynValues.standardDeviation);
				}
				else
				{
					averageValue = m_infraredToColorSettings.averageValue;
					standardDeviation = m_infraredToColorSettings.standardDeviation;
				}

				UpdateTexture(m_infraredTexture, irFrame.width, irFrame.height, irFrame.pitch, irFrame.ptr.get());
				sourceTexture = m_depthIRConvertEffect->Convert(1920, 1080, m_infraredTexture, averageValue, standardDeviation);
				break;
			}

			default:
				break;
		}

		if (m_greenScreenSettings.enabled)
		{
			gs_texture_t* depthMappingTexture;
			if (m_sourceType == SourceType::Color)
			{
				KinectDevice::ColorFrameData& colorFrame = frameData->colorFrame.value();
				KinectDevice::DepthFrameData& depthFrame = frameData->depthFrame.value();

				DepthMappingFrameData depthMappingFrame = RetrieveDepthMappingFrame(colorFrame, depthFrame);

				UpdateTexture(m_depthMappingTexture, depthMappingFrame.width, depthMappingFrame.height, depthMappingFrame.pitch, depthMappingFrame.ptr.get());
				depthMappingTexture = m_depthMappingTexture;
			}
			else
				depthMappingTexture = nullptr;

			gs_texture_t* filterTexture;
			if (m_greenScreenSettings.type == GreenScreenType::Depth)
			{
				DepthFilterEffect::Params filterParams;
				filterParams.colorToDepthTexture = depthMappingTexture;
				filterParams.depthTexture = m_depthTexture;
				filterParams.maxDepth = m_greenScreenSettings.depthMax;
				filterParams.minDepth = m_greenScreenSettings.depthMin;
				filterParams.progressiveDepth = m_greenScreenSettings.fadeDist;

				std::uint32_t width = gs_texture_get_width(sourceTexture);
				std::uint32_t height = gs_texture_get_height(sourceTexture);

				filterTexture = m_depthFilter->Filter(width, height, filterParams);
			}
			else
			{
				if (!frameData->bodyIndexFrame.has_value())
					return;

				KinectDevice::BodyIndexFrameData& bodyIndexFrame = frameData->bodyIndexFrame.value();
				UpdateTexture(m_bodyIndexTexture, bodyIndexFrame.width, bodyIndexFrame.height, bodyIndexFrame.pitch, bodyIndexFrame.ptr.get());

				BodyIndexFilterEffect::Params filterParams;
				filterParams.bodyIndexTexture = m_bodyIndexTexture;
				filterParams.colorToDepthTexture = depthMappingTexture;

				std::uint32_t width = gs_texture_get_width(sourceTexture);
				std::uint32_t height = gs_texture_get_height(sourceTexture);

				filterTexture = m_bodyIndexFilterEffect->Filter(width, height, filterParams);
			}

			if (m_greenScreenSettings.blurPassCount > 0)
				filterTexture = m_gaussianBlur->Blur(filterTexture, m_greenScreenSettings.blurPassCount);

			gs_texture_t* filteredTexture = m_alphaMaskFilter->Filter(sourceTexture, filterTexture);

			m_finalTexture.reset(filteredTexture);
		}
		else
			m_finalTexture.reset(sourceTexture);
	}
	catch (const std::exception& e)
	{
		warn("an error occurred: %s", e.what());
	}
}

/*
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
*/
auto KinectSource::RetrieveDepthMappingFrame(const KinectDevice::ColorFrameData& colorFrame, const KinectDevice::DepthFrameData& depthFrame) -> DepthMappingFrameData
{
	static_assert(sizeof(DepthSpacePoint) == 2 * sizeof(float));

	DepthMappingFrameData outputFrameData;
	outputFrameData.width = colorFrame.width;
	outputFrameData.height = colorFrame.height;
	outputFrameData.pitch = colorFrame.pitch;

	if (outputFrameData.width != depthFrame.width || outputFrameData.height != depthFrame.height)
	{
		std::size_t colorPixelCount = outputFrameData.width * outputFrameData.height;

		const std::uint16_t* depthPtr = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
		std::size_t depthPixelCount = depthFrame.width * depthFrame.height;

		outputFrameData.memory.resize(colorPixelCount * sizeof(KinectDevice::DepthCoordinates));

		KinectDevice::DepthCoordinates* coordinatePtr = reinterpret_cast<KinectDevice::DepthCoordinates*>(outputFrameData.memory.data());

		if (!m_device.MapColorToDepth(depthPtr, depthPixelCount, colorPixelCount, coordinatePtr))
			throw std::runtime_error("failed to map color to depth");

		outputFrameData.ptr.reset(reinterpret_cast<std::uint8_t*>(coordinatePtr));
	}
	else
	{
		// Don't map depth to depth/infrared
	}

	return outputFrameData;
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
