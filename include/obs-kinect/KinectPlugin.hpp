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

#ifndef OBS_KINECT_PLUGIN_KINECTPLUGIN
#define OBS_KINECT_PLUGIN_KINECTPLUGIN

#include "Helper.hpp"
#include "KinectPluginImpl.hpp"
#include <util/platform.h>
#include <string>
#include <vector>

class KinectDevice;

class KinectPlugin
{
	public:
		KinectPlugin() = default;
		KinectPlugin(const KinectPlugin&) = delete;
		KinectPlugin(KinectPlugin&&) noexcept = default;
		~KinectPlugin();

		void Close();

		const std::string& GetUniqueName() const;

		bool IsOpen() const;

		bool Open(const char* path);

		std::vector<std::unique_ptr<KinectDevice>> Refresh() const;

		KinectPlugin& operator=(const KinectPlugin&) = delete;
		KinectPlugin& operator=(KinectPlugin&&) noexcept = default;

	private:
		std::unique_ptr<KinectPluginImpl> m_impl;
		std::string m_uniqueName;
		ObsLibPtr m_lib;
};

#endif
