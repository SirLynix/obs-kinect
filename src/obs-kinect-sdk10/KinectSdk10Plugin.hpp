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

#ifndef OBS_KINECT_PLUGIN_KINECTSDK10PLUGIN
#define OBS_KINECT_PLUGIN_KINECTSDK10PLUGIN

#include "Sdk10Helper.hpp"
#include <obs-kinect-core/KinectPluginImpl.hpp>
#include <obs-kinect-core/Win32Helper.hpp>
#include <combaseapi.h>
#include <NuiApi.h>

#if __has_include(<KinectBackgroundRemoval.h>)
#define HAS_BACKGROUND_REMOVAL 1
#include <KinectBackgroundRemoval.h>
#else
#define HAS_BACKGROUND_REMOVAL 0
#endif

namespace Dyn
{
#if HAS_BACKGROUND_REMOVAL
	using NuiCreateBackgroundRemovedColorStreamPtr = decltype(&::NuiCreateBackgroundRemovedColorStream);

	extern NuiCreateBackgroundRemovedColorStreamPtr NuiCreateBackgroundRemovedColorStream;
#endif
}

class KinectSdk10Plugin : public KinectPluginImpl
{
	public:
		KinectSdk10Plugin();
		KinectSdk10Plugin(const KinectSdk10Plugin&) = delete;
		KinectSdk10Plugin(KinectSdk10Plugin&&) = delete;
		~KinectSdk10Plugin();

		std::string GetUniqueName() const override;

		std::vector<std::unique_ptr<KinectDevice>> Refresh() const override;

		KinectSdk10Plugin& operator=(const KinectSdk10Plugin&) = delete;
		KinectSdk10Plugin& operator=(KinectSdk10Plugin&&) = delete;

	private:
#if HAS_BACKGROUND_REMOVAL
		ObsLibPtr m_backgroundRemovalLib;
#endif
};

#endif
