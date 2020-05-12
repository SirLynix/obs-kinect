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

#ifndef OBS_KINECT_PLUGIN_ENUMS
#define OBS_KINECT_PLUGIN_ENUMS

#include "Helper.hpp"
#include <cstdint>
#include <string>

enum EnabledSources
{
	Source_BackgroundRemoval   = 1 << 0,
	Source_Body                = 1 << 1,
	Source_Color               = 1 << 2,
	Source_ColorToDepthMapping = 1 << 3,
	Source_Depth               = 1 << 4,
	Source_Infrared            = 1 << 5
};

using SourceFlags = std::uint32_t;

enum class ProcessPriority
{
	Normal = 0,
	AboveNormal = 1,
	High = 2
};

OBSKINECT_API std::string EnabledSourceToString(SourceFlags flags);
OBSKINECT_API const char* ProcessPriorityToString(ProcessPriority priority);

#endif
