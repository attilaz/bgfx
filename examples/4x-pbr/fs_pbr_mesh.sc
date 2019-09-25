$input v_worldPosition, v_worldNormal, v_worldTangent, v_worldBitangent, v_lightSpacePosition

#include "../common/common.sh"

//#define SHADING_MODEL_SPECULAR_GLOSSINESS
//#define SHADING_MODEL_CLOTH
//#define SHADING_MODEL_SUBSURFACE         
//#define SHADING_MODEL_UNLIT

#define MATERIAL_HAS_SUBSURFACE_COLOR
#define MATERIAL_HAS_NORMAL
#define MATERIAL_HAS_CLEAR_COAT
#define MATERIAL_HAS_CLEAR_COAT_NORMAL
#define MATERIAL_HAS_ANISOTROPY
#define MATERIAL_HAS_DOUBLE_SIDED_CAPABILITY
#define MATERIAL_HAS_AMBIENT_OCCLUSION
#define MATERIAL_HAS_CLEAR_COAT_ROUGHNESS
#define MATERIAL_HAS_EMISSIVE

//lighting
#define HAS_SHADOWING
#define HAS_DIRECTIONAL_LIGHTING
#define HAS_DYNAMIC_LIGHTING
#define HAS_SHADOW_MULTIPLIER

#define GEOMETRIC_SPECULAR_AA
#define SPECULAR_AMBIENT_OCCLUSION	(1)
#define MULTI_BOUNCE_AMBIENT_OCCLUSION (1)
#define CLEAR_COAT_IOR_CHANGE

//#define BLEND_MODE_MASKED
#define BLEND_MODE_TRANSPARENT
//#define BLEND_MODE_FADE



#include "filament.sh"

void main() {
	FragmentStageInputs stageIn;
	initFragmentStageInputs(stageIn);
	stageIn.worldPosition = v_worldPosition;
	stageIn.worldNormal = v_worldNormal;
	stageIn.worldTangent = v_worldTangent;
	stageIn.worldBitangent = v_worldBitangent;
	stageIn.fragCoord = gl_FragCoord;
	stageIn.frontFacing = gl_FrontFacing;

	// Initialize the inputs to sensible default values, see material_inputs.fs
	MaterialInputs materialIn;
	initMaterial(materialIn);

	// todo: modify material inputs here

	gl_FragColor = evaluate(materialIn, stageIn);
}

#if 0
#include <simd/simd.h>

using namespace metal;

constant float _1696 = {};
constant float3 _1697 = {};
constant float4 _1698 = {};

struct xlatMtlMain_out
{
    float4 bgfx_FragData0 [[color(0)]];
};

fragment xlatMtlMain_out xlatMtlMain(texture2d<float> light_iblDFG [[texture(3)]], texturecube<float> light_iblSpecular [[texture(4)]], texture2d<float> light_ssao [[texture(5)]], sampler light_iblDFGSampler [[sampler(3)]], sampler light_iblSpecularSampler [[sampler(4)]], sampler light_ssaoSampler [[sampler(5)]], float4 gl_FragCoord [[position]])
{
    xlatMtlMain_out out = {};
    float4 _1030 = light_iblDFG.sample(light_iblDFGSampler, float2(_1696, 1.0), level(0.0));
    float2 _1676;
    out.bgfx_FragData0 = float4((((float3(0.800000011920928955078125) * fast::max((((((((_1697 + (_1697 * _1697.y)) + (_1697 * _1697.z)) + (_1697 * _1697.x)) + (_1697 * (_1697.y * _1697.x))) + (_1697 * (_1697.y * _1697.z))) + (_1697 * (((3.0 * _1697.z) * _1697.z) - 1.0))) + (_1697 * (_1697.z * _1697.x))) + (_1697 * ((_1697.x * _1697.x) - (_1697.y * _1697.y))), float3(0.0))) * fast::min(1.0, light_ssao.sample(light_ssaoSampler, (gl_FragCoord.xy * _1698.zw)).x)) + (((float3(0.039999999105930328369140625) * _1030.x) + float3(_1030.y)) * light_iblSpecular.sample(light_iblSpecularSampler, _1697, level(_1676.x)).xyz)) * _1696, 1.0);
    return out;
}
#endif
