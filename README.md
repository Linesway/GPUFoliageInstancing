# GPU Foliage Instnacing using DirectX12
DirectX12 Vertex Factories Created for GPU Instancing onto Marching Cubes surfaces

https://github.com/user-attachments/assets/109425c0-f248-4862-8c70-b655b6b8dd3f

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

![Screenshot 2024-12-26 201452](https://github.com/user-attachments/assets/d6de7ef4-159e-42de-9acd-7671ec1b33ab)
![Screenshot 2024-12-18 223840](https://github.com/user-attachments/assets/6dd79bb9-f72c-42f4-bdc2-90d7a9281bbe)

https://github.com/user-attachments/assets/fd2ed82f-0aa4-48ca-81d5-c357830cd5f8



