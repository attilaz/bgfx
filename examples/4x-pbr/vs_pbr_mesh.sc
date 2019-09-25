$input a_position, a_normal, a_texcoord0
$output v_worldPosition, v_worldNormal, v_worldTangent, v_worldBitangent, v_lightSpacePosition

#include "../common/common.sh"

#define VERTEX_DOMAIN_OBJECT
//#define VERTEX_DOMAIN_WORLD 
//#define VERTEX_DOMAIN_VIEW  
//#define VERTEX_DOMAIN_DEVICE

#define HAS_ATTRIBUTE_TANGENTS
#define HAS_ATTRIBUTE_BONE_INDICES
#define HAS_ATTRIBUTE_BONE_WEIGHTS
#define HAS_SKINNING_OR_MORPHING

#define HAS_SHADOWING
#define HAS_DIRECTIONAL_LIGHTING

#include "filament.sh"

void main()
{
	VertexAttributes attributes;
	initAttributes(attributes);
	attributes.position = a_position;
#if defined(HAS_ATTRIBUTE_TANGENTS)
	attributes.tangents = vec4(0.0,0.0,0.0,1.0);
#endif

	VertexOutput output;
	evaluate(output, attributes);

	v_worldPosition = output.worldPosition;
	v_worldNormal = output.worldNormal;
	v_worldTangent = output.worldTangent;
	v_worldBitangent = output.worldBitangent;
	v_lightSpacePosition = output.lightSpacePosition;
	gl_Position = output.clipPosition;
}


#if 0

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct _Global
{
    float4x4 u_viewProj;
    float4x4 u_model[32];
};

struct xlatMtlMain_out
{
    float3 _entryPointOutput_v_normal [[user(locn0)]];
    float3 _entryPointOutput_v_vertex_worldPosition [[user(locn1)]];
    float3 _entryPointOutput_v_view [[user(locn2)]];
    float4 gl_Position [[position]];
};

struct xlatMtlMain_in
{
    float4 a_position [[attribute(1)]];
};

vertex xlatMtlMain_out xlatMtlMain(xlatMtlMain_in in [[stage_in]], constant _Global& _mtl_u [[buffer(0)]])
{
    xlatMtlMain_out out = {};
    float4 _253 = (float4(_mtl_u.u_model[0][0][0], _mtl_u.u_model[0][1][0], _mtl_u.u_model[0][2][0], _mtl_u.u_model[0][3][0]) * in.a_position.x) + ((float4(_mtl_u.u_model[0][0][1], _mtl_u.u_model[0][1][1], _mtl_u.u_model[0][2][1], _mtl_u.u_model[0][3][1]) * in.a_position.y) + ((float4(_mtl_u.u_model[0][0][2], _mtl_u.u_model[0][1][2], _mtl_u.u_model[0][2][2], _mtl_u.u_model[0][3][2]) * in.a_position.z) + float4(_mtl_u.u_model[0][0][3], _mtl_u.u_model[0][1][3], _mtl_u.u_model[0][2][3], _mtl_u.u_model[0][3][3])));
    out.gl_Position = _mtl_u.u_viewProj * _253;
    out._entryPointOutput_v_normal = float3(0.0, 0.0, 1.0);
    out._entryPointOutput_v_vertex_worldPosition = _253.xyz;
    out._entryPointOutput_v_view = float3(0.0);
    return out;
}

   € 
 #endif
