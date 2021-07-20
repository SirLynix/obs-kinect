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

#ifndef OBS_KINECT_PLUGIN_KINECTFREENECT2PLUGIN
#define OBS_KINECT_PLUGIN_KINECTFREENECT2PLUGIN

#include "Freenect2Helper.hpp"
#include <obs-kinect-core/Enums.hpp>
#include <obs-kinect-core/KinectPluginImpl.hpp>
#include <libfreenect2/libfreenect2.hpp>

class KinectFreenect2Plugin : public KinectPluginImpl
{
	public:
		KinectFreenect2Plugin() = default;
		KinectFreenect2Plugin(const KinectFreenect2Plugin&) = delete;
		KinectFreenect2Plugin(KinectFreenect2Plugin&&) = delete;
		~KinectFreenect2Plugin() = default;

		std::string GetUniqueName() const override;

		std::vector<std::unique_ptr<KinectDevice>> Refresh() const override;

		KinectFreenect2Plugin& operator=(const KinectFreenect2Plugin&) = delete;
		KinectFreenect2Plugin& operator=(KinectFreenect2Plugin&&) = delete;

	private:
		mutable libfreenect2::Freenect2 m_freenect;
};

#endif
