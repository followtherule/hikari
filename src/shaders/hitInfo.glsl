hitAttributeEXT vec2 attribs;

struct GeometryNode {
    uint64_t vertexBufferDeviceAddress;
    uint64_t indexBufferDeviceAddress;
    int baseColorTextureIndex;
    int occlusionTextureIndex;
    int normalTextureIndex;
};

layout(binding = 4, set = 0) buffer GeometryNodes {
    GeometryNode nodes[];
} geometryNodes;
layout(buffer_reference, scalar) buffer Vertices {
    vec4 v[];
};
layout(buffer_reference, scalar) buffer Indices {
    uint i[];
};
layout(buffer_reference, scalar) buffer Data {
    vec4 f[];
};

layout(binding = 5, set = 0) uniform sampler2D textures[];

struct Vertex
{
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

struct HitInfo {
    vec3 localPos;
    vec3 worldPos;
    vec3 localNormal;
    vec3 worldNormal;
    vec4 color;
    vec2 uv;
};

HitInfo GetHitInfo(uint primitiveID) {
    HitInfo hitInfo;
    const uint triIndex = primitiveID * 3;

    GeometryNode geometryNode = geometryNodes.nodes[gl_GeometryIndexEXT];

    Indices indices = Indices(geometryNode.indexBufferDeviceAddress);
    Vertices vertices = Vertices(geometryNode.vertexBufferDeviceAddress);
    Vertex verticeInfos[3];
    for (uint i = 0; i < 3; i++) {
        const uint vertexIndex = indices.i[triIndex + i];
        const uint glTFVertexSize = 24; // 24 float
        const uint offset = vertexIndex * glTFVertexSize / 4;
        vec4 d0 = vertices.v[offset + 0];
        vec4 d1 = vertices.v[offset + 1];
        verticeInfos[i].pos = d0.xyz;
        verticeInfos[i].normal = vec3(d0.w, d1.xy);
        verticeInfos[i].uv = d1.zw;
    }
    const vec3 barycentric = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    // position
    hitInfo.localPos = verticeInfos[0].pos * barycentric.x + verticeInfos[1].pos * barycentric.y + verticeInfos[2].pos * barycentric.z;
    hitInfo.worldPos = gl_ObjectToWorldEXT * vec4(hitInfo.localPos, 1.0f);

    // uv
    hitInfo.uv = verticeInfos[0].uv * barycentric.x + verticeInfos[1].uv * barycentric.y + verticeInfos[2].uv * barycentric.z;

    // normal
    hitInfo.localNormal = texture(textures[nonuniformEXT(geometryNode.normalTextureIndex)], hitInfo.uv).rgb;
    hitInfo.worldNormal = normalize((hitInfo.localNormal * gl_WorldToObjectEXT).xyz);
    hitInfo.worldNormal = faceforward(hitInfo.worldNormal, gl_WorldRayDirectionEXT, hitInfo.worldNormal);

    // color
    hitInfo.color = texture(textures[nonuniformEXT(geometryNode.baseColorTextureIndex)], hitInfo.uv);

    return hitInfo;
}
