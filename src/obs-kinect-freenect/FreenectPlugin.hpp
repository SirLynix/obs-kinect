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

#ifndef OBS_KINECT_PLUGIN_KINECTFREENECTPLUGIN
#define OBS_KINECT_PLUGIN_KINECTFREENECTPLUGIN

#include "FreenectHelper.hpp"
#include <obs-kinect-core/Enums.hpp>
#include <obs-kinect-core/KinectPluginImpl.hpp>
#include <libfreenect/libfreenect.h>

class KinectFreenectPlugin : public KinectPluginImpl
{
	public:
		KinectFreenectPlugin();
		KinectFreenectPlugin(const KinectFreenectPlugin&) = delete;
		KinectFreenectPlugin(KinectFreenectPlugin&&) = delete;
		~KinectFreenectPlugin();

		std::string GetUniqueName() const override;

		std::vector<std::unique_ptr<KinectDevice>> Refresh() const override;

		KinectFreenectPlugin& operator=(const KinectFreenectPlugin&) = delete;
		KinectFreenectPlugin& operator=(KinectFreenectPlugin&&) = delete;

	private:
		freenect_context* m_context;
};

#endif
