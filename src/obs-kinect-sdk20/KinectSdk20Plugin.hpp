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

#ifndef OBS_KINECT_PLUGIN_KINECTSDK20PLUGIN
#define OBS_KINECT_PLUGIN_KINECTSDK20PLUGIN

#include "Enums.hpp"
#include "KinectPluginImpl.hpp"

class KinectSdk20Plugin : public KinectPluginImpl
{
	public:
		KinectSdk20Plugin() = default;
		KinectSdk20Plugin(const KinectSdk20Plugin&) = delete;
		KinectSdk20Plugin(KinectSdk20Plugin&&) = delete;
		~KinectSdk20Plugin() = default;

		std::string GetUniqueName() const override;

		std::vector<std::unique_ptr<KinectDevice>> Refresh() const override;

		KinectSdk20Plugin& operator=(const KinectSdk20Plugin&) = delete;
		KinectSdk20Plugin& operator=(KinectSdk20Plugin&&) = delete;
};

#endif
