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

#include "GaussianBlurEffect.hpp"
#include "Helper.hpp"
#include <string>
#include <stdexcept>

static const char* gaussianBlurEffect = R"(
uniform float4x4 ViewProj;
uniform texture2d Image;
uniform float2 Filter;
uniform float2 InvImageSize;

sampler_state textureSampler {
	Filter   = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertData {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

VertData VSDefault(VertData vert_in)
{
	VertData vert_out;
	vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = vert_in.uv;
	return vert_out;
}

float4 PSColorFilterRGBA(VertData vert_in) : TARGET
{
	static const float KernelOffsets[3] = { 0.0f, 1.3846153846f, 3.2307692308f };
	static const float BlurWeights[3] = { 0.2270270270f, 0.3162162162f, 0.0702702703f };

	/* Grab the current pixel to perform operations on. */
	float3 color = Image.Sample(textureSampler, vert_in.uv).xyz * BlurWeights[0];

	for (int i = 1; i < 3; ++i)
	{
		float2 offset = InvImageSize * Filter * KernelOffsets[i];
		color += BlurWeights[i] * (Image.Sample(textureSampler, vert_in.uv + offset).xyz +
		                           Image.Sample(textureSampler, vert_in.uv - offset).xyz);
	}

	return float4(color, 1.0);
}

technique Draw
{
	pass
	{
		vertex_shader = VSDefault(vert_in);
		pixel_shader = PSColorFilterRGBA(vert_in);
	}
}
)";

GaussianBlurEffect::GaussianBlurEffect(gs_color_format colorFormat)
{
	ObsGraphics gfx;

	char* errStr;

	m_blurEffect = gs_effect_create(gaussianBlurEffect, "gaussian_blur.effect", &errStr);
	if (m_blurEffect)
	{
		m_blurEffect_Filter = gs_effect_get_param_by_name(m_blurEffect, "Filter");
		m_blurEffect_Image = gs_effect_get_param_by_name(m_blurEffect, "Image");
		m_blurEffect_InvImageSize = gs_effect_get_param_by_name(m_blurEffect, "InvImageSize");
		m_blurEffect_DrawTech = gs_effect_get_technique(m_blurEffect, "Draw");

		m_workTextureA = gs_texrender_create(colorFormat, GS_ZS_NONE);
		m_workTextureB = gs_texrender_create(colorFormat, GS_ZS_NONE);
	}
	else
	{
		std::string err("failed to create effect: ");
		err.append((errStr) ? errStr : "shader error");
		bfree(errStr);

		throw std::runtime_error(err);
	}
}

GaussianBlurEffect::~GaussianBlurEffect()
{
	ObsGraphics gfx;

	gs_effect_destroy(m_blurEffect);
	gs_texrender_destroy(m_workTextureA);
	gs_texrender_destroy(m_workTextureB);
}

gs_texture_t* GaussianBlurEffect::Blur(gs_texture_t* source, std::size_t count)
{
	std::uint32_t width = gs_texture_get_width(source);
	std::uint32_t height = gs_texture_get_height(source);

	vec2 filter;
	vec2 invTextureSize = { 1.f / width, 1.f / height };

	for (std::size_t blurIndex = 0; blurIndex < count; ++blurIndex)
	{
		gs_texrender_reset(m_workTextureA);
		if (!gs_texrender_begin(m_workTextureA, width, height))
			return nullptr;

		gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);

		filter.x = 1.f;
		filter.y = 0.f;

		gs_effect_set_vec2(m_blurEffect_Filter, &filter);
		gs_effect_set_vec2(m_blurEffect_InvImageSize, &invTextureSize);
		gs_effect_set_texture(m_blurEffect_Image, (blurIndex == 0) ? source : gs_texrender_get_texture(m_workTextureB));

		gs_technique_begin(m_blurEffect_DrawTech);
		gs_technique_begin_pass(m_blurEffect_DrawTech, 0);
		gs_draw_sprite(nullptr, 0, width, height);
		gs_technique_end_pass(m_blurEffect_DrawTech);
		gs_technique_end(m_blurEffect_DrawTech);

		gs_texrender_end(m_workTextureA);

		gs_texrender_reset(m_workTextureB);
		if (!gs_texrender_begin(m_workTextureB, width, height))
			return nullptr;
			
		gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);

		filter.x = 0.f;
		filter.y = 1.f;

		gs_effect_set_vec2(m_blurEffect_Filter, &filter);
		gs_effect_set_vec2(m_blurEffect_InvImageSize, &invTextureSize);
		gs_effect_set_texture(m_blurEffect_Image, gs_texrender_get_texture(m_workTextureA));

		gs_technique_begin(m_blurEffect_DrawTech);
		gs_technique_begin_pass(m_blurEffect_DrawTech, 0);
		gs_draw_sprite(nullptr, 0, width, height);
		gs_technique_end_pass(m_blurEffect_DrawTech);
		gs_technique_end(m_blurEffect_DrawTech);

		gs_texrender_end(m_workTextureB);
	}

	return gs_texrender_get_texture(m_workTextureB);
}
