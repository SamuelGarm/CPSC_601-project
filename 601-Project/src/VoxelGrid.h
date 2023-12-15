/*
* A file that contains necessary functions and classes for contining data in a voxel based grid
* indexing starts with 0 at (0,0,0) then increases first along the x, then the y, then the z
*/
#pragma once
#include <glm/glm.hpp>
#include <set>
#include <stdexcept>
#include <iostream>
#include <mutex>

template <typename T>
class VoxelGrid  {
public:
	VoxelGrid(int x_length, int y_length, int z_length);
	~VoxelGrid();
	T& at(int x, int y, int z);
	T& at(glm::vec3);
	T& at(int _index);
	glm::vec3 indexToPos(int _index);
	int posToIndex(glm::vec3 position);
	void markUnoccupied(int _index);
	void markUnoccupied(glm::vec3 position);
	const std::set<int>& getOccupiedMap() { return occupied; };
	glm::vec3 getDimensions();

	

private:
	//how many voxels are along each axis
	int x_length = 0;
	int y_length = 0;
	int z_length = 0;

	T* data = nullptr;
	std::set<int> occupied;
	std::mutex mutex;
};

//definitions

template <class T>
T& VoxelGrid<T>::at(int _index) {
	if (_index < 0) {
		std::cout << "Negative index provided: " << _index << '\n' << std::fflush;
		throw std::exception("Negative index provided");
	}
	if (_index > (x_length * y_length * z_length) - 1) {
		std::cout << "out of bounds index provided: " << _index << '\n' << std::fflush;
		throw std::exception("out of bounds index provided");
	}
	//mark that cell as occupied since the voxel is in use
	std::lock_guard<std::mutex> lock(mutex);
	occupied.insert(_index);
	return data[_index];
}

template <class T>
T& VoxelGrid<T>::at(int _x, int _y, int _z) {
	int offset_z = y_length * x_length * _z;
	int offset_y = x_length * _y;
	int offset_x = _x;
	int index = offset_x + offset_y + offset_z;
	return at(index);
}

template <class T>
T& VoxelGrid<T>::at(glm::vec3 _pos) {
	return at(_pos.x, _pos.y, _pos.z);
}

template <class T>
glm::vec3 VoxelGrid<T>::indexToPos(int _index) {
	int z = floor(_index / (x_length * y_length));
	_index -= z * x_length * y_length;
	int y = floor(_index / x_length);
	_index -= y * x_length;
	int x = _index;
	return glm::vec3(x, y, z);
}

template <class T>
int VoxelGrid<T>::posToIndex(glm::vec3 pos) {
	int offset_z = y_length * x_length * pos.z;
	int offset_y = x_length * pos.y;
	int offset_x = pos.x;
	int index = offset_x + offset_y + offset_z;
	return index;
}

template <class T>
void VoxelGrid<T>::markUnoccupied(int _index) {
	occupied.erase(_index);
}

template <class T>
void VoxelGrid<T>::markUnoccupied(glm::vec3 pos) {

	markUnoccupied(posToIndex(pos));
}

template <class T>
VoxelGrid<T>::VoxelGrid(int _x_length, int _y_length, int _z_length) : x_length(_x_length), y_length(_y_length), z_length(_z_length) {
	// Initialize your voxel grid based on the provided dimensions
	data = new T[_x_length * _y_length * _z_length];  // Allocate memory for the voxel grid
	std::cout << "Set can contain " << occupied.max_size() << " entries\n";
}

template <class T>
VoxelGrid<T>::~VoxelGrid() {
	delete[] data;  // Free the allocated memory in the destructor
}

template <class T>
glm::vec3 VoxelGrid<T>::getDimensions() {
	return glm::vec3(x_length, y_length, z_length);
}
