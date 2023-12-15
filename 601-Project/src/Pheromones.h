#pragma once
#include <map>
#include "VoxelGrid.h"
#include <vector>
#include "settings.h"
#include "soil.h"
#include <array>



struct PheromoneVoxel {
	enum Pheromones {
		Wander = 0,
		Food,
		NUMBER_OF_PHEROMONES 
	};

	//stores how many agents of each type are in a single 'agent voxel'
	float pheromones[NUMBER_OF_PHEROMONES] = { 0,0 };
	//float wanderPheremone = 0;
	//float foodFoundPheremone = 0;
	PheromoneVoxel& operator+=(const PheromoneVoxel& rhs) {
		for (int i = 0; i < NUMBER_OF_PHEROMONES; i++)
			pheromones[i] += rhs.pheromones[i];
		//wanderPheremone += rhs.wanderPheremone;
		//foodFoundPheremone += rhs.foodFoundPheremone;
		return *this;
	}

	PheromoneVoxel operator*(const float& rhs) const {
		PheromoneVoxel result = *this; // Create a copy of the current instance
		//result.wanderPheremone *= rhs;
		//result.foodFoundPheremone *= rhs;
		for (int i = 0; i < NUMBER_OF_PHEROMONES; i++) {
			result.pheromones[i] *= rhs;
		}
		return result;
	}

};

struct pheremoneRenderData {
	glm::mat4 transform = glm::mat4(1);
	glm::vec3 color = glm::vec3(0);
};

void diffusePheromones(VoxelGrid<PheromoneVoxel>& pheromones, VoxelGrid<SoilVoxel>& soil) {
	const float diffusionRate = 0.2;
	//evaporate pheremones outwards
	std::map<int, PheromoneVoxel> newPheremoneMap;
	for (int e : pheromones.getOccupiedMap()) {
		glm::vec3 originVoxelPos = pheromones.indexToPos(e);
		//for each voxel with a pheremone cosntruct a list of what neighbour voxels can be diffused to
		std::vector<glm::vec3> neighbours;
		for (int x = -1; x <= 1; x++) {
			for (int y = -1; y <= 1; y++) {
				for (int z = -1; z <= 1; z++) {
					glm::vec3 neighbourVoxelPos = originVoxelPos + glm::vec3(x, y, z);
					//check the voxel is in bounds
					if (neighbourVoxelPos.x < 0 || neighbourVoxelPos.x > SOIL_X_LENGTH * 3 - 1
						|| neighbourVoxelPos.y < 0 || neighbourVoxelPos.y > SOIL_Y_LENGTH * 3 - 1
						|| neighbourVoxelPos.z < 0 || neighbourVoxelPos.z > SOIL_Z_LENGTH * 3 - 1)
						continue;
					//check if the neighbour position is in a soil voxel
					if (soil.at(floor(neighbourVoxelPos / 3.f)).isSoil)
						continue;

					//add it to the neighbour list
					neighbours.push_back(neighbourVoxelPos);
				}
			}
		}

		//for each neighbour in the list add some pheremone to it
		PheromoneVoxel original = pheromones.at(originVoxelPos) * diffusionRate;
		for (glm::vec3 neighbourPos : neighbours) {
			PheromoneVoxel neighbour;
			for (int i = 0; i < neighbour.NUMBER_OF_PHEROMONES; i++) {
				neighbour.pheromones[i] += original.pheromones[i] / neighbours.size();
			}
			//neighbour.foodFoundPheremone += original.foodFoundPheremone / neighbours.size();
			//neighbour.wanderPheremone += original.wanderPheremone / neighbours.size();
			newPheremoneMap[pheromones.posToIndex(neighbourPos)] += neighbour;
		}
		newPheremoneMap[pheromones.posToIndex(originVoxelPos)] += pheromones.at(originVoxelPos) * (1 - diffusionRate);
	}

	//update the actual map with the new pheromones
	for (auto e : newPheremoneMap)
		pheromones.at(e.first) = e.second;
}

void evaporatePheromones(VoxelGrid<PheromoneVoxel>& pheromones) {
	//evaporate pheremones

	for (auto e : pheromones.getOccupiedMap()) {
		float a = 0.003;
		//float& foodPheremone = pheromones.at(e).foodFoundPheremone;
		//float& wanderPheremone = pheromones.at(e).wanderPheremone;

		//foodPheremone = foodPheremone > 0.02 ? foodPheremone - log(a * foodPheremone + 1) : 0;
		//wanderPheremone = wanderPheremone > 0.02 ? wanderPheremone - log(a * wanderPheremone + 1) : 0;


		for (int i = 0; i < PheromoneVoxel::NUMBER_OF_PHEROMONES; i++) {
			float& pheromone = pheromones.at(e).pheromones[i];
			pheromone = pheromone > 0.02 ? pheromone - log(a * pheromone + 1) : 0;

		}
	}
}


void loadPheremoneRenderData(VoxelGrid<PheromoneVoxel>& pheromones, std::vector<pheremoneRenderData>& instancedPheremoneData) {
	instancedPheremoneData.clear();
	//float maxWander = 0;
	//float maxFood = 0;
	auto& map = pheromones.getOccupiedMap();

	std::array<float, PheromoneVoxel::NUMBER_OF_PHEROMONES> maxs;

	std::vector<int> toMarkUnoccupied;
	for (auto e : map) {
		glm::vec3 position = pheromones.indexToPos(e);
		PheromoneVoxel voxel = pheromones.at(position);

		//maxWander = std::max(maxWander, voxel.wanderPheremone);
		//maxFood = std::max(maxFood, voxel.foodFoundPheremone);

		int zeros = 0;
		for (int i = 0; i < PheromoneVoxel::NUMBER_OF_PHEROMONES; i++) {
			maxs[i] = std::max(maxs[i], voxel.pheromones[i]);
			if (voxel.pheromones[i] == 0)
				zeros++;
		}
		if (zeros == PheromoneVoxel::NUMBER_OF_PHEROMONES) {
			toMarkUnoccupied.push_back(e);
			continue;
		}

		pheremoneRenderData data;
		data.transform = glm::translate(glm::mat4(1), (position - glm::vec3(1)) / 3.f) * glm::scale(glm::mat4(1), glm::vec3(1 / 3.f));
		data.color = voxel.pheromones[PheromoneVoxel::Food] * glm::vec3(0, 0, 1) + voxel.pheromones[PheromoneVoxel::Wander] * glm::vec3(0, 1, 0);
		instancedPheremoneData.push_back(data);
	}

	for (auto e : toMarkUnoccupied)
		pheromones.markUnoccupied(e);

	for (auto& e : instancedPheremoneData) {
		e.color.b /= maxs[PheromoneVoxel::Food];
		e.color.g /= maxs[PheromoneVoxel::Wander];
	}
}
