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

#ifndef OBS_KINECT_PLUGIN_KINECTDEVICEACCESS
#define OBS_KINECT_PLUGIN_KINECTDEVICEACCESS

#include "Enums.hpp"
#include "Helper.hpp"
#include "KinectDevice.hpp"
#include <memory>

class KinectDeviceAccess
{
	friend KinectDevice;

	public:
		KinectDeviceAccess(const KinectDeviceAccess&) = delete;
		KinectDeviceAccess(KinectDeviceAccess&& access) noexcept;
		~KinectDeviceAccess();

		SourceFlags GetEnabledSourceFlags() const;

		KinectFrameConstPtr GetLastFrame();

		void SetEnabledSourceFlags(SourceFlags enabledSources);

		void UpdateDeviceParameters(obs_data_t* settings);

		KinectDeviceAccess& operator=(const KinectDeviceAccess&) = delete;
		KinectDeviceAccess& operator=(KinectDeviceAccess&& access) noexcept;

	private:
		KinectDeviceAccess(KinectDevice& owner, KinectDevice::AccessData* accessData);

		KinectDevice* m_owner;
		KinectDevice::AccessData* m_data;
};

#endif
