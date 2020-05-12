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
#include "KinectDevice.hpp"
#include "KinectDeviceRegistry.hpp"
#include <util/platform.h>
#include <algorithm>
#include <array>
#include <numeric>
#include <optional>

KinectSource::KinectSource(KinectDeviceRegistry& registry) :
m_gaussianBlur(GS_RGBA),
m_registry(registry),
m_sourceType(SourceType::Color),
m_height(0),
m_width(0),
m_lastFrameIndex(KinectDevice::InvalidFrameIndex),
m_isVisible(false),
m_stopOnHide(false)
{
	m_registry.RegisterSource(this);
}

KinectSource::~KinectSource()
{
	m_registry.UnregisterSource(this);
}

std::uint32_t KinectSource::GetHeight() const
{
	return m_height;
}

std::uint32_t KinectSource::GetWidth() const
{
	return m_width;
}

void KinectSource::OnVisibilityUpdate(bool isVisible)
{
	m_isVisible = isVisible || !m_stopOnHide;
	RefreshDeviceAccess();

	if (!m_isVisible)
		m_finalTexture.reset(); //< Free some memory
}

void KinectSource::SetSourceType(SourceType sourceType)
{
	if (m_sourceType != sourceType)
	{
		m_sourceType = sourceType;
		m_finalTexture.reset();

		if (m_deviceAccess)
			m_deviceAccess->SetEnabledSourceFlags(ComputeEnabledSourceFlags());
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

	if (m_deviceAccess)
		m_deviceAccess->SetEnabledSourceFlags(ComputeEnabledSourceFlags());
}

void KinectSource::UpdateInfraredToColor(InfraredToColorSettings infraredToColor)
{
	m_infraredToColorSettings = infraredToColor;
}

void KinectSource::ShouldStopOnHide(bool shouldStop)
{
	m_stopOnHide = shouldStop;
	if (!m_stopOnHide && !m_deviceAccess)
		RefreshDeviceAccess();
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
	auto UpdateTexture = [](ObsTexturePtr& texture, gs_color_format format, std::uint32_t width, std::uint32_t height, std::uint32_t pitch, const void* content)
	{
		gs_texture_t* texPtr = texture.get();
		const std::uint8_t* contentInput = static_cast<const std::uint8_t*>(content);
		if (!texPtr || format != gs_texture_get_color_format(texPtr) || width != gs_texture_get_width(texPtr) || height != gs_texture_get_height(texPtr))
		{
			texture.reset(gs_texture_create(width, height, format, 1, &contentInput, GS_DYNAMIC));
			if (!texture)
				throw std::runtime_error("failed to create texture");
		}
		else
		{
			uint8_t* ptr;
			uint32_t texPitch;
			if (!gs_texture_map(texPtr, &ptr, &texPitch))
				throw std::runtime_error("failed to map texture");

			if (pitch == texPitch)
				std::memcpy(ptr, content, pitch * height);
			else
			{
				std::uint32_t bestPitch = std::min(pitch, texPitch);
				for (std::size_t y = 0; y < height; ++y)
				{
					const std::uint8_t* input = &contentInput[y * pitch];
					std::uint8_t* output = ptr + y * texPitch;

					std::memcpy(output, input, bestPitch);
				}
			}

			gs_texture_unmap(texPtr);
		}
	};

	if (!m_deviceAccess)
	{
		m_height = 0;
		m_width = 0;
		return;
	}

	try
	{
		auto frameData = m_deviceAccess->GetLastFrame();
		if (!frameData || frameData->frameIndex == m_lastFrameIndex)
			return;

		m_height = 0;
		m_width = 0;

		m_lastFrameIndex = frameData->frameIndex;

		ObsGraphics obsGfx;

		if ((m_greenScreenSettings.enabled && m_greenScreenSettings.gpuDepthMapping) || m_sourceType == SourceType::Depth)
		{
			if (!frameData->depthFrame)
				return;

			const DepthFrameData& depthFrame = frameData->depthFrame.value();
			UpdateTexture(m_depthTexture, GS_R16, depthFrame.width, depthFrame.height, depthFrame.pitch, depthFrame.ptr.get());
		}

		gs_texture_t* sourceTexture;
		switch (m_sourceType)
		{
			case SourceType::Color:
			{
				if (!frameData->colorFrame)
					return;

				const ColorFrameData& colorFrame = *frameData->colorFrame;

				UpdateTexture(m_colorTexture, colorFrame.format, colorFrame.width, colorFrame.height, colorFrame.pitch, colorFrame.ptr.get());
				sourceTexture = m_colorTexture.get();
				break;
			}

			case SourceType::Depth:
			{
				assert(frameData->depthFrame); //< Depth has already been checked/processed at this point
				const DepthFrameData& depthFrame = *frameData->depthFrame;

				float averageValue;
				float standardDeviation;
				if (m_depthToColorSettings.dynamic)
				{
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

				sourceTexture = m_depthIRConvertEffect.Convert(depthFrame.width, depthFrame.height, m_depthTexture.get(), averageValue, standardDeviation);
				break;
			}

			case SourceType::Infrared:
			{
				if (!frameData->infraredFrame)
					return;

				const InfraredFrameData& irFrame = *frameData->infraredFrame;

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
				sourceTexture = m_depthIRConvertEffect.Convert(irFrame.width, irFrame.height, m_infraredTexture.get(), averageValue, standardDeviation);
				break;
			}

			default:
				break;
		}

		m_width = gs_texture_get_width(sourceTexture);
		m_height = gs_texture_get_height(sourceTexture);

		if (m_greenScreenSettings.enabled)
		{
			// Handle CPU|GPU depth mapping + dirty depth values
			gs_texture_t* depthMappingTexture;
			gs_texture_t* depthValues;
			if (m_sourceType == SourceType::Color)
			{
				if (!frameData->depthMappingFrame)
					return;

				const DepthMappingFrameData& depthMappingFrame = *frameData->depthMappingFrame;

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
					if (!frameData->colorFrame || !frameData->depthFrame)
						return;

					const ColorFrameData& colorFrame = *frameData->colorFrame;
					const DepthFrameData& depthFrame = *frameData->depthFrame;

					constexpr float InvalidDepth = -std::numeric_limits<float>::infinity();

					const std::uint16_t* depthPixels = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
					const DepthMappingFrameData::DepthCoordinates* depthMapping = reinterpret_cast<const DepthMappingFrameData::DepthCoordinates*>(depthMappingFrame.ptr.get());

					m_depthMappingMemory.resize(colorFrame.width* colorFrame.height * sizeof(std::uint16_t));
					m_depthMappingDirtyCounter.resize(colorFrame.width * colorFrame.height);
					std::uint16_t* depthOutput = reinterpret_cast<std::uint16_t*>(m_depthMappingMemory.data());

					for (std::size_t y = 0; y < colorFrame.height; ++y)
					{
						for (std::size_t x = 0; x < colorFrame.width; ++x)
						{
							std::uint8_t& dirtyCounter = m_depthMappingDirtyCounter[y * colorFrame.width + x];
							std::uint16_t* output = &depthOutput[y * colorFrame.width + x];
							const auto& depthCoordinates = depthMapping[y * depthMappingFrame.width + x];
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

			// All green screen types (except depth/dedicated) require body index texture
			if (m_greenScreenSettings.type != GreenScreenType::Depth && m_greenScreenSettings.type != GreenScreenType::Dedicated)
			{
				if (!frameData->bodyIndexFrame)
					return;

				const BodyIndexFrameData& bodyIndexFrame = *frameData->bodyIndexFrame;
				UpdateTexture(m_bodyIndexTexture, GS_R8, bodyIndexFrame.width, bodyIndexFrame.height, bodyIndexFrame.pitch, bodyIndexFrame.ptr.get());
			}

			// Apply green screen filtering
			gs_texture_t* filterTexture = nullptr;
			if (m_greenScreenSettings.type == GreenScreenType::Dedicated)
			{
				if (!frameData->backgroundRemovalFrame)
					return;

				const BackgroundRemovalFrameData& backgroundRemovalFrame = *frameData->backgroundRemovalFrame;
				UpdateTexture(m_backgroundRemovalTexture, GS_R8, backgroundRemovalFrame.width, backgroundRemovalFrame.height, backgroundRemovalFrame.pitch, backgroundRemovalFrame.ptr.get());

				filterTexture = m_backgroundRemovalTexture.get();
			}
			else
			{
				m_backgroundRemovalTexture.reset(); //< Release some memory

				switch (m_greenScreenSettings.type)
				{
					case GreenScreenType::Body:
					{
						GreenScreenFilterEffect::BodyFilterParams filterParams;
						filterParams.bodyIndexTexture = m_bodyIndexTexture.get();
						filterParams.colorToDepthTexture = depthMappingTexture;

						filterTexture = m_greenScreenFilterEffect.Filter(m_width, m_height, filterParams);
						break;
					}

					case GreenScreenType::BodyOrDepth:
					{
						GreenScreenFilterEffect::BodyOrDepthFilterParams filterParams;
						filterParams.bodyIndexTexture = m_bodyIndexTexture.get();
						filterParams.colorToDepthTexture = depthMappingTexture;
						filterParams.depthTexture = depthValues;
						filterParams.maxDepth = m_greenScreenSettings.depthMax;
						filterParams.minDepth = m_greenScreenSettings.depthMin;
						filterParams.progressiveDepth = m_greenScreenSettings.fadeDist;

						filterTexture = m_greenScreenFilterEffect.Filter(m_width, m_height, filterParams);
						break;
					}

					case GreenScreenType::BodyWithinDepth:
					{
						GreenScreenFilterEffect::BodyWithinDepthFilterParams filterParams;
						filterParams.bodyIndexTexture = m_bodyIndexTexture.get();
						filterParams.colorToDepthTexture = depthMappingTexture;
						filterParams.depthTexture = depthValues;
						filterParams.maxDepth = m_greenScreenSettings.depthMax;
						filterParams.minDepth = m_greenScreenSettings.depthMin;
						filterParams.progressiveDepth = m_greenScreenSettings.fadeDist;

						filterTexture = m_greenScreenFilterEffect.Filter(m_width, m_height, filterParams);
						break;
					}

					case GreenScreenType::Depth:
					{
						GreenScreenFilterEffect::DepthFilterParams filterParams;
						filterParams.colorToDepthTexture = depthMappingTexture;
						filterParams.depthTexture = depthValues;
						filterParams.maxDepth = m_greenScreenSettings.depthMax;
						filterParams.minDepth = m_greenScreenSettings.depthMin;
						filterParams.progressiveDepth = m_greenScreenSettings.fadeDist;

						filterTexture = m_greenScreenFilterEffect.Filter(m_width, m_height, filterParams);
						break;
					}
				}

				if (!filterTexture)
					return;

				if (m_greenScreenSettings.blurPassCount > 0)
					filterTexture = m_gaussianBlur.Blur(filterTexture, m_greenScreenSettings.blurPassCount);
			}


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

void KinectSource::UpdateDevice(std::string deviceName)
{
	if (m_deviceName == deviceName)
		return;

	m_deviceName = std::move(deviceName);
	RefreshDeviceAccess();
}

void KinectSource::UpdateDeviceParameters(obs_data_t* settings)
{
	if (m_deviceAccess)
		m_deviceAccess->UpdateDeviceParameters(settings);
}

void KinectSource::ClearDeviceAccess()
{
	m_deviceAccess.reset();
}

SourceFlags KinectSource::ComputeEnabledSourceFlags() const
{
	SourceFlags flags = 0;
	switch (m_sourceType)
	{
		case SourceType::Color:
			flags |= Source_Color;
			break;

		case SourceType::Depth:
			flags |= Source_Depth;
			break;

		case SourceType::Infrared:
			flags |= Source_Infrared;
			break;
	}

	if (m_greenScreenSettings.enabled)
	{
		if (m_sourceType == SourceType::Color)
			flags |= Source_ColorToDepthMapping;

		switch (m_greenScreenSettings.type)
		{
			case GreenScreenType::Body:
			case GreenScreenType::BodyOrDepth:
			case GreenScreenType::BodyWithinDepth:
				flags |= Source_Body;
				[[fallthrough]];
			case GreenScreenType::Depth:
				flags |= Source_Depth;
				break;

			case GreenScreenType::Dedicated:
				flags |= Source_BackgroundRemoval;
				break;
		}
	}

	return flags;
}

std::optional<KinectDeviceAccess> KinectSource::OpenAccess(KinectDevice& device)
{
	try
	{
		KinectDeviceAccess deviceAccess = device.AcquireAccess(ComputeEnabledSourceFlags());

		return deviceAccess;
	}
	catch (const std::exception& e)
	{
		warn("failed to access kinect device: %s", e.what());
		return {};
	}
}

void KinectSource::RefreshDeviceAccess()
{
	auto Clear = [&] {
		m_deviceAccess.reset();
		m_finalTexture.reset();
		m_lastFrameIndex = KinectDevice::InvalidFrameIndex;
	};

	if (m_isVisible)
	{
		KinectDevice* device = m_registry.GetDevice(m_deviceName);
		if (device)
			m_deviceAccess = OpenAccess(*device);
		else
			Clear();
	}
	else
		Clear();
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
