#include "Application.hpp"

GameObject::GameObject() :
	world(player),
	player(playerFunctions.player)
{
	TextFormat::log("Application enter");
	playerFunctions.world = &world;
	callbacks.app = this;

	int width, height;
	glfwGetWindowSize(game.window, &width, &height);

	glViewport(0, 0, width, height); // Ensure viewport is updated with initial window size

	// OGL rendering settings
	glEnable(GL_PROGRAM_POINT_SIZE); // Make GL_POINTS size set in shaders instead of having some global value
	glEnable(GL_DEPTH_TEST); // Enable depth testing so objects are rendered properly
	glDepthFunc(GL_LEQUAL); // Suitable depth testing function

	glEnable(GL_BLEND); // Transparency blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Suitable blend function

	glEnable(GL_CULL_FACE); // Enable face culling
	glCullFace(GL_BACK); // The inside of objects are not seen normally so they can be ignored

	// Setup buffers for shared program data (update size to reflect shader ubo size)
	OGL::SetupUBO(m_matricesUBO, 0, sizeof(glm::mat4[4]));
	OGL::SetupUBO(m_timesUBO, 1, sizeof(float[5]));
	OGL::SetupUBO(m_coloursUBO, 2, sizeof(glm::vec4[3]));
	OGL::SetupUBO(m_positionsUBO, 3, sizeof(glm::dvec4[2]));
	OGL::SetupUBO(m_sizesUBO, 4, sizeof(float[7]));

	// Update block texture size in shader UBO
	const float sizesData[] = {
		16.0f / static_cast<float>(game.blocksTextureInfo.width),
		1.0f / static_cast<float>(game.textTextureInfo.width)
	};
	OGL::UpdateUBO(m_sizesUBO, 0, sizeof(sizesData), sizesData);

	// Aspect ratio for GUI sizing (also updates perspective matrix)
	// ***	Must do this BEFORE text creation so the text objects use the
	//		correct size multipliers without needing to be recalculated ***
	UpdateAspect();

	// Text objects creation
	TextRenderer &tr = world.textRenderer;
	const std::uint8_t bgShadowInvis = TextRenderer::TS_Background | TextRenderer::TS_Shadow | TextRenderer::TS_InventoryInvisible;

	// Combine static text for less draw calls and memory
	const std::string infofmt = fmt::format(
		"{}\n{}\nSeed: {}", 
		glfwGetVersionString(), 
		reinterpret_cast<const char*>(glGetString(GL_RENDERER)), 
		game.noiseGenerators.continentalness.seed
	);
	tr.CreateText(glm::vec2(0.01f, 0.01f), infofmt, 12u, bgShadowInvis);

	m_chatText = 	tr.CreateText(glm::vec2(0.01f, 0.49f), "", 10u, bgShadowInvis, TextRenderer::TextType::Hidden); // Chat log
	m_commandText = tr.CreateText(glm::vec2(0.01f, 0.85f), "", 12u, bgShadowInvis | TextRenderer::TS_BackgroundFullX, TextRenderer::TextType::Hidden); // Command text
	m_infoText = 	tr.CreateText(glm::vec2(0.01f, 0.18f), "", 12u, bgShadowInvis); // Reserve text object for dynamic info text

	// Initialize player text
	playerFunctions.InitializeText();

	#ifndef GAME_SINGLE_THREAD
	world.StartThreadChunkUpdate(); // Update chunks when needed on a seperate thread
	#else
	// Move player to highest block on starting position
	const PosType startingAxis = static_cast<PosType>(ChunkSettings::CHUNK_SIZE_HALF);
	const PosType highestYPosition = world.HighestBlockPosition(startingAxis, startingAxis);
	playerFunctions.SetPosition({16.5, static_cast<double>(highestYPosition) + 2.0, 16.5});
	#endif

	// Clear all image data in game struct as they are no longer needed
	game.blocksTextureInfo.data.clear();
	game.textTextureInfo.data.clear();
	game.inventoryTextureInfo.data.clear();

	TextFormat::log("Application exit");
}

