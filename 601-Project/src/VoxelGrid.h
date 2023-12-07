/*
* A file that contains necessary functions and classes for contining data in a voxel based grid
* indexing starts with 0 at (0,0,0) then increases first along the x, then the y, then the z
*/
#pragma once
#include <glm/glm.hpp>
#include <map>

template <typename T>
class VoxelGrid  {
public:
	VoxelGrid(int x_length, int y_length, int z_length);
	~VoxelGrid();
	T& at(int x, int y, int z);
	T& at(glm::vec3);
	T& at(int _index);
	const std::map<int, bool>& getOccupiedMap() { return occupied; };
	glm::vec3 getDimensions();

	

private:
	//how many voxels are along each axis
	int x_length = 0;
	int y_length = 0;
	int z_length = 0;

	T* data = nullptr;
	std::map<int, bool> occupied;
};

//definitions

template <class T>
T& VoxelGrid<T>::at(int _index) {
	//mark that cell as occupied since the voxel is in use
	occupied[_index] = true;
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
VoxelGrid<T>::VoxelGrid(int _x_length, int _y_length, int _z_length) : x_length(_x_length), y_length(_y_length), z_length(_z_length) {
	// Initialize your voxel grid based on the provided dimensions
	data = new T[_x_length * _y_length * _z_length];  // Allocate memory for the voxel grid
}

template <class T>
VoxelGrid<T>::~VoxelGrid() {
	delete[] data;  // Free the allocated memory in the destructor
}

template <class T>
glm::vec3 VoxelGrid<T>::getDimensions() {
	return glm::vec3(x_length, y_length, z_length);
}
