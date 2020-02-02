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

#include <obs-module.h>
#include <atomic>
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
		KinectSource(obs_source_t* source);
		~KinectSource();

	private:
		struct ColorFrameData
		{
			video_format format;
			uint32_t width;
			uint32_t height;
			uint32_t pitch;
			uint8_t* ptr;
		};

		std::optional<ColorFrameData> RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame, std::vector<uint8_t>& memory, bool forceRGBA = false);
		void ThreadFunc();

		std::atomic_bool m_running;
		std::thread m_thread;
		obs_source_t* m_source;
};

#endif
