#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/random.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "VoxelGrid.h"
#include "settings.h"
#include "clippingPlanes.h"


struct SoilVoxel {
	float nutrient = 0;
	bool isSoil = true; //if the soil is actually there or if it is now 'root'
};

struct soilRenderData {
	glm::mat4 transform = glm::mat4(1);
	float nutrient = 0;
};

void generateSoil(VoxelGrid<SoilVoxel>& soil) {
	const int numberOfSources = 5;
	//generate the soil with reasonable nutrient distribution
	//generate n nutrient source points
	std::vector<glm::vec3> sources;
	for (int i = 0; i < numberOfSources; i++) {
		sources.push_back(glm::vec3(glm::linearRand<int>(0, SOIL_X_LENGTH), glm::linearRand<int>(0, SOIL_Y_LENGTH), glm::linearRand<int>(0, SOIL_Z_LENGTH)));
	}
	for (int x = 0; x < SOIL_X_LENGTH; x++) {
		for (int y = 0; y < SOIL_Y_LENGTH; y++) {
			for (int z = 0; z < SOIL_Z_LENGTH; z++) {
				glm::vec3 soilPoint(x, y, z);
				//find the closest source and use a linear falloff
				float shortestDistance = glm::distance(soilPoint, sources[0]);
				for (int i = 1; i < numberOfSources; i++) {
					float distance = glm::distance(soilPoint, sources[i]);
					if (distance < shortestDistance)
						shortestDistance = distance;
				}
				//calculate the nutrient value based on a falloff
				soil.at(x, y, z).nutrient = 50.f / (shortestDistance + 50.f);
			}
		}
	}

	/*
	//generate a hold for the 'nest'
	for (int x = (SOIL_X_LENGTH / 2) - 4; x <= (SOIL_X_LENGTH / 2) + 4; x++) {
		for (int y = SOIL_Y_LENGTH-2; y < SOIL_Y_LENGTH; y++) {
			for (int z = (SOIL_Z_LENGTH / 2) - 4; z <= (SOIL_Z_LENGTH / 2) + 4; z++) {
				glm::vec3 samplePos = glm::vec3(x, y, z);
				if (samplePos.x < 0 || samplePos.x > SOIL_X_LENGTH - 1
					|| samplePos.y < 0 || samplePos.y > SOIL_Y_LENGTH - 1
					|| samplePos.z < 0 || samplePos.z > SOIL_Z_LENGTH - 1)
					continue;
				soil.at(samplePos).isSoil = false;
				soil.at(samplePos).nutrient = 0;
			}
		}
	}*/
}


void loadSoilRenderData(VoxelGrid<SoilVoxel>& soil, std::vector<soilRenderData>& instancedVoxelData, bool isSoilCond = true, clippingPlanes* clip = nullptr) {
	glm::vec3 upperBounds;
	glm::vec3 lowerBounds;
	if (clip == nullptr) {
		upperBounds = soil.getDimensions();
		lowerBounds = glm::vec3(0);
	}
	else {
		upperBounds = glm::vec3(clip->xClipMax, clip->yClipMax, clip->zClipMax);
		lowerBounds = glm::vec3(clip->xClipMin, clip->yClipMin, clip->zClipMin);
	}

	instancedVoxelData.clear();
	//set up simulation (columns sweep the x, rows sweep the y, layers sweep the z
		for (int x = lowerBounds.x; x < upperBounds.x; x++) {
			for (int y = lowerBounds.y; y < upperBounds.y; y++) {
				for (int z = lowerBounds.z; z < upperBounds.z; z++) {
				if (soil.at(x, y, z).isSoil != isSoilCond) {
					continue;
				}
				char neighbours = 0;
				if (x > lowerBounds.x && soil.at(x - 1, y, z).isSoil == isSoilCond) neighbours++;
				if (x < upperBounds.x - 1 && soil.at(x + 1, y, z).isSoil == isSoilCond) neighbours++;
				if (y > lowerBounds.y && soil.at(x, y - 1, z).isSoil == isSoilCond) neighbours++;
				if (y < upperBounds.y - 1 && soil.at(x, y + 1, z).isSoil == isSoilCond) neighbours++;
				if (z > lowerBounds.z && soil.at(x, y, z - 1).isSoil == isSoilCond) neighbours++;
				if (z < upperBounds.z - 1 && soil.at(x, y, z + 1).isSoil == isSoilCond) neighbours++;
				if (neighbours == 6) continue;
				soilRenderData data;
				data.transform = glm::translate(glm::mat4(1), glm::vec3(x, y, z));
				data.nutrient = soil.at(x, y, z).nutrient;
				instancedVoxelData.push_back(data);
			}
		}
	}
}
