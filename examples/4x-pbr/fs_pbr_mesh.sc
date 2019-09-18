$input v_vertex_worldPosition

#include "../common/common.sh"


#include "filament.sh"

void material(inout MaterialInputs material) {
	prepareMaterial(material);
	material.baseColor.rgb = vec3(0.8);
}