void GameObject::Main()
{
	double windowTitleUpdateTime = 0.0; // The time since the last window title update
	double largestUpdateTime = 0.0; // The largest amount of time between two frames
	int avgFPSCount = 0; // Number of frames since last title update

	// Main game loop
	TextFormat::log("Main loop enter");
	game.mainLoopActive = true;
	
	while (game.mainLoopActive) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear depth and colour buffer
		glfwPollEvents(); // Poll I/O such as key and mouse events

		// Time values
		game.deltaTime = glfwGetTime() - m_lastTime;
		game.tickedDeltaTime = game.deltaTime * game.tickSpeed;
		if (game.deltaTime > largestUpdateTime) largestUpdateTime = game.deltaTime;
		game.gameTime += game.tickedDeltaTime;
		windowTitleUpdateTime += game.deltaTime;
		m_updateTime += game.deltaTime;
		m_lastTime = game.deltaTime + m_lastTime;
		avgFPSCount++;

		playerFunctions.CheckInput(); // Check for per-frame inputs
		playerFunctions.UpdateMovement(); // Apply smoothed movement using velocity and other position-related functions
		if (player.moved) MovementUpdate(); // Update matrices, frustum, etc on position change

		//if (world.threadUpdateBuffers) world.UpdateWorldBuffers(); // Update the world buffers with any updated chunks
		UpdateFrameValues(); // Update shader UBO values (day/night cycle time, colours)

		// Game rendering
		playerFunctions.RenderBlockOutline();
		world.DrawWorld();

		//m_skybox.RenderSky();
		m_skybox.RenderStars();
		m_skybox.RenderSunAndMoon();
		m_skybox.RenderClouds();

		if (game.showGUI) {
			glClear(GL_DEPTH_BUFFER_BIT); // Clear depth again to guarantee GUI draws on top
			playerFunctions.RenderPlayerGUI();
			world.textRenderer.RenderText(player.inventoryOpened);
		}

		// Window title update
		if (windowTitleUpdateTime >= 0.05f) {
			// Determine FPS values
			m_avgFPS = static_cast<int>(static_cast<double>(avgFPSCount) / windowTitleUpdateTime);
			m_lowFPS = static_cast<int>(1.0 / largestUpdateTime);
			m_nowFPS = static_cast<int>(1.0 / game.deltaTime);

			// Update displayed game info text and title window
			TextUpdate();

			// Reset values
			largestUpdateTime = game.deltaTime;
			windowTitleUpdateTime = 0.0f;
			avgFPSCount = 0;
		}

		// Swap the buffers so the game window updates with the new frame
		glfwSwapBuffers(game.window);
	}

	// Main loop has ended, exit game (no menu yet)
	game.mainLoopActive = false;
	ExitGame();
}

void GameObject::MovementUpdate()
{
	player.moved = false;
	// Update view matrix for complicated 3D maths to determine screen position of vertices in shader
	playerFunctions.SetViewMatrix(m_matrices[Matrix_View]);
	m_matrices[Matrix_World] = m_matrices[Matrix_Perspective] * m_matrices[Matrix_View];

	// Update frustum values for frustum culling
	playerFunctions.UpdateFrustum();
	// Sort the world buffers to use new frustum values
	world.SortWorldBuffers();

	// Remove translation from origin matrix so certain objects always appear around the player
	m_matrices[Matrix_Origin] = m_matrices[Matrix_Perspective] * glm::mat4(glm::mat3(m_matrices[Matrix_View]));

	// Update world matrix values in GPU shaders
	OGL::UpdateUBO(m_matricesUBO, 0, sizeof(glm::mat4[2]), m_matrices + 2);
}

