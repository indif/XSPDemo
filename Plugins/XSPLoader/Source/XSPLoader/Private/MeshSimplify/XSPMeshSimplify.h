#pragma once
#include <vector>

bool XSPSimplifyMesh(const std::vector<float>& InPositions, float PercentTriangles, float PercentVertices, std::vector<float>& OutPositions, std::vector<float>& OutNormals, std::vector<uint32_t>& OutIndices);