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

#include "ConvertDepthIRToColorEffect.hpp"
#include "Helper.hpp"
#include <string>
#include <stdexcept>

static const char* colorMultiplierEffect = R"(
uniform float4x4 ViewProj;
uniform texture2d ColorImage;
uniform float ColorMultiplier;

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
	float color = ColorImage.Sample(textureSampler, vert_in.uv).r;
	color *= ColorMultiplier;

	return float4(color, color, color, 1.0);
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

ConvertDepthIRToColorEffect::ConvertDepthIRToColorEffect()
{
	ObsGraphics gfx;

	char* errStr;

	m_effect = gs_effect_create(colorMultiplierEffect, "color_multiplier.effect", &errStr);
	if (m_effect)
	{
		m_params_ColorImage = gs_effect_get_param_by_name(m_effect, "ColorImage");
		m_params_ColorMultiplier = gs_effect_get_param_by_name(m_effect, "ColorMultiplier");
		m_tech_Draw = gs_effect_get_technique(m_effect, "Draw");

		m_workTexture = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	}
	else
	{
		std::string err("failed to create effect: ");
		err.append((errStr) ? errStr : "shader error");
		bfree(errStr);

		throw std::runtime_error(err);
	}
}

ConvertDepthIRToColorEffect::~ConvertDepthIRToColorEffect()
{
	ObsGraphics gfx;

	gs_effect_destroy(m_effect);
	gs_texrender_destroy(m_workTexture);
}

gs_texture_t* ConvertDepthIRToColorEffect::Convert(std::uint32_t width, std::uint32_t height, gs_texture_t* source, float averageValue, float standardDeviation)
{
	gs_texrender_reset(m_workTexture);
	if (!gs_texrender_begin(m_workTexture, width, height))
		return nullptr;

	vec4 black = { 0.f, 0.f, 0.f, 0.f };
	gs_clear(GS_CLEAR_COLOR, &black, 0.f, 0);
	gs_ortho(0.0f, float(width), 0.0f, float(height), -100.0f, 100.0f);

	gs_effect_set_texture(m_params_ColorImage, source);
	gs_effect_set_float(m_params_ColorMultiplier, float(1.0 / (double(averageValue) * double(standardDeviation))));

	gs_technique_begin(m_tech_Draw);
	gs_technique_begin_pass(m_tech_Draw, 0);
	gs_draw_sprite(nullptr, 0, width, height);
	gs_technique_end_pass(m_tech_Draw);
	gs_technique_end(m_tech_Draw);

	gs_texrender_end(m_workTexture);

	return gs_texrender_get_texture(m_workTexture);
}