void GameObject::TextUpdate()
{
	/*
		Even if the game runs with no framerate cap, there are a few things that
		are pointless to be done every frame, such as the dynamic information text;

		This would have no real benefit and could instead make it harder to read some
		values that change frequently.
	*/

	const std::string FPStext = fmt::format("{} FPS | {} AVG | {} LOW", m_nowFPS, m_avgFPS, m_lowFPS);

	// Update window title
	glfwSetWindowTitle(game.window, FPStext.c_str());

	// Update dynamic information text (shortcut variables to make it less painful)
	const glm::dvec3& pos = player.position;
	const glm::dvec3& vel = playerFunctions.GetVelocity();
	const WorldPos& off = player.offset;

	std::string infofmt = fmt::format(
		"\n{:.3f} {:.3f} {:.3f} ({} {} {})\nVel: {:.2f} {:.2f} {:.2f}\nYaw:{:.1f} Ptc:{:.1f} ({})\nFOV:{:.1f} Spd:{:.1f} Time:{:.2f}\nTris:{}",
		pos.x, pos.y, pos.z, off.x, off.y, off.z, vel.x, vel.y, vel.z,
		player.yaw, player.pitch, PlayerObject::directionText[static_cast<int>(player.lookDirectionYaw) + 1], 
		glm::degrees(player.fov), player.currentSpeed, game.gameTime,
		fmt::group_digits(world.squaresCount * 2u)
	);

	// Update text buffer with new string
	world.textRenderer.ChangeText(m_infoText, FPStext + infofmt);

	// Check the status of text objects to determine visibility
	world.textRenderer.CheckTextStatus();
	m_updateTime = 0.0; // Reset function wait timer
}

void GameObject::ExitGame()
{
	TextFormat::log("Game exit");
	world.threadUpdateBuffers = false;
	game.noGeneration = false;

	// Join any created threads as there is no longer any need for them
	for (std::thread& thread : game.standaloneThreads) thread.join();

	// All buffers (VBO, VAO, etc) are deleted in destructors, which is called at the end of the 'main' function.

	// Close the game window and terminate GLFW and the program itself
	glfwDestroyWindow(game.window);
	glfwTerminate();
}

void GameObject::UpdateAspect()
{
	// Get window dimensions as a floating point value
	const float w = static_cast<float>(game.width), h = static_cast<float>(game.height);
	// Aspect ratio of screen
	game.aspect = w / h;

	// Used for resizing GUI elements
	world.textRenderer.textWidth = 0.006f;
	world.textRenderer.textHeight = 0.07f;
	const float inventoryWidth = 1.0f, inventoryHeight = 1.0f;

	// Update the shader UBO values
	float sizesData[] = { 
		game.aspect,
		world.textRenderer.textWidth,
		world.textRenderer.textHeight,
		inventoryWidth,
		inventoryHeight
	};
	OGL::UpdateUBO(m_sizesUBO, sizeof(float[2]), sizeof(sizesData), sizesData);

	// Update text values
	world.textRenderer.characterSpacingUnits = 0.001f;
	world.textRenderer.RecalculateAllText();

	// Update player inventory GUI
	playerFunctions.UpdateInventory();

	// Update perspective matrix and other perspective-related values
	UpdatePerspective();
}

void GameObject::UpdatePerspective()
{
	// Update the perspective matrix with the current saved variables
	m_matrices[Matrix_Perspective] = glm::perspective(
		player.fov,
		static_cast<double>(game.aspect),
		player.nearPlaneDistance,
		player.farPlaneDistance
	);
	// Update other matrices on next frame to use new perspective matrix
	player.moved = true;
	// Update frustum culling plane values
	playerFunctions.UpdateFrustum();
}

