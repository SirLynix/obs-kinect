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

		void OnVisibilityUpdate(bool isVisible);

		void Render();

		void SetServicePriority(ProcessPriority servicePriority);
		void SetSourceType(SourceType sourceType);

		void ShouldStopOnHide(bool shouldStop);

		void Update(float seconds);
		void UpdateDevice(std::string deviceName);
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
		EnabledSourceFlags ComputeEnabledSourceFlags() const;
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
		ObsTexturePtr m_bodyIndexTexture;
		ObsTexturePtr m_colorTexture;
		ObsTexturePtr m_depthMappingTexture;
		ObsTexturePtr m_depthTexture;
		ObsTexturePtr m_infraredTexture;
		ProcessPriority m_servicePriority;
		SourceType m_sourceType;
		std::string m_deviceName;
		bool m_isVisible;
		bool m_stopOnHide;
};

#endif
