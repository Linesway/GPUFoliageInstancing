# VertexFactories
My Custom HLSL Vertex Factories for Instances on Marching Cubes

https://github.com/user-attachments/assets/fd2ed82f-0aa4-48ca-81d5-c357830cd5f8

They use DrawIndirect by writing the indices to a pointer to minimize CPU to GPU transfers of small information like triangle count or instance count for the instances.

The rotation of the intances is stored as a float4 representing a Quaternion to allow easy snapping of the foliage rotation the surface level.

struct MeshItem
{
	float3 Position;
	float4 Rotation;
	float3 Scale;
	int ID;
};

ID stores the foliage type which points to an array in game to determine which static mesh to spawn.

