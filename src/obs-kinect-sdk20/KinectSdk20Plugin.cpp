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

#include "KinectSdk20Plugin.hpp"
#include "KinectSdk20Device.hpp"

std::string KinectSdk20Plugin::GetUniqueName() const
{
	return "KinectSDK2.0";
}

std::vector<std::unique_ptr<KinectDevice>> KinectSdk20Plugin::Refresh() const
{
	std::vector<std::unique_ptr<KinectDevice>> devices;

	try
	{
		// We have only one device: the default one
		devices.emplace_back(std::make_unique<KinectSdk20Device>());
	}
	catch (const std::exception& e)
	{
		warn("%s", e.what());
	}

	return devices;
}
