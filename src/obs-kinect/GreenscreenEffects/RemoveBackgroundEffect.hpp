/******************************************************************************
	Copyright (C) 2021 by Jérôme Leclercq <lynix680@gmail.com>

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

#ifndef OBS_KINECT_PLUGIN_REMOVEBACKGROUNDEFFECT
#define OBS_KINECT_PLUGIN_REMOVEBACKGROUNDEFFECT

#include <obs-kinect/Shaders/AlphaMaskShader.hpp>

class RemoveBackgroundEffect
{
	public:
		struct Config;

		RemoveBackgroundEffect() = default;
		~RemoveBackgroundEffect() = default;

		gs_texture_t* Apply(const Config& config, gs_texture_t* sourceTexture, gs_texture_t* filterTexture);

		static obs_properties_t* BuildProperties();
		static void SetDefaultValues(obs_data_t* settings);
		static Config ToConfig(obs_data_t* settings);

		struct Config
		{
			using Effect = RemoveBackgroundEffect;
		};

	private:
		AlphaMaskShader m_alphaMaskFilter;
};

#endif
