#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/random.hpp"

#include <vector>
#include "settings.h"
#include "VoxelGrid.h"
#include "Pheromones.h"
#include "soil.h"

struct Agent {
	enum State { SEARCHING, RETURNING };
	State state = State::SEARCHING;
	glm::vec3 direction = glm::vec3(0, -1, 0);
	glm::vec3 position = glm::vec3(0);
};

struct agentRenderData {
	glm::mat4 transform = glm::mat4(1);
};


void stepAgents(std::vector<Agent>& agents, VoxelGrid<PheromoneVoxel>& pheromones, VoxelGrid<SoilVoxel>& soil) {
	for (Agent& agent : agents) {
		//create the coordinate frame
		glm::vec3 front = agent.direction;
		glm::vec3 right = agent.direction.x == 0 ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
		glm::vec3 up = glm::cross(glm::vec3(front), glm::vec3(right));

		//std::cout << "\n-----------------------------------------------------------------------\n";
		//std::cout << "Agent position: " << glm::to_string(agent.position) << '\n';
		//std::cout << "Agent orientation:\n\tFront: " << glm::to_string(front) << "\n\tRight: " << glm::to_string(right) << "\n\tUp: " << glm::to_string(up) << '\n';

		//patterns are specified as coordinates relative to the agents local frame <Heading, Right, Up> (They should be symetrical)
		//pattern 1 is a search of the 3x3 grid directly in front
		std::vector<glm::vec3> pattern1 = { {1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

		//set the pattern you want to use
		std::vector<glm::vec3> pattern = pattern1;

		float nutrientWeight = 1;
		float foodPheremoneWeight = 1;
		float wanderPheremoneWeight = 1;


		//calculate where the agent will be moving based on each element in the pattern
		//create a vector of possible locations with their respective weights
		std::vector<std::pair<glm::vec3, float>> weights;
		for (glm::vec3 offset : pattern) {
			glm::vec3 searchLoc = agent.position + (front * offset[0] + right * offset[1] + up * offset[2]); //the location in the agent grid
			glm::vec3 soilLoc = floor(searchLoc / 3.f); //the location in the soil grid
			if (searchLoc.x < 0 || searchLoc.x > SOIL_X_LENGTH * 3 - 1
				|| searchLoc.y < 0 || searchLoc.y > SOIL_Y_LENGTH * 3 - 1
				|| searchLoc.z < 0 || searchLoc.z > SOIL_Z_LENGTH * 3 - 1)
				continue;
			//std::cout << '\n';
			//std::cout << "Searching agent position: " << glm::to_string(searchLoc) << '\n';
			//std::cout << "Searching soil position: " << glm::to_string(soilLoc) << '\n';
			//based on the agent state the weight can be calculated differently
			float weight = -1;

			if (agent.state == agent.SEARCHING) {
				float nutrient = soil.at(soilLoc.x, soilLoc.y, soilLoc.z).nutrient;
				float pheremone = pheromones.at(searchLoc.x, searchLoc.y, searchLoc.z).foodFoundPheremone;
				weight = nutrient * nutrientWeight + pheremone * foodPheremoneWeight;
			}
			else if (agent.state == agent.RETURNING) {
				float pheremone = pheromones.at(searchLoc.x, searchLoc.y, searchLoc.z).wanderPheremone;
				weight = pheremone * wanderPheremoneWeight;
			}


			//add the weight to the vector after the if statments
			if (weight <= 0)
				continue;
			weights.push_back(std::pair<glm::vec3, float>(searchLoc, weight));
		}

		//calculate how the agent decides to move based on its state
		//go through all the weights that have been calculated and choose a direction
		std::pair<glm::vec3, float> best(glm::vec3(0), -1);
		for (auto& e : weights) {
			if (e.second > best.second)
				best = e;
		}
		//std::cout << "Best weight is\n\tPos: " << glm::to_string(best.first) << " \n\tWeight: " << best.second << '\n';
		while (best.second == -1 || best.first.x < 0 || best.first.x > SOIL_X_LENGTH * 3 - 1
			|| best.first.y < 0 || best.first.y > SOIL_Y_LENGTH * 3 - 1
			|| best.first.z < 0 || best.first.z > SOIL_Z_LENGTH * 3 - 1) {
			//if there is no best then choose a random direction
			float randInd = glm::linearRand<float>(0, pattern.size() - 1);
			best.first = pattern[randInd] + agent.position;
			best.second = 0;
			//std::cout << "Random position choosen: " << glm::to_string(pattern[randInd]) << '\n';
		}
		glm::vec3 direction = best.first - agent.position;
		agent.direction = direction;
	}

	//update position step
	for (Agent& agent : agents) {
		//loop over each agent and move it
		//if the agent encounters a soil voxel eat some nutrient and make the agent want to follow the return pheromones
		glm::vec3 nextPos = agent.position + agent.direction;
		glm::vec3 nextSoilPos = nextPos / 3.f;
		SoilVoxel& soilVox = soil.at(nextSoilPos.x, nextSoilPos.y, nextSoilPos.z);

		if (agent.state == agent.SEARCHING) {
			//if it is going to collide
			if (soilVox.isSoil) {
				//std::cout << "collision detected\n";
				soilVox.nutrient -= 1;
				if (soilVox.nutrient <= 0) {
					soilVox.isSoil = false;
					soilVox.nutrient = 0;
					//std::cout << "Soil depleted, removing\n";
				}
				agent.state = agent.RETURNING;
				agent.direction *= -1;
			}
		}
		else if (agent.state == agent.RETURNING) {
			if (soilVox.isSoil)
				agent.direction *= -1;
			//detect if the agent is in the nest region
			glm::vec3 smallValues = glm::vec3(((SOIL_X_LENGTH * 3) / 2) - 2, (SOIL_Y_LENGTH * 3) - 2, ((SOIL_Z_LENGTH * 3) / 2) - 2);
			glm::vec3 largeValues = glm::vec3(((SOIL_X_LENGTH * 3) / 2) + 2, (SOIL_Y_LENGTH * 3), ((SOIL_Z_LENGTH * 3) / 2) + 2);
			if (agent.position.x >= smallValues.x && agent.position.x <= largeValues.x &&
				agent.position.y >= smallValues.y && agent.position.y <= largeValues.y &&
				agent.position.z >= smallValues.z && agent.position.z <= largeValues.z)
				agent.state = agent.SEARCHING;
		}

		if (agent.state == agent.SEARCHING)
			pheromones.at(agent.position.x, agent.position.y, agent.position.z).wanderPheremone += 10;
		else if (agent.state == agent.RETURNING)
			pheromones.at(agent.position.x, agent.position.y, agent.position.z).foodFoundPheremone += 10;

		agent.position += agent.direction;

	}
}


void loadAgentRenderData(const std::vector<Agent>& agents, std::vector<agentRenderData>& instancedAgentData) {
	instancedAgentData.clear();
	for (const Agent& agent : agents) {
		int index = agent.position.x + agent.position.y * SOIL_X_LENGTH * 3 + agent.position.z * SOIL_X_LENGTH * 3 * SOIL_Y_LENGTH * 3;
		glm::vec3 position = glm::vec3(agent.position.x, agent.position.y, agent.position.z);
		agentRenderData data;
		data.transform = glm::translate(glm::mat4(1), (position - glm::vec3(1)) / 3.f) * glm::scale(glm::mat4(1), glm::vec3(1 / 3.f));
		instancedAgentData.push_back(data);
	}
}
