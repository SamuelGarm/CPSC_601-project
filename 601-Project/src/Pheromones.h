#pragma once
#include <map>
#include "VoxelGrid.h"
#include <vector>
#include "settings.h"
#include "soil.h"
#include <array>

#include "clippingPlanes.h"

struct PheromoneProperties {
	float evaporationRate;
	float diffusionRate;
};

struct PheromoneVoxel {
	enum Pheromones {
		Wander = 0,
		Food,
		Root,
		NUMBER_OF_PHEROMONES 
	};

	//stores how many agents of each type are in a single 'agent voxel'
	float pheromones[NUMBER_OF_PHEROMONES] = { 0,0,0 };

	struct PheromoneProperties {
		double evaporation;
		double diffusion;
	};
	static const PheromoneProperties properties[NUMBER_OF_PHEROMONES];

	PheromoneVoxel& operator+=(const PheromoneVoxel& rhs) {
		for (int i = 0; i < NUMBER_OF_PHEROMONES; i++)
			pheromones[i] += rhs.pheromones[i];
		return *this;
	}

	PheromoneVoxel operator*(const float& rhs) const {
		PheromoneVoxel result = *this; // Create a copy of the current instance
		for (int i = 0; i < NUMBER_OF_PHEROMONES; i++) {
			result.pheromones[i] *= rhs;
		}
		return result;
	}

};

const PheromoneVoxel::PheromoneProperties PheromoneVoxel::properties[NUMBER_OF_PHEROMONES] = {
		{0.003, 0.2},  // Wander 0.2
		{0.003, 0.2},  // Food
		{0, 0}         // Root
};

struct pheremoneRenderData {
	glm::mat4 transform = glm::mat4(1);
	glm::vec3 color = glm::vec3(0);
};

std::mutex mutex;

void diffusePheromones(VoxelGrid<PheromoneVoxel>& pheromones, VoxelGrid<SoilVoxel>& soil) {
	std::lock_guard<std::mutex> lock(mutex);
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
		PheromoneVoxel original = pheromones.at(originVoxelPos);
		for (glm::vec3 neighbourPos : neighbours) {
			PheromoneVoxel neighbour;
			for (int i = 0; i < neighbour.NUMBER_OF_PHEROMONES; i++) {
				neighbour.pheromones[i] += (original.pheromones[i] * PheromoneVoxel::properties[i].diffusion) / neighbours.size();
			}
			newPheremoneMap[pheromones.posToIndex(neighbourPos)] += neighbour;
		}

		for(int i = 0; i < PheromoneVoxel::NUMBER_OF_PHEROMONES; i++)
			newPheremoneMap[pheromones.posToIndex(originVoxelPos)].pheromones[i] += pheromones.at(originVoxelPos).pheromones[i] * (1 - PheromoneVoxel::properties[i].diffusion);
		

	}//end neighbour diffusion loop

	//update the actual map with the new pheromones
	for (auto e : newPheremoneMap)
		pheromones.at(e.first) = e.second;
}

void evaporatePheromones(VoxelGrid<PheromoneVoxel>& pheromones) {
	std::lock_guard<std::mutex> lock(mutex);
	//evaporate pheremones
	for (auto e : pheromones.getOccupiedMap()) {
		for (int i = 0; i < PheromoneVoxel::NUMBER_OF_PHEROMONES; i++) {
			float& pheromone = pheromones.at(e).pheromones[i];
			pheromone = pheromone > 0.02 ? pheromone - log(PheromoneVoxel::properties[i].evaporation * pheromone + 1) : 0;

		}
	}
}

void pheromoneReactions(VoxelGrid<PheromoneVoxel>& pheromones) {
	std::lock_guard<std::mutex> lock(mutex);
	for (auto e : pheromones.getOccupiedMap()) {
		if (pheromones.at(e).pheromones[PheromoneVoxel::Food] > 5) {
			//convert food pheromone into established root pheromones
			pheromones.at(e).pheromones[PheromoneVoxel::Root] += 1;
			pheromones.at(e).pheromones[PheromoneVoxel::Food] -= 5;
		}
	}
}

void loadPheremoneRenderData(VoxelGrid<PheromoneVoxel>& pheromones, std::vector<pheremoneRenderData>& instancedPheremoneData, std::vector<PheromoneVoxel::Pheromones> filter = {}, clippingPlanes* clip = nullptr) {
	glm::vec3 upperBounds;
	glm::vec3 lowerBounds;
	if (clip == nullptr) {
		upperBounds = pheromones.getDimensions();
		lowerBounds = glm::vec3(0);
	}
	else {
		upperBounds = glm::vec3(clip->xClipMax, clip->yClipMax, clip->zClipMax);
		lowerBounds = glm::vec3(clip->xClipMin, clip->yClipMin, clip->zClipMin);
	}

	std::lock_guard<std::mutex> lock(mutex);
	instancedPheremoneData.clear();

	std::array<float, PheromoneVoxel::NUMBER_OF_PHEROMONES> renderFlags;
	for (int i = 0; i < PheromoneVoxel::NUMBER_OF_PHEROMONES; i++) {
		if (filter.size() > 0) {
			bool found = false;
			for (auto filt : filter) {
				int numRep = static_cast<int>(filt);
				if (numRep == i)
					found = true;
			}
			if (!found) {
				renderFlags[i] = 0;
				continue;
			}
		}
		renderFlags[i] = 1;
	}


	std::array<float, PheromoneVoxel::NUMBER_OF_PHEROMONES> maxs;

	std::vector<int> toMarkUnoccupied;
	for (auto e : pheromones.getOccupiedMap()) {
		glm::vec3 position = pheromones.indexToPos(e);
		if (position.x < lowerBounds.x || position.x >= upperBounds.x ||
			position.y < lowerBounds.y || position.y >= upperBounds.y ||
			position.z < lowerBounds.z || position.z >= upperBounds.z)
			continue;
		
		PheromoneVoxel voxel = pheromones.at(position);

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
		data.color = voxel.pheromones[PheromoneVoxel::Food] * glm::vec3(0, 0, 1) * renderFlags[PheromoneVoxel::Food] +
								voxel.pheromones[PheromoneVoxel::Wander] * glm::vec3(0, 1, 0) * renderFlags[PheromoneVoxel::Wander] +
								voxel.pheromones[PheromoneVoxel::Root] * glm::vec3(1, 0, 0) * renderFlags[PheromoneVoxel::Root];
		if (glm::length(data.color) > 0) {
			data.transform = glm::translate(glm::mat4(1), (position - glm::vec3(1)) / 3.f) * glm::scale(glm::mat4(1), glm::vec3(1 / 3.f));
			instancedPheremoneData.push_back(data);
		}
	}

	for (auto e : toMarkUnoccupied)
		pheromones.markUnoccupied(e);

	for (auto& e : instancedPheremoneData) {
		e.color.b /= maxs[PheromoneVoxel::Food];
		e.color.g /= maxs[PheromoneVoxel::Wander];
		e.color.r /= maxs[PheromoneVoxel::Root];
	}
}