void GameObject::UpdateFrameValues()
{
	// Update positions UBO
	const double gamePositions[] = {
		// relativeRaycastBlockPosition
		static_cast<double>(player.targetBlockPosition.x) - player.position.x,
		static_cast<double>(player.targetBlockPosition.y) - player.position.y,
		static_cast<double>(player.targetBlockPosition.z) - player.position.z, 1.0,
		// playerPosition
		player.position.x,
		player.position.y,
		player.position.z, 1.0
	};
	OGL::UpdateUBO(m_positionsUBO, 0, sizeof(gamePositions), gamePositions);

	// Time values for shaders
	const float crntFrame = static_cast<float>(game.gameTime);
	const float m_gameDayTime = 1.0f + sinf((crntFrame * Skybox::dayNightTimeReciprocal * glm::pi<float>()) - glm::half_pi<float>());

	const float skyLerp = m_gameDayTime * 0.5f;
	const float starAlpha = (m_gameDayTime - 0.8f) * 0.5f;

	// Partial matrix UBO update (perspective and other matrices are done on player movement)
	const glm::mat4& originMatrix = m_matrices[Matrix_Origin];
	glm::mat4 newMatrices[2] = { 
		glm::rotate(originMatrix, crntFrame * Skybox::starRotationalSpeed, glm::vec3(1.0f, 0.54f, 0.2f)),
		glm::rotate(originMatrix, crntFrame * glm::radians(1.0f) * (180.0f / Skybox::dayNightTimeSeconds), glm::vec3(-1.0f, 0.0f, 0.0f))
	};
	OGL::UpdateUBO(m_matricesUBO, sizeof(glm::mat4[2]), sizeof(newMatrices), &newMatrices);

	// Update time variables UBO
	const float gameTimes[] = {
		m_gameDayTime, // time
		1.0f - skyLerp, // fogTime
		starAlpha, // starTime
		crntFrame, // gameTime
		1.1f - (m_gameDayTime * 0.5f), // cloudsTime
	};
	OGL::UpdateUBO(m_timesUBO, 0, sizeof(gameTimes), gameTimes);

	const glm::vec3 mainSkyColour = glm::vec3(
		Math::lerp(0.45f, 0.00f, skyLerp),
		Math::lerp(0.72f, 0.00f, skyLerp),
		Math::lerp(0.98f, 0.05f, skyLerp)
	);
	
	// Paint/clear background with sky colour instead of having a
	// massive shape surrounding player (expensive fragment shader)
	glClearColor(mainSkyColour.x, mainSkyColour.y, mainSkyColour.z, 1.0f);
	const float eveningLerp = powf(1.0f - m_gameDayTime, 20.0f);
	
	const glm::vec3 skyTransitionColour = glm::vec3(1.0f, 0.45f, 0.0f);
	const glm::vec3 eveningSkyColour = glm::vec3(
		Math::lerp(mainSkyColour.x, skyTransitionColour.x, eveningLerp),
		Math::lerp(mainSkyColour.y, skyTransitionColour.y, eveningLerp),
		Math::lerp(mainSkyColour.z, skyTransitionColour.z, eveningLerp)
	);
	const float worldLight = 1.1f - (m_gameDayTime * 0.5f);

	// Update colour vectors UBO
	const float gameColours[] = {
		// mainSkyColour
		mainSkyColour.x, mainSkyColour.y, mainSkyColour.z, 1.0f,
		// eveningSkyColour
		eveningSkyColour.x, eveningSkyColour.y, eveningSkyColour.z, 1.0f,
		// worldLight
		worldLight, worldLight, worldLight, 1.0f,
	};
	OGL::UpdateUBO(m_coloursUBO, 0, sizeof(gameColours), gameColours);
}

GameObject::~GameObject()
{
	// Delete each UBO
	const GLuint deleteUBOs[] = {
		static_cast<GLuint>(m_positionsUBO),
		static_cast<GLuint>(m_matricesUBO),
		static_cast<GLuint>(m_coloursUBO),
		static_cast<GLuint>(m_timesUBO),
		static_cast<GLuint>(m_sizesUBO),
	};

	glDeleteBuffers(sizeof(deleteUBOs) / sizeof(GLuint), deleteUBOs);
}
