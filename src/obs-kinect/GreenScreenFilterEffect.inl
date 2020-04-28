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

#include "GreenScreenFilterEffect.hpp"

template<typename Params>
void GreenScreenFilterEffect::SetBodyParams(const Params& params)
{
	std::uint32_t bodyIndexWidth = gs_texture_get_width(params.bodyIndexTexture);
	std::uint32_t bodyIndexHeight = gs_texture_get_height(params.bodyIndexTexture);

	vec2 invDepthSize = { 1.f / bodyIndexWidth, 1.f / bodyIndexHeight };

	gs_effect_set_vec2(m_params_InvDepthImageSize, &invDepthSize);
	gs_effect_set_texture(m_params_BodyIndexImage, params.bodyIndexTexture);
	gs_effect_set_texture(m_params_DepthMappingImage, params.colorToDepthTexture);
}

template<typename Params>
void GreenScreenFilterEffect::SetDepthParams(const Params& params)
{
	std::uint32_t depthWidth = gs_texture_get_width(params.depthTexture);
	std::uint32_t depthHeight = gs_texture_get_height(params.depthTexture);

	vec2 invDepthSize = { 1.f / depthWidth, 1.f / depthHeight };

	constexpr float maxDepthValue = 0xFFFF;
	constexpr float invMaxDepthValue = 1.f / maxDepthValue;

	gs_effect_set_vec2(m_params_InvDepthImageSize, &invDepthSize);
	gs_effect_set_texture(m_params_DepthImage, params.depthTexture);
	gs_effect_set_texture(m_params_DepthMappingImage, params.colorToDepthTexture);
	gs_effect_set_float(m_params_InvDepthProgressive, maxDepthValue / params.progressiveDepth);
	gs_effect_set_float(m_params_MaxDepth, params.maxDepth * invMaxDepthValue);
	gs_effect_set_float(m_params_MinDepth, params.minDepth * invMaxDepthValue);
}
