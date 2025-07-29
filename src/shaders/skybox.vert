#version 450

layout(location = 0) in vec3 inPos;

layout(binding = 0) uniform UBO
{
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 outUVW;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    outUVW = inPos;
    vec4 pos = ubo.proj * ubo.view * vec4(inPos, 0.0);
    gl_Position = pos.xyww;
}
