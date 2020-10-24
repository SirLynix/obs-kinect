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

#include "RemoveBackgroundEffect.hpp"
#include "Helper.hpp"
#include <string>
#include <stdexcept>

gs_texture_t* RemoveBackgroundEffect::Apply(const Config& config, gs_texture_t* sourceTexture, gs_texture_t* filterTexture)
{
	return m_alphaMaskFilter.Filter(sourceTexture, filterTexture);
}

obs_properties_t* RemoveBackgroundEffect::BuildProperties()
{
	return nullptr;
}

void RemoveBackgroundEffect::SetDefaultValues(obs_data_t* /*settings*/)
{
}

auto RemoveBackgroundEffect::ToConfig(obs_data_t* /*settings*/) -> Config
{
	return {};
}
