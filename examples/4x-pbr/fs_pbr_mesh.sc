$input v_vertex_worldPosition

#include "../common/common.sh"


#if 0


void main()
{
	gl_FragColor = vec4(vertex_worldPosition.x,0.0,0.0,0.0);
}

#else

#include "uniforms.sh"

#define LAYOUT_LOCATION(A)

#include "filament_uniforms.sh"
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


void material(inout MaterialInputs material) {
	prepareMaterial(material);
	material.baseColor.rgb = vec3(0.8);
}


#endif
