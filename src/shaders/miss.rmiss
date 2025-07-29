#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "common.glsl"

layout(binding = 3, set = 0) uniform samplerCube samplerEnv;

layout(location = 0) rayPayloadInEXT Payload pld;

void main()
{
    pld.color = texture(samplerEnv, gl_WorldRayDirectionEXT).rgb;
    pld.miss = true;
}
