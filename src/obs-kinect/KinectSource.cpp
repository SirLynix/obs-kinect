/******************************************************************************
	Copyright (C) 2021 by Jérôme Leclercq <lynix680@gmail.com>

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

#include <obs-kinect/KinectSource.hpp>
#include <obs-kinect-core/KinectDevice.hpp>
#include <obs-kinect/KinectDeviceRegistry.hpp>
#include <util/platform.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>
#include <optional>

KinectSource::KinectSource(std::shared_ptr<KinectDeviceRegistry> registry, const obs_source_t* source) :
m_filterBlur(GS_RGBA),
m_registry(std::move(registry)),
m_sourceType(SourceType::Color),
m_source(source),
m_height(0),
m_width(0),
m_lastFrameIndex(KinectDevice::InvalidFrameIndex),
m_lastTextureTick(0),
m_isVisible(false),
m_stopOnHide(false)
{
	m_registry->RegisterSource(this);
}

KinectSource::~KinectSource()
{
	m_registry->UnregisterSource(this);
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
	if (!m_stopOnHide)
		isVisible = true;

	if (m_isVisible != isVisible)
	{
		m_isVisible = isVisible;
		RefreshDeviceAccess();

		if (!m_isVisible)
			m_finalTexture.reset(); //< Free some memory
	}
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

	m_greenScreenSettings = std::move(greenScreen);

	// If green screen effect config isn't linked to the current effect, update it
	std::visit([&](auto&& effect)
	{
		using C = std::decay_t<decltype(effect)>;
		using E = typename C::Effect;

		if (!std::holds_alternative<E>(m_greenscreenEffect))
			m_greenscreenEffect.emplace<E>();

	}, m_greenScreenSettings.effectConfig);

	if (m_deviceAccess)
		m_deviceAccess->SetEnabledSourceFlags(ComputeEnabledSourceFlags());
}

void KinectSource::UpdateInfraredToColor(InfraredToColorSettings infraredToColor)
{
	m_infraredToColorSettings = infraredToColor;
}

void KinectSource::UpdateVisibilityMaskFile(const std::string_view& filePath)
{
	if (m_visibilityMaskPath != filePath)
	{
		if (!filePath.empty())
		{
			if (!m_visibilityMaskImage)
				m_visibilityMaskImage.reset(new gs_image_file_t);

			gs_image_file_init(m_visibilityMaskImage.get(), filePath.data());

			ObsGraphics gfx;
			gs_image_file_init_texture(m_visibilityMaskImage.get());
		}
		else
			m_visibilityMaskImage.reset();

		m_visibilityMaskPath = filePath;
	}
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

		// Update animated textures, if any
		std::uint64_t now = obs_get_video_frame_time();
		if (m_lastTextureTick == 0)
			m_lastTextureTick = now;

		if (m_visibilityMaskImage && m_visibilityMaskImage->texture && gs_image_file_tick(m_visibilityMaskImage.get(), now - m_lastTextureTick))
		{
			ObsGraphics gfx;
			gs_image_file_update_texture(m_visibilityMaskImage.get());
		}

		// Process frame
		m_height = 0;
		m_width = 0;

		m_lastFrameIndex = frameData->frameIndex;

		ObsGraphics obsGfx;

		bool isDepthColorMapped = frameData->colorMappedDepthFrame.has_value();
		bool softwareDepthMapping = (!m_greenScreenSettings.gpuDepthMapping || m_greenScreenSettings.maxDirtyDepth > 0);

		if ((m_greenScreenSettings.enabled && DoesRequireDepthFrame(m_greenScreenSettings.filterType) && !softwareDepthMapping && !isDepthColorMapped) || m_sourceType == SourceType::Depth)
		{
			if (!frameData->depthFrame)
				return;

			const DepthFrameData& depthFrame = frameData->depthFrame.value();
			UpdateTexture(m_depthTexture, GS_R16, depthFrame.width, depthFrame.height, depthFrame.pitch, depthFrame.ptr.get());
		}

		// Fetch/compute color texture
		gs_texture_t* sourceTexture = nullptr;
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

		if (!sourceTexture)
			return;

		m_width = gs_texture_get_width(sourceTexture);
		m_height = gs_texture_get_height(sourceTexture);

		// Apply greenscreen effected if enabled
		if (m_greenScreenSettings.enabled)
		{
			// All green screen types (except depth/dedicated) require body index texture
			if (!softwareDepthMapping && DoesRequireBodyFrame(m_greenScreenSettings.filterType))
			{
				if (!frameData->bodyIndexFrame)
					return;

				const BodyIndexFrameData& bodyIndexFrame = *frameData->bodyIndexFrame;
				UpdateTexture(m_bodyIndexTexture, GS_R8, bodyIndexFrame.width, bodyIndexFrame.height, bodyIndexFrame.pitch, bodyIndexFrame.ptr.get());
			}

			// Handle CPU|GPU depth mapping + dirty depth values
			gs_texture_t* bodyIndexTexture = m_bodyIndexTexture.get();
			gs_texture_t* depthMappingTexture = nullptr;
			gs_texture_t* depthTexture = m_depthTexture.get();

			if (m_sourceType == SourceType::Color)
			{
				if (!frameData->depthMappingFrame && !frameData->colorMappedDepthFrame)
					return;

				if (frameData->colorMappedDepthFrame)
				{
					const DepthFrameData& mappedDepthFrame = *frameData->colorMappedDepthFrame;

					UpdateTexture(m_depthTexture, GS_R16, mappedDepthFrame.width, mappedDepthFrame.height, mappedDepthFrame.width * sizeof(std::uint16_t), mappedDepthFrame.ptr.get());
					depthMappingTexture = nullptr;
					depthTexture = m_depthTexture.get();
				}
				else
				{
					const DepthMappingFrameData& depthMappingFrame = *frameData->depthMappingFrame;

					if (softwareDepthMapping)
					{
						if (!frameData->colorFrame || !frameData->depthFrame)
							return;

						const ColorFrameData& colorFrame = *frameData->colorFrame;
						const DepthFrameData& depthFrame = *frameData->depthFrame;

						constexpr float InvalidDepth = -std::numeric_limits<float>::infinity();

						const DepthMappingFrameData::DepthCoordinates* depthMapping = reinterpret_cast<const DepthMappingFrameData::DepthCoordinates*>(depthMappingFrame.ptr.get());

						constexpr std::uint16_t InvalidDepthOutput = 0;

						m_depthMappingMemory.resize(colorFrame.width * colorFrame.height * sizeof(std::uint16_t), InvalidDepthOutput);
						m_depthMappingDirtyCounter.resize(colorFrame.width * colorFrame.height, 0);
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
										*output = InvalidDepthOutput;

									continue;
								}

								int dX = static_cast<int>(depthCoordinates.x + 0.5f);
								int dY = static_cast<int>(depthCoordinates.y + 0.5f);

								if (dX < 0 || dX >= int(depthFrame.width) ||
									dY < 0 || dY >= int(depthFrame.height))
								{
									if (++dirtyCounter > m_greenScreenSettings.maxDirtyDepth)
										*output = InvalidDepthOutput;

									continue;
								}

								*output = depthFrame.ptr[depthFrame.width * dY + dX];
								dirtyCounter = 0;
							}
						}

						UpdateTexture(m_depthMappingTexture, GS_R16, colorFrame.width, colorFrame.height, colorFrame.width * sizeof(std::uint16_t), m_depthMappingMemory.data());
						depthMappingTexture = nullptr;
						depthTexture = m_depthMappingTexture.get();

						if (DoesRequireBodyFrame(m_greenScreenSettings.filterType))
						{
							if (!frameData->bodyIndexFrame)
								return;

							// Map body info as well
							const BodyIndexFrameData& bodyIndexFrame = *frameData->bodyIndexFrame;

							const std::uint8_t* bodyPixels = reinterpret_cast<const std::uint8_t*>(bodyIndexFrame.ptr.get());

							constexpr std::uint8_t InvalidBodyIndexOutput = 255;

							m_bodyMappingMemory.resize(colorFrame.width * colorFrame.height * sizeof(std::uint8_t), InvalidBodyIndexOutput);
							m_bodyMappingDirtyCounter.resize(colorFrame.width * colorFrame.height, 0);
							std::uint8_t* bodyIndexOutput = m_bodyMappingMemory.data();

							for (std::size_t y = 0; y < colorFrame.height; ++y)
							{
								for (std::size_t x = 0; x < colorFrame.width; ++x)
								{
									std::uint8_t& dirtyCounter = m_bodyMappingDirtyCounter[y * colorFrame.width + x];
									std::uint8_t* output = &bodyIndexOutput[y * colorFrame.width + x];
									const auto& depthCoordinates = depthMapping[y * depthMappingFrame.width + x];
									if (depthCoordinates.x == InvalidDepth || depthCoordinates.y == InvalidDepth)
									{
										if (++dirtyCounter > m_greenScreenSettings.maxDirtyDepth)
											*output = InvalidBodyIndexOutput;

										continue;
									}

									int dX = static_cast<int>(depthCoordinates.x + 0.5f);
									int dY = static_cast<int>(depthCoordinates.y + 0.5f);

									if (dX < 0 || dX >= int(depthFrame.width) ||
										dY < 0 || dY >= int(depthFrame.height))
									{
										if (++dirtyCounter > m_greenScreenSettings.maxDirtyDepth)
											*output = InvalidBodyIndexOutput;

										continue;
									}

									*output = bodyPixels[depthFrame.width * dY + dX];
									dirtyCounter = 0;
								}
							}

							UpdateTexture(m_bodyIndexTexture, GS_R8, colorFrame.width, colorFrame.height, colorFrame.width * sizeof(std::uint8_t), m_bodyMappingMemory.data());
							bodyIndexTexture = m_bodyIndexTexture.get();
						}
						else
						{
							// Reclaim some memory
							m_bodyMappingMemory.clear();
							m_bodyMappingMemory.shrink_to_fit();

							m_bodyMappingDirtyCounter.clear();
							m_bodyMappingDirtyCounter.shrink_to_fit();
						}
					}
					else
					{
						// Reclaim some memory
						m_bodyMappingMemory.clear();
						m_bodyMappingMemory.shrink_to_fit();

						m_bodyMappingDirtyCounter.clear();
						m_bodyMappingDirtyCounter.shrink_to_fit();

						m_depthMappingMemory.clear();
						m_depthMappingMemory.shrink_to_fit();

						m_depthMappingDirtyCounter.clear();
						m_depthMappingDirtyCounter.shrink_to_fit();

						UpdateTexture(m_depthMappingTexture, GS_RG32F, depthMappingFrame.width, depthMappingFrame.height, depthMappingFrame.pitch, depthMappingFrame.ptr.get());
						depthMappingTexture = m_depthMappingTexture.get();
					}
				}
			}

			// Apply green screen filtering
			gs_texture_t* filterTexture = nullptr;
			if (m_greenScreenSettings.filterType == GreenScreenFilterType::Dedicated)
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

				switch (m_greenScreenSettings.filterType)
				{
					case GreenScreenFilterType::Body:
					{
						GreenScreenFilterShader::BodyFilterParams filterParams;
						filterParams.bodyIndexTexture = bodyIndexTexture;
						filterParams.colorToDepthTexture = depthMappingTexture;

						filterTexture = m_greenScreenFilterEffect.Filter(m_width, m_height, filterParams);
						break;
					}

					case GreenScreenFilterType::BodyOrDepth:
					{
						GreenScreenFilterShader::BodyOrDepthFilterParams filterParams;
						filterParams.bodyIndexTexture = bodyIndexTexture;
						filterParams.colorToDepthTexture = depthMappingTexture;
						filterParams.depthTexture = depthTexture;
						filterParams.maxDepth = m_greenScreenSettings.depthMax;
						filterParams.minDepth = m_greenScreenSettings.depthMin;
						filterParams.progressiveDepth = m_greenScreenSettings.fadeDist;

						filterTexture = m_greenScreenFilterEffect.Filter(m_width, m_height, filterParams);
						break;
					}

					case GreenScreenFilterType::BodyWithinDepth:
					{
						GreenScreenFilterShader::BodyWithinDepthFilterParams filterParams;
						filterParams.bodyIndexTexture = bodyIndexTexture;
						filterParams.colorToDepthTexture = depthMappingTexture;
						filterParams.depthTexture = depthTexture;
						filterParams.maxDepth = m_greenScreenSettings.depthMax;
						filterParams.minDepth = m_greenScreenSettings.depthMin;
						filterParams.progressiveDepth = m_greenScreenSettings.fadeDist;

						filterTexture = m_greenScreenFilterEffect.Filter(m_width, m_height, filterParams);
						break;
					}

					case GreenScreenFilterType::Depth:
					{
						GreenScreenFilterShader::DepthFilterParams filterParams;
						filterParams.colorToDepthTexture = depthMappingTexture;
						filterParams.depthTexture = depthTexture;
						filterParams.maxDepth = m_greenScreenSettings.depthMax;
						filterParams.minDepth = m_greenScreenSettings.depthMin;
						filterParams.progressiveDepth = m_greenScreenSettings.fadeDist;

						filterTexture = m_greenScreenFilterEffect.Filter(m_width, m_height, filterParams);
						break;
					}

					case GreenScreenFilterType::Dedicated:
						break; //< Already handled in a branch
				}

				if (!filterTexture)
					return;

				if (m_greenScreenSettings.blurPassCount > 0)
					filterTexture = m_filterBlur.Blur(filterTexture, m_greenScreenSettings.blurPassCount);

				if (m_visibilityMaskImage && m_visibilityMaskImage->texture)
					filterTexture = m_visibilityMaskEffect.Mask(filterTexture, m_visibilityMaskImage->texture);
			}

			// Present processed texture
			m_finalTexture.reset(std::visit([&](auto&& effect) -> gs_texture_t*
			{
				using E = std::decay_t<decltype(effect)>;
				using C = typename E::Config;

				return effect.Apply(std::get<C>(m_greenScreenSettings.effectConfig), sourceTexture, filterTexture);
			}, m_greenscreenEffect));
		}
		else
			m_finalTexture.reset(sourceTexture);
	}
	catch (const std::exception& e)
	{
		warnlog("an error occurred: %s", e.what());
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

bool KinectSource::DoesRequireBodyFrame(GreenScreenFilterType greenscreenType)
{
	switch (greenscreenType)
	{
		case GreenScreenFilterType::Body:
		case GreenScreenFilterType::BodyOrDepth:
		case GreenScreenFilterType::BodyWithinDepth:
			return true;

		case GreenScreenFilterType::Dedicated:
		case GreenScreenFilterType::Depth:
			return false;
	}

	return false;
}

bool KinectSource::DoesRequireDepthFrame(GreenScreenFilterType greenscreenType)
{
	switch (greenscreenType)
	{
		case GreenScreenFilterType::BodyOrDepth:
		case GreenScreenFilterType::BodyWithinDepth:
		case GreenScreenFilterType::Depth:
			return true;

		case GreenScreenFilterType::Body:
		case GreenScreenFilterType::Dedicated:
			return false;
	}

	return false;
}

void KinectSource::ClearDeviceAccess()
{
	m_deviceAccess.reset();
}

SourceFlags KinectSource::ComputeEnabledSourceFlags() const
{
	return ComputeEnabledSourceFlags(m_deviceAccess->GetDevice());
}

SourceFlags KinectSource::ComputeEnabledSourceFlags(const KinectDevice& device) const
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
		// If device supports depth to color mapping, use it for color source
		bool colorMapped = (m_sourceType == SourceType::Color);
		bool hasDepthToColorMapping = (device.GetSupportedSources() & Source_ColorToDepthMapping);

		if (DoesRequireBodyFrame(m_greenScreenSettings.filterType))
		{
			if (colorMapped)
			{
				if (hasDepthToColorMapping)
					flags |= Source_Body | Source_ColorToDepthMapping;
				else
					flags |= Source_ColorMappedBody;
			}
			else
				flags |= Source_Body;
		}

		if (DoesRequireDepthFrame(m_greenScreenSettings.filterType))
		{
			if (colorMapped)
			{
				if (hasDepthToColorMapping)
					flags |= Source_Depth | Source_ColorToDepthMapping;
				else
					flags |= Source_ColorMappedDepth;
			}
			else
				flags |= Source_Depth;
		}

		if (m_greenScreenSettings.filterType == GreenScreenFilterType::Dedicated)
			flags |= Source_BackgroundRemoval;
	}

	return flags;
}

std::optional<KinectDeviceAccess> KinectSource::OpenAccess(KinectDevice& device)
{
	obs_data_t* settings = obs_source_get_settings(m_source);
	auto ReleaseSettings = [](obs_data_t* settings) { obs_data_release(settings); };
	std::unique_ptr<obs_data_t, decltype(ReleaseSettings)> unlockRect(settings, ReleaseSettings);

	try
	{
		KinectDeviceAccess deviceAccess = device.AcquireAccess(ComputeEnabledSourceFlags(device));
		deviceAccess.UpdateDeviceParameters(settings);

		return std::make_optional(std::move(deviceAccess));
	}
	catch (const std::exception& e)
	{
		warnlog("failed to access kinect device: %s", e.what());
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
		KinectDevice* device = m_registry->GetDevice(m_deviceName);
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
