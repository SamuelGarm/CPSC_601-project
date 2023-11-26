GLfloat cube_data[]{
	//x y z
	// cube data generated using chatGPT
	// Front face
	-0.5, -0.5, -0.5, 0.0, 0.0, -1.0,  // Vertex 1 (front-bottom-left)
	 0.5, -0.5, -0.5, 0.0, 0.0, -1.0,  // Vertex 2 (front-bottom-right)
	 0.5,  0.5, -0.5, 0.0, 0.0, -1.0,  // Vertex 3 (front-top-right)

	-0.5, -0.5, -0.5, 0.0, 0.0, -1.0,  // Vertex 1 (front-bottom-left)
	 0.5,  0.5, -0.5, 0.0, 0.0, -1.0,  // Vertex 3 (front-top-right)
	-0.5,  0.5, -0.5, 0.0, 0.0, -1.0,  // Vertex 4 (front-top-left)

	// Back face
	-0.5, -0.5,  0.5, 0.0, 0.0,  1.0,  // Vertex 5 (back-bottom-left)
	 0.5, -0.5,  0.5, 0.0, 0.0,  1.0,  // Vertex 6 (back-bottom-right)
	 0.5,  0.5,  0.5, 0.0, 0.0,  1.0,  // Vertex 7 (back-top-right)

	-0.5, -0.5,  0.5, 0.0, 0.0,  1.0,  // Vertex 5 (back-bottom-left)
	 0.5,  0.5,  0.5, 0.0, 0.0,  1.0,  // Vertex 7 (back-top-right)
	-0.5,  0.5,  0.5, 0.0, 0.0,  1.0,  // Vertex 8 (back-top-left)

	// Left face
	-0.5, -0.5, -0.5, -1.0, 0.0, 0.0,  // Vertex 1 (front-bottom-left)
	-0.5,  0.5, -0.5, -1.0, 0.0, 0.0,  // Vertex 4 (front-top-left)
	-0.5,  0.5,  0.5, -1.0, 0.0, 0.0,  // Vertex 8 (back-top-left)

	-0.5, -0.5, -0.5, -1.0, 0.0, 0.0,  // Vertex 1 (front-bottom-left)
	-0.5,  0.5,  0.5, -1.0, 0.0, 0.0,  // Vertex 8 (back-top-left)
	-0.5, -0.5,  0.5, -1.0, 0.0, 0.0,  // Vertex 5 (back-bottom-left)

	// Right face
	 0.5, -0.5, -0.5, 1.0, 0.0, 0.0,  // Vertex 2 (front-bottom-right)
	 0.5,  0.5, -0.5, 1.0, 0.0, 0.0,  // Vertex 3 (front-top-right)
	 0.5,  0.5,  0.5, 1.0, 0.0, 0.0,  // Vertex 7 (back-top-right)

	 0.5, -0.5, -0.5, 1.0, 0.0, 0.0,  // Vertex 2 (front-bottom-right)
	 0.5,  0.5,  0.5, 1.0, 0.0, 0.0,  // Vertex 7 (back-top-right)
	 0.5, -0.5,  0.5, 1.0, 0.0, 0.0,  // Vertex 6 (back-bottom-right)

	 // Bottom face
	 -0.5, -0.5, -0.5, 0.0, -1.0, 0.0,  // Vertex 1 (front-bottom-left)
		0.5, -0.5, -0.5, 0.0, -1.0, 0.0,  // Vertex 2 (front-bottom-right)
		0.5, -0.5,  0.5, 0.0, -1.0, 0.0,  // Vertex 6 (back-bottom-right)

	 -0.5, -0.5, -0.5, 0.0, -1.0, 0.0,  // Vertex 1 (front-bottom-left)
		0.5, -0.5,  0.5, 0.0, -1.0, 0.0,  // Vertex 6 (back-bottom-right)
	 -0.5, -0.5,  0.5, 0.0, -1.0, 0.0,  // Vertex 5 (back-bottom-left)

	 // Top face
	 -0.5,  0.5, -0.5, 0.0, 1.0, 0.0,  // Vertex 4 (front-top-left)
		0.5,  0.5, -0.5, 0.0, 1.0, 0.0,  // Vertex 3 (front-top-right)
		0.5,  0.5,  0.5, 0.0, 1.0, 0.0,  // Vertex 7 (back-top-right)

	 -0.5,  0.5, -0.5, 0.0, 1.0, 0.0,  // Vertex 4 (front-top-left)
		0.5,  0.5,  0.5, 0.0, 1.0, 0.0,  // Vertex 7 (back-top-right)
	 -0.5,  0.5,  0.5, 0.0, 1.0, 0.0  // Vertex 8 (back-top-left)
};
