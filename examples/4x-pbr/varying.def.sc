vec3 v_worldNormal    : NORMAL    = vec3(0.0, 0.0, 1.0);
vec3 v_worldTangent   : TANGENT   = vec3(1.0, 0.0, 0.0);
vec3 v_worldBitangent : BITANGENT = vec3(0.0, 1.0, 0.0);

vec3 v_worldPosition : TEXCOORD0 = vec3(0.0, 0.0, 0.0);

vec4 v_lightSpacePosition : TEXCOORD1 = vec4(0.0, 0.0, 0.0, 0.0);

vec4 a_position  : POSITION;
vec2 a_texcoord0 : TEXCOORD0;
vec3 a_normal    : NORMAL;
vec4 a_tangent   : TANGENT;
