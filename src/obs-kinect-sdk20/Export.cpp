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

#include "Helper.hpp"
#include "KinectSdk20Plugin.hpp"

extern "C"
{
	OBSKINECT_EXPORT KinectPluginImpl* ObsKinect_CreatePlugin(std::uint32_t version)
	{
		if (version != OBSKINECT_VERSION)
		{
			warn("Kinect plugin incompatibilities (obs-kinect version: %d, plugin version: %d)", OBSKINECT_VERSION, version);
			return nullptr;
		}

		return new KinectSdk20Plugin;
	}
}
