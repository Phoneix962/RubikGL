#include <cube.hpp>

Cube::Cube(unsigned int size) : size(size), shader(VSHADER_PATH, FSHADER_PATH)
{
	float offset = (size - 1) / 2.0f;

	for (int i = 0; i < size; i++)
	{
		for (int j = 0; j < size; j++)
		{
			for (int k = 0; k < size; k++)
			{	
				if (i != size - 1 && i != 0 && j != size - 1 && j != 0
					&& k != size - 1 && k != 0)
					continue;

				float scale = 1.5f / size;
				glm::vec3 pos = glm::vec3(i - offset, j - offset, k - offset);

				Piece* piece = new Piece(pos, scale, size);
				pieces.push_back(piece);
			}
		}
	}
	load_texture();
}

Cube::~Cube()
{
	for (Piece* piece : pieces)
	{
		piece->cleanup();
		delete piece;
	}
	pieces.clear();
}

void Cube::draw(SETTINGS settings, GLfloat deltaTime)
{
	for (Piece* piece : pieces)
	{
		shader.use();
		glUniform1i(glGetUniformLocation(shader.ID, "texture1"), 0);
		piece->draw(shader, texture, settings.rotationAngle, settings.zoom, settings.flipAngle);
	}

	if (rotating) {
		update_face_rotation(deltaTime);
	}
}

std::vector<Piece*> Cube::get_face_pieces(int faceIndex)
{
	std::vector<Piece*> facePieces;
	float offset = (size - 1) / 2.0f;

	for (Piece* piece : pieces)
	{
		// cols rotation
		if (piece->get_pos().x + offset == faceIndex && rotationDir == col)
		{
			facePieces.push_back(piece);
		}

		// lines rotation
		if (piece->get_pos().y + offset == faceIndex && rotationDir == line)
		{
			facePieces.push_back(piece);
		}

		// faces rotation
		if (piece->get_pos().z + offset == faceIndex && rotationDir == face)
		{
			facePieces.push_back(piece);
		}
	}

	return facePieces;
}

void Cube::rotate_face(int faceIndex, bool contrary, RotateDirection dir)
{
	if (rotating) return;

	float angle = 90.0f, duration = 0.0f;
	scrambling ? duration = SCRAMBLE_ROTATION_DURATION : duration = ROTATION_DURATION;
	if (dir == face) angle *= -1;
	if (!contrary) angle *= -1;

	rotatingFaceIndex = faceIndex;
	rotationSpeed = angle / duration;
	totalRotationAngle = angle;
	rotationDir = dir;

	rotatingFacePieces = get_face_pieces(faceIndex);

	rotating = true;
	currentRotationAngle = 0.0f;	
}

void Cube::load_texture()
{
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true);

	unsigned char* data = stbi_load(TEXT_PATH, &width, &height, &nrChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
	{
		std::cout << "Failed to load texture" << std::endl;
	}
	stbi_image_free(data);
}

void Cube::update_face_rotation(GLfloat deltaTime) {
	if (!rotating) return;

	float angleStep = rotationSpeed * deltaTime;
	currentRotationAngle += angleStep;

	glm::vec3 rotVec;
	if (rotationDir == line) rotVec = glm::vec3(0.0f, angleStep, 0.0f);
	if (rotationDir == col) rotVec = glm::vec3(angleStep, 0.0f, 0.0f);
	if (rotationDir == face) rotVec = glm::vec3(0.0f, 0.0f, angleStep);

	// Stop condition
	if ((angleStep >= 0 && currentRotationAngle >= totalRotationAngle) ||
		(angleStep <= 0 && currentRotationAngle <= totalRotationAngle))
	{
		angleStep -= (currentRotationAngle - totalRotationAngle);
		if (rotationDir == line) rotVec = glm::vec3(0.0f, angleStep, 0.0f);
		if (rotationDir == col) rotVec = glm::vec3(angleStep, 0.0f, 0.0f);
		if (rotationDir == face) rotVec = glm::vec3(0.0f, 0.0f, angleStep);
		rotating = false;

		for (Piece* piece : rotatingFacePieces)
		{
			if (size % 2 == 0)
				piece->set_pos(roundToNearestHalf(piece->get_pos()));
			else
				piece->set_pos(round(piece->get_pos()));

			piece->update_rotation(rotVec);
		}

		// 1. Handle Scramble Sequence (Priority 1)
		if (rotParams.empty()) scrambling = false;

		if (scrambling)
		{
			rotate_face(rotParams[0].faceIndex, rotParams[0].contrary, rotParams[0].dir);
			rotParams.erase(rotParams.begin());
		}
		// 2. Handle Manual Input Queue (Priority 2)
		else if (!moveQueue.empty())
		{
			RotationParams next = moveQueue.front();
			moveQueue.pop();
			rotate_face(next.faceIndex, next.contrary, next.dir);
		}

		return;
	}

	for (Piece* piece : rotatingFacePieces)
	{
		glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(angleStep), rotVec / angleStep);
		piece->set_pos(glm::vec3(rotation * glm::vec4(piece->get_pos(), 1.0f)));
		piece->update_rotation(rotVec);
	}
}

