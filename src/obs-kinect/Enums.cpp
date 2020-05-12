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

#include "Enums.hpp"

std::string EnabledSourceToString(SourceFlags flags)
{
	std::string str;
	str.reserve(64);
	if (flags & Source_BackgroundRemoval)
		str += "BackgroundRemoval | ";

	if (flags & Source_Body)
		str += "Body | ";

	if (flags & Source_Color)
		str += "Color | ";

	if (flags & Source_ColorToDepthMapping)
		str += "ColorToDepth | ";

	if (flags & Source_Depth)
		str += "Depth | ";

	if (flags & Source_Infrared)
		str += "Infrared | ";

	if (!str.empty())
		str.resize(str.size() - 3);
	else
		str = "<None>";

	return str;
}

const char* ProcessPriorityToString(ProcessPriority priority)
{
	switch (priority)
	{
		case ProcessPriority::Normal:      return "Normal";
		case ProcessPriority::AboveNormal: return "AboveNormal";
		case ProcessPriority::High:        return "High";
	}

	return "<unknown>";
}
