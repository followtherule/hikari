#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "common.glsl"
#include "random.glsl"
#include "hitInfo.glsl"

layout(location = 0) rayPayloadInEXT Payload pld;
layout(location = 2) rayPayloadEXT bool shadowed;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0) uniform UBO
{
    mat4 viewInverse;
    mat4 projInverse;
    vec4 viewPos;
    vec3 lightPos;
    uint frame;
} ubo;

layout(binding = 5, set = 0) uniform sampler2D textures[];

vec3 offsetPositionAlongNormal(vec3 worldPosition, vec3 worldNormal)
{
    const float int_scale = 256.0f;
    const ivec3 of_i = ivec3(int_scale * worldNormal);

    const vec3 p_i = vec3(
            intBitsToFloat(floatBitsToInt(worldPosition.x) + ((worldPosition.x < 0) ? -of_i.x : of_i.x)),
            intBitsToFloat(floatBitsToInt(worldPosition.y) + ((worldPosition.y < 0) ? -of_i.y : of_i.y)),
            intBitsToFloat(floatBitsToInt(worldPosition.z) + ((worldPosition.z < 0) ? -of_i.z : of_i.z)));

    const float origin = 1.0f / 32.0f;
    const float floatScale = 1.0f / 65536.0f;
    return vec3(
        abs(worldPosition.x) < origin ? worldPosition.x + floatScale * worldNormal.x : p_i.x,
        abs(worldPosition.y) < origin ? worldPosition.y + floatScale * worldNormal.y : p_i.y,
        abs(worldPosition.z) < origin ? worldPosition.z + floatScale * worldNormal.z : p_i.z);
}

vec3 diffuseReflection(vec3 normal, inout uint rngState)
{
    const float theta = 2.0 * PI * rnd(rngState);
    const float u = 2.0 * rnd(rngState) - 1.0;
    const float r = sqrt(1.0 - u * u);
    const vec3 direction = normal + vec3(r * cos(theta), r * sin(theta), u);

    return normalize(direction);
}

void main()
{
    const int primitiveID = gl_PrimitiveID; // ID of the triangle in the geometry in the BLAS
    HitInfo hitInfo = GetHitInfo(primitiveID);

    pld.color = hitInfo.color.rgb;
    pld.miss = false;
    pld.newOrigin = offsetPositionAlongNormal(hitInfo.worldPos, hitInfo.worldNormal);
    pld.newDirection = diffuseReflection(hitInfo.worldNormal, pld.rngState);
    // pld.newDirection = reflect(gl_WorldRayDirectionEXT, hitInfo.worldNormal);

    // Shadow casting
    float tmin = 0.001;
    float tmax = 10000.0;
    float epsilon = 0.001;
    // vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT + hitInfo.worldNormal * epsilon;
    shadowed = true;
    vec3 lightVector = ubo.lightPos;
    // traceRayEXT(
    //     topLevelAS,
    //     gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT,
    //     0xFF,
    //     0,
    //     0,
    //     1,
    //     pld.newOrigin,
    //     tmin,
    //     lightVector,
    //     tmax,
    //     2
    // );
    // if (shadowed) {
    //     pld.color *= 0.7;
    // }
}