void Cube::scramble()
{
	std::random_device rd;
	std::mt19937 gen(rd());

	std::uniform_int_distribution<> faceIndexDist(0, size - 1);
	std::uniform_int_distribution<> boolDist(0, 1);
	std::uniform_int_distribution<> dirDist(0, 2);

	scrambling = true;

	for (int i = 0; i < 15 * (size - 1); i++) {
		RotationParams r;

		r.faceIndex = faceIndexDist(gen);
		r.contrary = boolDist(gen) == 0 ? true : false;

		switch (dirDist(gen)) {
		case 0:
			r.dir = col;
			break;
		case 1:
			r.dir = line;
			break;
		case 2:
			r.dir = face;
			break;
		}

		rotParams.push_back(r);
	}

	rotate_face(rotParams[0].faceIndex, rotParams[0].contrary, rotParams[0].dir);
}

std::vector<std::string> splitBySpace(const std::string& text) {
	std::stringstream ss(text);
	std::string word;
	std::vector<std::string> words;

	while (std::getline(ss, word, ' ')) { // Splits by comma
		words.push_back(word);
	}

	return words;
}

void Cube::algInput(std::string alg)
{
	std::vector<std::string> moves = splitBySpace(alg);

	for (const std::string& move : moves)
	{
		if (move.empty()) continue;

		bool isInverse = false;
		int repetitions = 1;
		std::string baseMove = move;

		// 1. Detect ' (Inverse)
		if (baseMove.back() == '\'') {
			isInverse = true;
			baseMove.pop_back();
		}
		// 2. Detect 2 (Double Move)
		if (baseMove.back() == '2') {
			repetitions = 2;
			baseMove.pop_back();
		}

		// Determine face parameters
		int fIndex = 0;
		RotateDirection direction;

		// Map letters to your specific Enums/Indices
		if (baseMove == "L") { fIndex = 0; direction = col; }
		else if (baseMove == "R") { fIndex = 2; direction = col; }
		else if (baseMove == "U") { fIndex = 2; direction = line; }
		else if (baseMove == "D") { fIndex = 0; direction = line; }
		else if (baseMove == "F") { fIndex = 2; direction = face; }
		else if (baseMove == "B") { fIndex = 0; direction = face; }
		else { continue; } // Skip invalid characters

		// 3. Push to Queue (1 or 2 times)
		for (int i = 0; i < repetitions; i++)
		{
			RotationParams r;
			r.faceIndex = fIndex;
			r.dir = direction;

			// Note: L, D, F usually need !inverse relative to R, U, B in standard implementations.
			// I kept your original logic here:
			if (baseMove == "L" || baseMove == "D" || baseMove == "F")
				r.contrary = !isInverse;
			else
				r.contrary = isInverse;

			this->moveQueue.push(r);
		}
	}

	// If we aren't currently animating, start the first move immediately!
	if (!rotating && !moveQueue.empty()) {
		RotationParams next = moveQueue.front();
		moveQueue.pop();
		rotate_face(next.faceIndex, next.contrary, next.dir);
	}
}

glm::vec3 roundToNearestHalf(glm::vec3 vec) {
	float x = round(vec.x * 2.0) / 2.0;
	float y = round(vec.y * 2.0) / 2.0;
	float z = round(vec.z * 2.0) / 2.0;

	return glm::vec3(x, y, z);
}