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

#ifndef OBS_KINECT_PLUGIN_GAUSSIANBLUREFFECT
#define OBS_KINECT_PLUGIN_GAUSSIANBLUREFFECT

#include <obs-module.h>
#include <cstddef>

class GaussianBlurEffect
{
	public:
		GaussianBlurEffect(gs_color_format colorFormat);
		~GaussianBlurEffect();

		gs_texture_t* Blur(gs_texture_t* source, std::size_t count);

	private:
		gs_effect_t* m_blurEffect;
		gs_eparam_t* m_blurEffect_Filter;
		gs_eparam_t* m_blurEffect_Image;
		gs_eparam_t* m_blurEffect_InvImageSize;
		gs_technique_t* m_blurEffect_DrawTech;
		gs_texrender_t* m_workTextureA;
		gs_texrender_t* m_workTextureB;
};

#endif
