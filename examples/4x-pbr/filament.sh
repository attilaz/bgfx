/*
documentation about interface (macros, varyings, uniforms)


*/

#define LOCATION_POSITION (0)
#define LAYOUT_LOCATION(a)

struct FrameUniforms
{
	highp mat4 viewFromWorldMatrix;
	highp mat4 worldFromViewMatrix;
	highp mat4 clipFromViewMatrix;
	highp mat4 viewFromClipMatrix;
	highp mat4 clipFromWorldMatrix;
	highp mat4 worldFromClipMatrix;
	highp mat4 lightFromWorldMatrix;
	highp float4 resolution;
	highp float3 cameraPosition;
	highp float  time;
	highp float4 lightColorIntensity;
	highp float4 sun;
	highp float3 lightDirection;
	uint fParamsX;
	float3 shadowBias;
	float oneOverFroxelDimensionY;
	float4 zParams;
	uint2 fParams;
	float2 origin;
	float oneOverFroxelDimension;
	float iblLuminance;
	float exposure;
	float ev100;
	float3 iblSH[9];
	float4 userTime;
	float2 iblMaxMipLevel;
	float2 padding0;
	float3 worldOffset;
	float padding1;
	float4 padding2;
};

uniform FrameUniforms frameUniforms;

#define CONFIG_MAX_LIGHT_COUNT (256)


struct LightsUniforms
{
	highp mat4 lights[CONFIG_MAX_LIGHT_COUNT];
};

uniform LightsUniforms lightsUniforms;

#if BGFX_SHADER_TYPE_VERTEX

#define CONFIG_MAX_BONE_COUNT (256)

struct BonesUniforms
{
	mediump float4 bones[CONFIG_MAX_BONE_COUNT*4];
};

uniform BonesUniforms bonesUniforms;

struct ObjectUniforms
{
	highp mat4 worldFromModelMatrix;
	highp mat3 worldFromModelNormalMatrix;
	highp float4 morphWeights;
	int skinningEnabled;
	int morphingEnabled;
	float2 padding0;
};
ObjectUniforms objectUniforms;

#define VERTEX_DOMAIN_OBJECT
//#define VERTEX_DOMAIN_DEVICE

#include "common_math.fs"
#include "inputs.vs"
#include "common_getters.fs"
#include "getters.vs"
#include "material_inputs.vs"
void materialVertex(inout MaterialVertexInputs material);
#include "main.vs"

#endif


#if BGFX_SHADER_TYPE_FRAGMENT

uniform lowp sampler2DShadow light_shadowMap;
ISAMPLER2D(light_records, 1); //uniform mediump isampler2D light_records;
ISAMPLER2D(light_froxels, 2); //uniform mediump isampler2D light_froxels;
uniform mediump sampler2D light_iblDFG;
uniform mediump samplerCube light_iblSpecular;
uniform mediump sampler2D light_ssao;

static highp vec4 FragCoord;

#define LAYOUT_LOCATION(A)

#include "material_inputs.fs"
#include "inputs.fs"
#include "common_shading.fs"
#include "getters.fs"
#include "shading_parameters.fs"
void material(inout MaterialInputs material);

#include "common_math.fs"
#include "common_lighting.fs"
#include "common_material.fs"
#include "common_graphics.fs"

#include "ambient_occlusion.fs"

#include "brdf.fs"
#include "shading_model_standard.fs"

#include "light_indirect.fs"
#include "light_punctual.fs"
#include "light_directional.fs"

#include "shading_lit.fs"
//#include "shading_unlit.fs"
#include "main.fs"

#endif
