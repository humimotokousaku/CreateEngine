#pragma once
#include "../VertexData.h"
#include "./base/MaterialData.h"
#include <vector>

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};