# VertexFactories
My Custom HLSL Vertex Factories for Instances on Marching Cubes

https://github.com/user-attachments/assets/fd2ed82f-0aa4-48ca-81d5-c357830cd5f8

They use DrawIndirect by writing the indices to a pointer to minimize CPU to GPU transfers of small information like triangle count or instance count for the instances.

