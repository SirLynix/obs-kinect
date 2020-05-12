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

#ifndef OBS_KINECT_PLUGIN_KINECTPLUGINIMPL
#define OBS_KINECT_PLUGIN_KINECTPLUGINIMPL

#include "Helper.hpp"
#include <memory>
#include <string>
#include <vector>

class KinectDevice;

class OBSKINECT_API KinectPluginImpl
{
	public:
		KinectPluginImpl() = default;
		KinectPluginImpl(const KinectPluginImpl&) = delete;
		KinectPluginImpl(KinectPluginImpl&&) = delete;
		virtual ~KinectPluginImpl();

		virtual std::string GetUniqueName() const = 0;

		virtual std::vector<std::unique_ptr<KinectDevice>> Refresh() const = 0;

		KinectPluginImpl& operator=(const KinectPluginImpl&) = delete;
		KinectPluginImpl& operator=(KinectPluginImpl&&) = delete;
};

#endif
