$input a_position, a_normal
$output v_view, v_normal

#include "../common/common.sh"
#include "uniforms.sh"

#define VERTEX_DOMAIN_OBJECT
//#define VERTEX_DOMAIN_DEVICE

#include "filament_uniforms.sh"
#include "common_math.fs"
#include "inputs.vs"
#include "common_getters.fs"
#include "getters.vs"
#include "material_inputs.vs"
void materialVertex(inout MaterialVertexInputs material);
#include "main.vs"

void materialVertex(inout MaterialVertexInputs material) {

}


