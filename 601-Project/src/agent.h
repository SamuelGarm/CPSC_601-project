#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/random.hpp"
#include "glm/gtx/string_cast.hpp"

#include <vector>
#include "settings.h"
#include "VoxelGrid.h"
#include "Pheromones.h"
#include "soil.h"
#include <iostream>

struct Agent {
	enum State { SEARCHING, RETURNING };
	State state = State::SEARCHING;
	glm::vec3 direction = glm::vec3(0, -1, 0);
	glm::vec3 position = glm::vec3(0);
};

struct agentRenderData {
	glm::mat4 transform = glm::mat4(1);
	glm::vec3 color = glm::vec3(1);
};


void stepAgents(std::vector<Agent>& agents, VoxelGrid<PheromoneVoxel>& pheromones, VoxelGrid<SoilVoxel>& soil) {
	const int numberRadialSamples = 8;
	const float sensorAngle = 3.14/6.f; //radians
	const float sensorDistance = 0.8; //1 = the side length of a soil voxel
	const float turnSpeed = 3.14/4; //how big the turn vector is
	const float moveSpeed = 0.8;
	const float randomMovementAngle = 3.14 / 10;
	const float nutrientWeight = 1;
	const float foodPheremoneWeight = 1;
	const float wanderPheremoneWeight = 1;

	//update agent
	for (Agent& agent : agents) {
		//create the coordinate frame
		glm::vec3 front = agent.direction;


		glm::vec3 right; //approximate a right vector. This is only used to generate a up vector that is guaranteed perpindicular to front
		//choose the axis that has the lowest dot with front
		float fdotx = abs(glm::dot(front, glm::vec3(1, 0, 0)));
		float fdoty = abs(glm::dot(front, glm::vec3(0, 1, 0)));
		float fdotz = abs(glm::dot(front, glm::vec3(0, 0, 1)));
		if (fdotx <= fdoty && fdotx <= fdotz)
			right = glm::vec3(1, 0, 0);
		else if (fdoty <= fdotx && fdoty <= fdotz)
			right = glm::vec3(0, 1, 0);
		else
			right = glm::vec3(0, 0, 1);

		glm::vec3 up = glm::cross(front, right);
		right = glm::cross(front, up); //calculate the TRUE right vector

		//std::cout << "\nAgent position: " << glm::to_string(agent.position) << '\n';
		//std::cout << "Front: " << glm::to_string(front) << " Right: " << glm::to_string(right) << " Up: " << glm::to_string(up) << '\n';

		
		std::vector<glm::vec3> samples;
		//sample the front direction
		samples.push_back(front * sensorDistance);

		for (int i = 0; i < numberRadialSamples; i++) {
			const float degreeStep = 6.28 / numberRadialSamples;
			float theta = degreeStep * i;
			//trace a circle normal to front
			glm::vec3 ds = normalize(sin(theta) * up + cos(theta) * right);
			glm::vec3 sampleOffset = normalize(sin(sensorAngle) * ds + cos(sensorAngle) * front);

			//scale the sample offset
			sampleOffset *= sensorDistance;
			
			//use the offset to calculate the sample point relative to the agent
			glm::vec3 samplePos = sampleOffset + agent.position;
			//std::cout << "Sample position " << glm::to_string(samplePos) << '\n';
			samples.push_back(samplePos);
		} //end sampling loop

		std::vector<std::pair<glm::vec3, float>> weights;
		//calculate the valid samples and their weight
		for (auto samplePos : samples) {
			//check that it is in bounds of the grid
			if (samplePos.x < 0 || samplePos.x > SOIL_X_LENGTH * 3 - 1
				|| samplePos.y < 0 || samplePos.y > SOIL_Y_LENGTH * 3 - 1
				|| samplePos.z < 0 || samplePos.z > SOIL_Z_LENGTH * 3 - 1)
				continue;
			glm::vec3 soilLoc = floor(samplePos / 3.f); //the location in the soil grid

			//calculate the weight for that location
			float weight = -1;

			if (agent.state == agent.SEARCHING) {
				float nutrient = soil.at(soilLoc.x, soilLoc.y, soilLoc.z).nutrient;
				float pheremone = pheromones.at(samplePos.x, samplePos.y, samplePos.z).pheromones[PheromoneVoxel::Food];
				weight = nutrient * nutrientWeight + pheremone * foodPheremoneWeight;
			}
			else if (agent.state == agent.RETURNING) {
				float pheremone = pheromones.at(samplePos.x, samplePos.y, samplePos.z).pheromones[PheromoneVoxel::Wander];
				weight = pheremone * wanderPheremoneWeight;
			}

			//add the weight to the vector after the if statments
			if (weight == -1)
				continue;
			if (weights.size() == 0 || weights[0].second == weight)
				weights.push_back(std::pair<glm::vec3, float>(samplePos, weight));
			else if (weight > weights[0].second) {
				weights.clear();
				weights.push_back(std::pair<glm::vec3, float>(samplePos, weight));
			}
		}

		//choose a random direction from the best ones
		if (weights.size() > 0) {
			int selection = glm::linearRand<int>(0, weights.size() - 1);
			glm::vec3 a = front;
			glm::vec3 b = normalize(weights[selection].first);
			glm::vec3 axis = glm::cross(a, b);

			glm::mat4 R = glm::rotate(glm::mat4(1.0f), turnSpeed, axis);

			glm::vec3 rotated = glm::vec3(R * glm::vec4(b, 1));
			/*
			turnVec *= turnSpeed;
			agent.direction = normalize(agent.direction + turnVec);
			agent.direction = normalize(agent.direction);
			*/
		}

			//std::cout << "CHOOSING RANDOM\n";
			//calculate a point on a disk defined by up and right
			float diskAngle = glm::linearRand<float>(0, 6.28);
			glm::vec3 b = normalize(sin(diskAngle) * up + cos(diskAngle) * right);
			glm::vec3 a = front;
			//compute direction vector
			float randAngle = glm::linearRand<float>(-randomMovementAngle, randomMovementAngle);
			glm::vec3 c = cos(randAngle) * a + sin(randAngle) * b;
			agent.direction += normalize(c);
			agent.direction = normalize(agent.direction);
		

	}//end direction update loop




	//update position step
	for (Agent& agent : agents) {
		//loop over each agent and move it
		//if the agent encounters a soil voxel eat some nutrient and make the agent want to follow the return pheromones

		/*
		* handle collisions
		* If the choosen direction will result in out of bounds the agent must be bounced immidietly
		* otherwise the agent will perform the appropiate interaction with the soil
		*/
		bool collision = false;
		do {
			collision = false;

			glm::vec3 nextPos = agent.position + (agent.direction * moveSpeed);
			glm::vec3 nextSoilPos = floor(nextPos / 3.f);
			//std::cout << "Next agent position is: " << glm::to_string(nextPos) << '\n';
			//std::cout << "Next soil position is " << glm::to_string(nextSoilPos) << '\n';


			//check that it is in bounds of the grid
			if (nextPos.x < 0 || nextPos.x > SOIL_X_LENGTH * 3
				|| nextPos.y < 0 || nextPos.y > SOIL_Y_LENGTH * 3
				|| nextPos.z < 0 || nextPos.z > SOIL_Z_LENGTH * 3) {
				//std::cout << "Out of bounds collision detected\n";
				collision = true;
			}
			else if (soil.at(nextSoilPos.x, nextSoilPos.y, nextSoilPos.z).isSoil) {
				//std::cout << "Soil collision detected\n";
				collision = true;
				SoilVoxel& nextSoilVox = soil.at(nextSoilPos.x, nextSoilPos.y, nextSoilPos.z);
				if (agent.state == agent.SEARCHING) {
					//if it is going to collide
					if (nextSoilVox.isSoil) {
						nextSoilVox.nutrient -= 1;
						if (nextSoilVox.nutrient <= 0) {
							nextSoilVox.isSoil = false;
							nextSoilVox.nutrient = 0;
							//std::cout << "Soil depleted, removing\n";
						}
						agent.state = agent.RETURNING;
					}
				}
			}

			//check if a collision occured on this frame and handle the bounce
			if (collision) {
				//calculate what vectors need to be flipped to bounce off the collision
				glm::vec3 currentSoilPos = floor(agent.position / 3.f);
				glm::vec3 diff = currentSoilPos - nextSoilPos;
				diff = glm::vec3(abs(diff.x) >= 1 ? -1 : 1, abs(diff.y) >= 1 ? -1 : 1, abs(diff.z) >= 1 ? -1 : 1);
				agent.direction *= diff;
			}
			
		} while (collision);

		//this is a strict state change, no need to put it in collison handler
		if (agent.state == agent.RETURNING) {
			//detect if the agent is in the nest region
			glm::vec3 smallValues = glm::vec3(((SOIL_X_LENGTH * 3) / 2) - 4*3, (SOIL_Y_LENGTH * 3) - 2, ((SOIL_Z_LENGTH * 3) / 2) - 4*3);
			glm::vec3 largeValues = glm::vec3(((SOIL_X_LENGTH * 3) / 2) + 4*3, (SOIL_Y_LENGTH * 3), ((SOIL_Z_LENGTH * 3) / 2) + 4*3);
			if (agent.position.x >= smallValues.x && agent.position.x <= largeValues.x &&
				agent.position.y >= smallValues.y && agent.position.y <= largeValues.y &&
				agent.position.z >= smallValues.z && agent.position.z <= largeValues.z)
				agent.state = agent.SEARCHING;
		}

		//deposit pheromones at the current location 
		if (agent.state == agent.SEARCHING)
			pheromones.at(agent.position.x, agent.position.y, agent.position.z).pheromones[PheromoneVoxel::Wander] += 1;
		else if (agent.state == agent.RETURNING)
			pheromones.at(agent.position.x, agent.position.y, agent.position.z).pheromones[PheromoneVoxel::Food] += 1;

		//move the agent
		agent.position += agent.direction * moveSpeed;

	}
}


void loadAgentRenderData(const std::vector<Agent>& agents, std::vector<agentRenderData>& instancedAgentData) {
	instancedAgentData.clear();
	for (const Agent& agent : agents) {
		int index = agent.position.x + agent.position.y * SOIL_X_LENGTH * 3 + agent.position.z * SOIL_X_LENGTH * 3 * SOIL_Y_LENGTH * 3;
		glm::vec3 position = glm::vec3(agent.position.x, agent.position.y, agent.position.z);
		agentRenderData data;
		data.transform = glm::translate(glm::mat4(1), (position - glm::vec3(1)) / 3.f) * glm::scale(glm::mat4(1), glm::vec3(1 / 3.f));
		data.color = agent.state == agent.RETURNING ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
		instancedAgentData.push_back(data);
	}
}
