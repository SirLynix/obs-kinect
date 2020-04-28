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

#ifndef OBS_KINECT_PLUGIN_GREENSCREENFILTEREFFECT
#define OBS_KINECT_PLUGIN_GREENSCREENFILTEREFFECT

#include <obs-module.h>
#include <cstdint>

class GreenScreenFilterEffect
{
	public:
		struct BodyFilterParams;
		struct DepthFilterParams;
		struct BodyOrDepthFilterParams;
		struct BodyWithinDepthFilterParams;

		GreenScreenFilterEffect();
		~GreenScreenFilterEffect();

		gs_texture_t* Filter(std::uint32_t width, std::uint32_t height, const BodyFilterParams& params);
		gs_texture_t* Filter(std::uint32_t width, std::uint32_t height, const BodyOrDepthFilterParams& params);
		gs_texture_t* Filter(std::uint32_t width, std::uint32_t height, const BodyWithinDepthFilterParams& params);
		gs_texture_t* Filter(std::uint32_t width, std::uint32_t height, const DepthFilterParams& params);

		struct BodyFilterParams
		{
			gs_texture_t* bodyIndexTexture;
			gs_texture_t* colorToDepthTexture;
		};

		struct DepthFilterParams
		{
			gs_texture_t* colorToDepthTexture;
			gs_texture_t* depthTexture;
			float progressiveDepth;
			float maxDepth;
			float minDepth;
		};

		struct BodyOrDepthFilterParams
		{
			gs_texture_t* bodyIndexTexture;
			gs_texture_t* colorToDepthTexture;
			gs_texture_t* depthTexture;
			float progressiveDepth;
			float maxDepth;
			float minDepth;
		};

		struct BodyWithinDepthFilterParams
		{
			gs_texture_t* bodyIndexTexture;
			gs_texture_t* colorToDepthTexture;
			gs_texture_t* depthTexture;
			float progressiveDepth;
			float maxDepth;
			float minDepth;
		};

	private:
		bool Begin(std::uint32_t width, std::uint32_t height);
		gs_texture* Process(std::uint32_t width, std::uint32_t height, gs_technique_t* technique);
		template<typename Params> void SetBodyParams(const Params& params);
		template<typename Params> void SetDepthParams(const Params& params);

		gs_effect_t* m_effect;
		gs_eparam_t* m_params_BodyIndexImage;
		gs_eparam_t* m_params_DepthImage;
		gs_eparam_t* m_params_DepthMappingImage;
		gs_eparam_t* m_params_InvDepthImageSize;
		gs_eparam_t* m_params_InvDepthProgressive;
		gs_eparam_t* m_params_MaxDepth;
		gs_eparam_t* m_params_MinDepth;
		gs_technique_t* m_tech_BodyOnlyWithDepthCorrection;
		gs_technique_t* m_tech_BodyOnlyWithoutDepthCorrection;
		gs_technique_t* m_tech_BodyOrDepthWithDepthCorrection;
		gs_technique_t* m_tech_BodyOrDepthWithoutDepthCorrection;
		gs_technique_t* m_tech_BodyWithinDepthWithDepthCorrection;
		gs_technique_t* m_tech_BodyWithinDepthWithoutDepthCorrection;
		gs_technique_t* m_tech_DepthOnlyWithDepthCorrection;
		gs_technique_t* m_tech_DepthOnlyWithoutDepthCorrection;
		gs_texrender_t* m_workTexture;
};

#include "GreenScreenFilterEffect.inl"

#endif
