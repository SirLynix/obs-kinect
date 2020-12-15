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

#ifndef OBS_KINECT_PLUGIN_HELPER_KINECTDEVICESDK10
#define OBS_KINECT_PLUGIN_HELPER_KINECTDEVICESDK10

#ifdef OBS_KINECT_PLUGIN_HELPER
#error "This file must be included before Helper.hpp"
#endif

#define logprefix "[obs-kinect] [sdk10] "

#include "Helper.hpp"

enum class BacklightCompensation
{
	AverageBrightness,
	CenterPriority,
	LowLightsPriority,
	CenterOnly
};

#endif
