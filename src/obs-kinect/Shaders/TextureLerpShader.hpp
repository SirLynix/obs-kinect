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

#ifndef OBS_KINECT_PLUGIN_TEXTURELERPSHADER
#define OBS_KINECT_PLUGIN_TEXTURELERPSHADER

#include <obs-module.h>
#include <cstddef>

class TextureLerpShader
{
	public:
		TextureLerpShader();
		~TextureLerpShader();

		gs_texture_t* Lerp(gs_texture_t* from, gs_texture_t* to, gs_texture_t* factor);

	private:
		gs_effect_t* m_effect;
		gs_eparam_t* m_params_FactorImage;
		gs_eparam_t* m_params_FromImage;
		gs_eparam_t* m_params_ToImage;
		gs_technique_t* m_tech_Draw;
		gs_texrender_t* m_workTexture;
};

#endif
