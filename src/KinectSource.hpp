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

#pragma once

#ifndef OBS_KINECT_PLUGIN_KINECTSOURCE
#define OBS_KINECT_PLUGIN_KINECTSOURCE

#include "Helper.hpp"
#include "AlphaMaskEffect.hpp"
#include "BodyIndexFilterEffect.hpp"
#include "ConvertDepthIRToColorEffect.hpp"
#include "DepthFilterEffect.hpp"
#include "GaussianBlurEffect.hpp"
#include "KinectDevice.hpp"
#include <obs-module.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

class KinectSource
{
	public:
		enum class GreenScreenType;
		enum class SourceType;
		struct DepthToColorSettings;
		struct GreenScreenSettings;
		struct InfraredToColorSettings;

		KinectSource(obs_source_t* source);
		~KinectSource();

		void OnVisibilityUpdate(bool isVisible);

		void Render();

		void SetSourceType(SourceType sourceType);

		void ShouldStopOnHide(bool shouldStop);

		void Update(float seconds);
		void UpdateDepthToColor(DepthToColorSettings depthToColor);
		void UpdateGreenScreen(GreenScreenSettings greenScreen);
		void UpdateInfraredToColor(InfraredToColorSettings infraredToColor);

		enum class GreenScreenType
		{
			Body,
			Depth
		};

		enum class SourceType
		{
			Color = 0,
			Depth = 1,
			Infrared = 2
		};

		struct DepthToColorSettings
		{
			bool dynamic = false;
			float averageValue = 0.015f;
			float standardDeviation = 3.f;
		};

		struct GreenScreenSettings
		{
			GreenScreenType type = GreenScreenType::Depth;
			bool enabled = true;
			std::size_t blurPassCount = 3;
			std::uint16_t depthMax = 1200;
			std::uint16_t depthMin = 1;
			std::uint16_t fadeDist = 100;
		};

		struct InfraredToColorSettings
		{
			bool dynamic = false;
			float averageValue = 0.08f;
			float standardDeviation = 3.f;
		};

	private:
		struct DepthMappingFrameData
		{
			std::uint32_t width;
			std::uint32_t height;
			std::uint32_t pitch;
			ObserverPtr<std::uint8_t[]> ptr;
			std::vector<std::uint8_t> memory; //< TODO: Reuse memory
		};

		struct DynamicValues
		{
			double average;
			double standardDeviation;
		};

		DepthMappingFrameData RetrieveDepthMappingFrame(const KinectDevice::ColorFrameData& colorFrame, const KinectDevice::DepthFrameData& depthFrame);

		static DynamicValues ComputeDynamicValues(const std::uint16_t* values, std::size_t valueCount);

		std::optional<AlphaMaskEffect> m_alphaMaskFilter;
		std::optional<BodyIndexFilterEffect> m_bodyIndexFilterEffect;
		std::optional<ConvertDepthIRToColorEffect> m_depthIRConvertEffect;
		std::optional<DepthFilterEffect> m_depthFilter;
		std::optional<GaussianBlurEffect> m_gaussianBlur;
		gs_texture_t* m_bodyIndexTexture;
		gs_texture_t* m_colorTexture;
		gs_texture_t* m_depthMappingTexture;
		gs_texture_t* m_depthTexture;
		gs_texture_t* m_infraredTexture;
		ObserverPtr<gs_texture_t> m_finalTexture;
		DepthToColorSettings m_depthToColorSettings;
		GreenScreenSettings m_greenScreenSettings;
		InfraredToColorSettings m_infraredToColorSettings;
		SourceType m_sourceType;
		obs_source_t* m_source;
		KinectDevice m_device;
		bool m_stopOnHide;
};

#endif
