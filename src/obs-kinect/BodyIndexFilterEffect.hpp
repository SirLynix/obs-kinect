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

#ifndef OBS_KINECT_PLUGIN_BODYINDEXFILTEREFFECT
#define OBS_KINECT_PLUGIN_BODYINDEXFILTEREFFECT

#include <obs-module.h>
#include <cstdint>

class BodyIndexFilterEffect
{
	public:
		struct Params;

		BodyIndexFilterEffect();
		~BodyIndexFilterEffect();

		gs_texture_t* Filter(std::uint32_t width, std::uint32_t height, const Params& params);

		struct Params
		{
			gs_texture_t* bodyIndexTexture;
			gs_texture_t* colorToDepthTexture;
		};

	private:
		gs_effect_t* m_effect;
		gs_eparam_t* m_params_BodyIndexImage;
		gs_eparam_t* m_params_DepthMappingImage;
		gs_eparam_t* m_params_InvDepthImageSize;
		gs_technique_t* m_tech_DepthCorrection;
		gs_technique_t* m_tech_WithoutDepthCorrection;
		gs_texrender_t* m_workTexture;
};

#endif
