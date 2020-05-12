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

#include "Enums.hpp"
#include "Helper.hpp"
#include "AlphaMaskEffect.hpp"
#include "ConvertDepthIRToColorEffect.hpp"
#include "GaussianBlurEffect.hpp"
#include "GreenScreenFilterEffect.hpp"
#include "KinectDeviceAccess.hpp"
#include <obs-module.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

class KinectDevice;
class KinectDeviceRegistry;

class KinectSource
{
	friend KinectDeviceRegistry;

	public:
		enum class GreenScreenType;
		enum class SourceType;
		struct DepthToColorSettings;
		struct GreenScreenSettings;
		struct InfraredToColorSettings;

		KinectSource(KinectDeviceRegistry& registry);
		~KinectSource();

		std::uint32_t GetHeight() const;
		std::uint32_t GetWidth() const;

		void OnVisibilityUpdate(bool isVisible);

		void Render();

		void SetSourceType(SourceType sourceType);

		void ShouldStopOnHide(bool shouldStop);

		void Update(float seconds);
		void UpdateDevice(std::string deviceName);
		void UpdateDeviceParameters(obs_data_t* settings);
		void UpdateDepthToColor(DepthToColorSettings depthToColor);
		void UpdateGreenScreen(GreenScreenSettings greenScreen);
		void UpdateInfraredToColor(InfraredToColorSettings infraredToColor);

		enum class GreenScreenType
		{
			Body = 0,            //< Requires Source_Body (| Source_ColorToDepthMapping if color source is used)
			BodyOrDepth = 2,     //< Requires Source_Body | Source_Depth (| Source_ColorToDepthMapping if color source is used)
			BodyWithinDepth = 3, //< Requires Source_Body | Source_Depth (| Source_ColorToDepthMapping if color source is used)
			Dedicated = 4,       //< Requires Source_BackgroundRemoval
			Depth = 1            //< Requires Source_Depth (| Source_ColorToDepthMapping if color source is used)
		};

		enum class SourceType
		{
			Color = 0,   //< Requires Source_Color
			Depth = 1,   //< Requires Source_Depth
			Infrared = 2 //< Requires Source_Infrared
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
			bool gpuDepthMapping = true;
			std::size_t blurPassCount = 3;
			std::uint16_t depthMax = 1200;
			std::uint16_t depthMin = 1;
			std::uint16_t fadeDist = 100;
			std::uint8_t maxDirtyDepth = 0;
		};

		struct InfraredToColorSettings
		{
			bool dynamic = false;
			float averageValue = 0.08f;
			float standardDeviation = 3.f;
		};

	private:
		struct DynamicValues
		{
			double average;
			double standardDeviation;
		};

		void ClearDeviceAccess();
		SourceFlags ComputeEnabledSourceFlags() const;
		std::optional<KinectDeviceAccess> OpenAccess(KinectDevice& device);
		void RefreshDeviceAccess();

		static DynamicValues ComputeDynamicValues(const std::uint16_t* values, std::size_t valueCount);

		std::optional<KinectDeviceAccess> m_deviceAccess;
		std::vector<std::uint8_t> m_depthMappingMemory;
		std::vector<std::uint8_t> m_depthMappingDirtyCounter;
		AlphaMaskEffect m_alphaMaskFilter;
		ConvertDepthIRToColorEffect m_depthIRConvertEffect;
		GaussianBlurEffect m_gaussianBlur;
		GreenScreenFilterEffect m_greenScreenFilterEffect;
		ObserverPtr<gs_texture_t> m_finalTexture;
		DepthToColorSettings m_depthToColorSettings;
		GreenScreenSettings m_greenScreenSettings;
		InfraredToColorSettings m_infraredToColorSettings;
		KinectDeviceRegistry& m_registry;
		ObsTexturePtr m_backgroundRemovalTexture;
		ObsTexturePtr m_bodyIndexTexture;
		ObsTexturePtr m_colorTexture;
		ObsTexturePtr m_depthMappingTexture;
		ObsTexturePtr m_depthTexture;
		ObsTexturePtr m_infraredTexture;
		SourceType m_sourceType;
		std::string m_deviceName;
		std::uint32_t m_height;
		std::uint32_t m_width;
		std::uint64_t m_lastFrameIndex;
		bool m_isVisible;
		bool m_stopOnHide;
};

#endif
