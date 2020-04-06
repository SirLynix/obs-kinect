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
m_gaussianBlur(GS_RGBA),
m_sourceType(SourceType::Color),
m_source(source),
m_stopOnHide(false)
{
}

KinectSource::~KinectSource() = default;

void KinectSource::OnVisibilityUpdate(bool isVisible)
{
	if (isVisible)
	{
		try
		{
			m_device.StartCapture(); //< Does nothing if already capturing
		}
		catch (const std::exception& e)
		{
			warn("failed to start capture: %s", e.what());
		}
	}
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
	auto UpdateTexture = [](ObsTexturePtr& texture, gs_color_format format, std::uint32_t width, std::uint32_t height, std::uint32_t pitch, const std::uint8_t* content)
	{
		gs_texture_t* texPtr = texture.get();
		if (!texPtr || format != gs_texture_get_color_format(texPtr) || width != gs_texture_get_width(texPtr) || height != gs_texture_get_height(texPtr))
		{
			texture.reset(gs_texture_create(width, height, format, 1, &content, GS_DYNAMIC));
			if (!texture)
				throw std::runtime_error("failed to create texture");
		}
		else
		{
			uint8_t* ptr;
			uint32_t pitch;
			if (!gs_texture_map(texPtr, &ptr, &pitch))
				throw std::runtime_error("failed to map texture");

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

			gs_texture_unmap(texPtr);
		}
	};

	try
	{
		auto frameData = m_device.GetLastFrame();
		if (!frameData)
			return;

		ObsGraphics obsGfx;

		if ((m_greenScreenSettings.enabled && m_greenScreenSettings.gpuDepthMapping) || m_sourceType == SourceType::Depth)
		{
			KinectDevice::DepthFrameData& depthFrame = frameData->depthFrame.value();
			UpdateTexture(m_depthTexture, GS_R16, depthFrame.width, depthFrame.height, depthFrame.pitch, depthFrame.ptr.get());
		}

		gs_texture_t* sourceTexture;
		switch (m_sourceType)
		{
			case SourceType::Color:
			{
				if (!frameData->colorFrame.has_value())
					return;

				KinectDevice::ColorFrameData& colorFrame = frameData->colorFrame.value();

				UpdateTexture(m_colorTexture, colorFrame.format, colorFrame.width, colorFrame.height, colorFrame.pitch, colorFrame.ptr.get());
				sourceTexture = m_colorTexture.get();
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

				sourceTexture = m_depthIRConvertEffect.Convert(1920, 1080, m_depthTexture.get(), averageValue, standardDeviation);
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

				UpdateTexture(m_infraredTexture, GS_R16, irFrame.width, irFrame.height, irFrame.pitch, irFrame.ptr.get());
				sourceTexture = m_depthIRConvertEffect.Convert(1920, 1080, m_infraredTexture.get(), averageValue, standardDeviation);
				break;
			}

			default:
				break;
		}

		if (m_greenScreenSettings.enabled)
		{
			gs_texture_t* depthMappingTexture;
			gs_texture_t* depthValues;
			if (m_sourceType == SourceType::Color)
			{
				KinectDevice::ColorFrameData& colorFrame = frameData->colorFrame.value();
				KinectDevice::DepthFrameData& depthFrame = frameData->depthFrame.value();

				DepthMappingFrameData depthMappingFrame = RetrieveDepthMappingFrame(colorFrame, depthFrame);

				if (m_greenScreenSettings.gpuDepthMapping && m_greenScreenSettings.maxDirtyDepth == 0)
				{
					m_depthMappingMemory.clear();
					m_depthMappingMemory.shrink_to_fit();
					m_depthMappingDirtyCounter.clear();
					m_depthMappingDirtyCounter.shrink_to_fit();

					UpdateTexture(m_depthMappingTexture, GS_RG32F, depthMappingFrame.width, depthMappingFrame.height, depthMappingFrame.pitch, depthMappingFrame.ptr.get());
					depthMappingTexture = m_depthMappingTexture.get();
					depthValues = m_depthTexture.get();
				}
				else
				{
					constexpr float InvalidDepth = -std::numeric_limits<float>::infinity();

					const std::uint16_t* depthPixels = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
					const KinectDevice::DepthCoordinates* depthMapping = reinterpret_cast<const KinectDevice::DepthCoordinates*>(depthMappingFrame.ptr.get());

					m_depthMappingMemory.resize(colorFrame.width * colorFrame.height * sizeof(std::uint16_t));
					m_depthMappingDirtyCounter.resize(colorFrame.width * colorFrame.height);
					std::uint16_t* depthOutput = reinterpret_cast<std::uint16_t*>(m_depthMappingMemory.data());

					for (std::size_t y = 0; y < colorFrame.height; ++y)
					{
						for (std::size_t x = 0; x < colorFrame.width; ++x)
						{
							std::uint8_t& dirtyCounter = m_depthMappingDirtyCounter[y * colorFrame.width + x];
							std::uint16_t* output = &depthOutput[y * colorFrame.width + x];
							const KinectDevice::DepthCoordinates& depthCoordinates = depthMapping[y * depthMappingFrame.width + x];
							if (depthCoordinates.x == InvalidDepth || depthCoordinates.y == InvalidDepth)
							{
								if (++dirtyCounter > m_greenScreenSettings.maxDirtyDepth)
									*output = 0;

								continue;
							}

							int dX = static_cast<int>(depthCoordinates.x + 0.5f);
							int dY = static_cast<int>(depthCoordinates.y + 0.5f);

							if (dX < 0 || dX >= int(depthFrame.width) ||
							    dY < 0 || dY >= int(depthFrame.height))
							{
								if (++dirtyCounter > m_greenScreenSettings.maxDirtyDepth)
									*output = 0;

								continue;
							}

							*output = depthPixels[depthFrame.width * dY + dX];
							dirtyCounter = 0;
						}
					}

					UpdateTexture(m_depthMappingTexture, GS_R16, colorFrame.width, colorFrame.height, colorFrame.width * sizeof(std::uint16_t), m_depthMappingMemory.data());
					depthMappingTexture = nullptr;
					depthValues = m_depthMappingTexture.get();
				}
			}
			else
			{
				depthMappingTexture = nullptr;
				depthValues = m_depthTexture.get();
			}

			gs_texture_t* filterTexture;
			if (m_greenScreenSettings.type == GreenScreenType::Depth)
			{
				DepthFilterEffect::Params filterParams;
				filterParams.colorToDepthTexture = depthMappingTexture;
				filterParams.depthTexture = depthValues;
				filterParams.maxDepth = m_greenScreenSettings.depthMax;
				filterParams.minDepth = m_greenScreenSettings.depthMin;
				filterParams.progressiveDepth = m_greenScreenSettings.fadeDist;

				std::uint32_t width = gs_texture_get_width(sourceTexture);
				std::uint32_t height = gs_texture_get_height(sourceTexture);

				filterTexture = m_depthFilter.Filter(width, height, filterParams);
			}
			else
			{
				if (!frameData->bodyIndexFrame.has_value())
					return;

				KinectDevice::BodyIndexFrameData& bodyIndexFrame = frameData->bodyIndexFrame.value();
				UpdateTexture(m_bodyIndexTexture, GS_R8, bodyIndexFrame.width, bodyIndexFrame.height, bodyIndexFrame.pitch, bodyIndexFrame.ptr.get());

				BodyIndexFilterEffect::Params filterParams;
				filterParams.bodyIndexTexture = m_bodyIndexTexture.get();
				filterParams.colorToDepthTexture = depthMappingTexture;

				std::uint32_t width = gs_texture_get_width(sourceTexture);
				std::uint32_t height = gs_texture_get_height(sourceTexture);

				filterTexture = m_bodyIndexFilterEffect.Filter(width, height, filterParams);
			}

			if (m_greenScreenSettings.blurPassCount > 0)
				filterTexture = m_gaussianBlur.Blur(filterTexture, m_greenScreenSettings.blurPassCount);

			gs_texture_t* filteredTexture = m_alphaMaskFilter.Filter(sourceTexture, filterTexture);

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

auto KinectSource::RetrieveDepthMappingFrame(const KinectDevice::ColorFrameData& colorFrame, const KinectDevice::DepthFrameData& depthFrame) -> DepthMappingFrameData
{
	DepthMappingFrameData outputFrameData;
	outputFrameData.width = colorFrame.width;
	outputFrameData.height = colorFrame.height;
	outputFrameData.pitch = colorFrame.pitch;

	std::size_t colorPixelCount = outputFrameData.width * outputFrameData.height;

	const std::uint16_t* depthPtr = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
	std::size_t depthPixelCount = depthFrame.width * depthFrame.height;

	outputFrameData.memory.resize(colorPixelCount * sizeof(KinectDevice::DepthCoordinates));

	KinectDevice::DepthCoordinates* coordinatePtr = reinterpret_cast<KinectDevice::DepthCoordinates*>(outputFrameData.memory.data());

	if (!m_device.MapColorToDepth(depthPtr, depthPixelCount, colorPixelCount, coordinatePtr))
		throw std::runtime_error("failed to map color to depth");

	outputFrameData.ptr.reset(reinterpret_cast<std::uint8_t*>(coordinatePtr));

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
