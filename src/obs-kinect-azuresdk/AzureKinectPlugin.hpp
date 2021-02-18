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

#ifndef OBS_KINECT_PLUGIN_AZUREKINECT
#define OBS_KINECT_PLUGIN_AZUREKINECT

#include "AzureHelper.hpp"
#include <obs-kinect-core/Enums.hpp>
#include <obs-kinect-core/KinectPluginImpl.hpp>

#if __has_include(<k4abt.hpp>)
#define HAS_BODY_TRACKING 1
#include "AzureKinectBodyTrackingDynFuncs.hpp"
#else
#define HAS_BODY_TRACKING 0
#endif

class AzureKinectPlugin : public KinectPluginImpl
{
	public:
		AzureKinectPlugin();
		AzureKinectPlugin(const AzureKinectPlugin&) = delete;
		AzureKinectPlugin(AzureKinectPlugin&&) = delete;
		~AzureKinectPlugin();

		std::string GetUniqueName() const override;

		std::vector<std::unique_ptr<KinectDevice>> Refresh() const override;

		AzureKinectPlugin& operator=(const AzureKinectPlugin&) = delete;
		AzureKinectPlugin& operator=(AzureKinectPlugin&&) = delete;

	private:
#if HAS_BODY_TRACKING
		ObsLibPtr m_bodyTrackingLib;
#endif
};

#endif
