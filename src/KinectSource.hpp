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
#include <obs-module.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Kinect.h>
#include <windows.h>

class KinectSource
{
	public:
		enum class SourceType;
		struct DepthToColorSettings;
		struct InfraredToColorSettings;

		KinectSource(obs_source_t* source);
		~KinectSource();

		void OnVisibilityUpdate(bool isVisible);

		void SetSourceType(SourceType sourceType);

		void ShouldStopOnHide(bool shouldStop);

		void UpdateDepthToColor(DepthToColorSettings depthToColor);
		void UpdateInfraredToColor(InfraredToColorSettings infraredToColor);

		enum class SourceType
		{
			Color,
			Depth,
			Infrared
		};

		struct DepthToColorSettings
		{
			bool dynamic = false;
			float averageValue = 0.015f;
			float standardDeviation = 3.f;
		};

		struct InfraredToColorSettings
		{
			bool dynamic = false;
			float averageValue = 0.08f;
			float standardDeviation = 3.f;
		};

	private:
		struct FrameData
		{
			uint32_t width;
			uint32_t height;
			uint32_t pitch;
			ObserverPtr<std::uint8_t[]> ptr;
			std::vector<std::uint8_t> fallbackMemory; //< Used only when memory consumption increases
		};

		struct ColorFrameData : FrameData
		{
			video_format format;

			ReleasePtr<IColorFrame> colorFrame;
		};

		struct DepthFrameData : FrameData
		{
			ReleasePtr<IDepthFrame> depthFrame;
		};

		struct InfraredFrameData : FrameData
		{
			ReleasePtr<IInfraredFrame> infraredFrame;
		};

		struct DynamicValues
		{
			double average;
			double standardDeviation;
		};

		std::uint8_t* AllocateMemory(std::vector<std::uint8_t>& fallback, std::size_t size);

		ColorFrameData ConvertDepthToColor(const DepthToColorSettings& settings, const DepthFrameData& infraredFrame);
		ColorFrameData ConvertInfraredToColor(const InfraredToColorSettings& settings, const InfraredFrameData& infraredFrame);

		std::optional<ColorFrameData> RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame, bool forceRGBA = false);
		std::optional<DepthFrameData> RetrieveDepthFrame(IMultiSourceFrame* multiSourceFrame);
		std::optional<InfraredFrameData> RetrieveInfraredFrame(IMultiSourceFrame* multiSourceFrame);

		void Start();
		void Stop();
		void ThreadFunc(std::condition_variable& cv, std::mutex& m);

		static DynamicValues ComputeDynamicValues(const std::uint16_t* values, std::size_t valueCount);

		std::atomic_bool m_running;
		std::atomic<DepthToColorSettings> m_depthToColorSettings;
		std::atomic<InfraredToColorSettings> m_infraredToColorSettings;
		std::atomic<SourceType> m_sourceType;
		std::size_t m_requiredMemory;
		std::thread m_thread;
		std::vector<std::uint8_t> m_memory;
		obs_source_t* m_source;
		bool m_stopOnHide;
};

#endif
