$input a_position, a_normal, a_tangent, a_bitangent, a_texcoord0
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

#define MATERIAL_HAS_NORMAL
#define MATERIAL_HAS_CLEAR_COAT_NORMAL
#define MATERIAL_HAS_ANISOTROPY

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
	//v_worldNormal = output.worldNormal;
	//v_worldTangent = output.worldTangent;
	//v_worldBitangent = output.worldBitangent;

	v_worldNormal = mul(u_objectUniforms_worldFromModelNormalMatrix, a_normal * 2.0 - 1.0);
	v_worldTangent = mul(u_objectUniforms_worldFromModelNormalMatrix, a_tangent * 2.0 - 1.0);
	v_worldBitangent = mul(u_objectUniforms_worldFromModelNormalMatrix, a_bitangent * 2.0 - 1.0);

	v_lightSpacePosition = output.lightSpacePosition;
	gl_Position = output.clipPosition;
}


