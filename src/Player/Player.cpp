#include "Player.hpp"

Player::Player() noexcept
{
	TextFormat::log("Player enter");

	// Debug raycast test
	m_raycastVAO = OGL::CreateVAO();
	glEnableVertexAttribArray(0u);
	m_raycastVBO = OGL::CreateBuffer(GL_ARRAY_BUFFER);
	glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Setup VAO and VBO for selection outline
	m_outlineVAO = OGL::CreateVAO();
	glEnableVertexAttribArray(0u);
	
	// To ensure that the block selection outline is always visible, a slightly *smaller* square outline 
	// is rendered slightly *in front* of each block face to avoid Z-fighting and block clipping

	const float front1 = 1.002f, front0 = -0.002f, small1 = 0.998f, small0 = 0.002f;
	const float cubeCoordinates[] = {
		// Top face
		small0, front1, small0,
		small1, front1, small0,
		small1, front1, small1,
		small0, front1, small1,
		// Bottom face
		small1, front0, small0,
		small0, front0, small0,
		small0, front0, small1,
		small1, front0, small1,
		// Right face
		front1, small0, small1,
		front1, small0, small0,
		front1, small1, small0,
		front1, small1, small1,
		// Left face
		front0, small0, small1,
		front0, small0, small0,
		front0, small1, small0,
		front0, small1, small1,
		// Front face
		small0, small1, front1,
		small0, small0, front1,
		small1, small0, front1,
		small1, small1, front1,
		// Back face
		small0, small1, front0,
		small0, small0, front0,
		small1, small0, front0,
		small1, small1, front0,
	};

	// VBO with line float data
	m_outlineVBO = OGL::CreateBuffer(GL_ARRAY_BUFFER);
	glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBufferStorage(GL_ARRAY_BUFFER, sizeof(cubeCoordinates), cubeCoordinates, 0u);

	// Line indices to determine positions of each line that makes up the outline cube
	const std::uint8_t outlineIndices[] = {
		0,  1,  1,  2,  2,  3,  3,  0,	// Right face
		4,  5,  5,  6,  6,  7,  7,  4,	// Left face
		8,  9,  9,  10, 10, 11, 11, 8,	// Top face	
		12, 13, 13, 14, 14, 15, 15, 12,	// Bottom face
		16, 17, 17, 18, 18, 19, 19, 16,	// Front face
		20, 21, 21, 22, 22, 23, 23, 20	// Back face
	};

	m_outlineEBO = OGL::CreateBuffer(GL_ELEMENT_ARRAY_BUFFER);
	glBufferStorage(GL_ELEMENT_ARRAY_BUFFER, sizeof(outlineIndices), outlineIndices, 0u);

	// Setup VAO for inventory
	m_inventoryVAO = OGL::CreateVAO();

	// Enable attributes 0-2: 0 = vec4, 1 = int, 2 = base vec3
	glEnableVertexAttribArray(0u);
	glEnableVertexAttribArray(1u);
	glEnableVertexAttribArray(2u);

	// Attribute divisors for instanced rendering
	glVertexAttribDivisor(0u, 1u);
	glVertexAttribDivisor(1u, 1u);

	// VBO for the inventory texture data
	m_inventoryIVBO = OGL::CreateBuffer(GL_ARRAY_BUFFER);

	// Attribute layout (location 0 and 1)
	// A0 (vec4): X, Y, W, H
	// A1 (int): Texture type - text or block (1st bit), inner texture id (other bits)
	glVertexAttribPointer(0u, 4, GL_FLOAT, GL_FALSE, sizeof(float[4]) + sizeof(std::int32_t), nullptr);
	glVertexAttribIPointer(1u, 1, GL_UNSIGNED_INT, sizeof(float[4]) + sizeof(std::int32_t), reinterpret_cast<const void*>(sizeof(float[4])));

	// Inventory quad vertex data
	const glm::vec3 quadVerticesData[4] = {
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },
		{ 1.0f, 1.0f, 0.0f },
		{ 1.0f, 0.0f, 1.0f }
	};

	// VBO for the quad vertices for inventory elements (location 2)
	m_inventoryQuadVBO = OGL::CreateBuffer(GL_ARRAY_BUFFER);
	glVertexAttribPointer(2u, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBufferStorage(GL_ARRAY_BUFFER, sizeof(quadVerticesData), quadVerticesData, 0u);

	UpdateCameraVectors();
	UpdateOffset();

	TextFormat::log("Player exit");
}

void Player::WorldInitialize() noexcept
{
	// Initialize variables that require other classes

	// Selected block information text - created in player class instead of
	// with other text objects so it can be updated only when the selected block changes
	m_blockText = world->textRenderer.CreateText(
		{}, "", 12u, 
		TextRenderer::TS_Shadow | TextRenderer::TS_Background | TextRenderer::TS_DebugText, 
		TextRenderer::TextType::Default, false
	);
	UpdateCameraDirection(); // Update yaw/pitch and raycast block which updates above text

	// Inventory item counters - hotbar slots have a mirrored one in the inventory
	for (int slotIndex = 0; slotIndex < 36; ++slotIndex) {
		PlayerObject::InventorySlot &slot = player.inventory[slotIndex];
		const std::string text = fmt::to_string(slot.count);
		bool isHotbar = slotIndex < 9;
		
		// Position is determined in UpdateInventoryText function
		slot.slotText = world->textRenderer.CreateText(
			{}, text, 10u, 
			isHotbar ? TextRenderer::TS_Shadow : (TextRenderer::TS_Shadow | TextRenderer::TS_InventoryVisible), 
			slot.count ? TextRenderer::TextType::Default : TextRenderer::TextType::Hidden, false
		);

		// Also create copy of hotbar text for inventory hotbar
		if (isHotbar) {
			m_inventoryHotbarText[slotIndex] = world->textRenderer.CreateText(
				{}, text, slot.slotText->GetUnitSize(),
				TextRenderer::TS_Shadow | TextRenderer::TS_InventoryVisible, slot.slotText->textType, false
			);
		}
	}

	UpdateInventoryTextPos(); // Update text positions
}

void Player::InventoryTextTest() noexcept
{
	for (int slotIndex = 0; slotIndex < 9; ++slotIndex) {
		PlayerObject::InventorySlot &slot = player.inventory[slotIndex];
		world->textRenderer.ChangePosition(slot.slotText, { game.testvals.x + (slotIndex * game.testvals.y), game.testvals.z });
	}
}

void Player::RaycastDebugCheck() noexcept
{
	game.shader.UseShader(Shader::ShaderID::Vec3);
	glBindVertexArray(m_raycastVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_raycastVBO);
	
	const glm::vec3 pointPositions[] = { glm::vec3(player.targetBlockPosition) + glm::vec3(0.5f) };
	glBufferData(GL_ARRAY_BUFFER, sizeof(pointPositions), pointPositions, GL_STATIC_DRAW);

	glDepthFunc(GL_ALWAYS);
	glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(Math::size(pointPositions)));
	glEnable(GL_CULL_FACE);
}

void Player::CheckInput() noexcept 
{
	// Check for any input that requires holding
	
	// Get key state
	const auto CheckKey = [](int key) {
		return glfwGetKey(game.window, key) == GLFW_PRESS;
	};

	// Only check if there is a key being pressed and if the player isn't chatting or in inventory
	if (game.anyKeyPressed && !game.chatting && !player.inventoryOpened) {
		// Movement inputs
		if (CheckKey(GLFW_KEY_W)) AddVelocity(PlayerDirection::Forwards);
		if (CheckKey(GLFW_KEY_S)) AddVelocity(PlayerDirection::Backwards);
		if (CheckKey(GLFW_KEY_A)) AddVelocity(PlayerDirection::Left);
		if (CheckKey(GLFW_KEY_D)) AddVelocity(PlayerDirection::Right);

		bool alreadyControl = false;
		if (player.doGravity) {
			// Jumping movement
			if (CheckKey(GLFW_KEY_SPACE)) AddVelocity(PlayerDirection::Jump);
		} else {
			// Flying mode inputs
			if (CheckKey(GLFW_KEY_SPACE)) AddVelocity(PlayerDirection::Fly_Up);
			if (CheckKey(GLFW_KEY_LEFT_CONTROL)) { AddVelocity(PlayerDirection::Fly_Down); alreadyControl = true; }
		}

		// Speed modifiers
		if (CheckKey(GLFW_KEY_LEFT_SHIFT)) player.currentSpeed = player.defaultSpeed * 1.5f;
		else if (!alreadyControl && CheckKey(GLFW_KEY_LEFT_CONTROL)) player.currentSpeed = player.defaultSpeed * 0.5f;
		else player.currentSpeed = player.defaultSpeed;
	}
}

void Player::ApplyMovement() noexcept
{
	// Make player slowly come to a stop instead of immediately stopping when no movement is applied
	// TODO: Framerate-independent lerp/smoothing
	const double movementLerp = glm::clamp(game.deltaTime * 10.0, 0.0, 1.0);
	
	// Gravity should have a maximum instead of increasing indefinetly (terminal velocity)
	if (player.doGravity) m_velocity.y = glm::max(m_velocity.y + player.gravity * game.deltaTime, player.maxGravity);
	else m_velocity.y = Math::lerp(m_velocity.y, 0.0, movementLerp);

	if (glm::length(m_velocity) <= DBL_EPSILON) return; // No movement checks for 0 velocity

	// Positions and axis direction list
	struct CollisionCheck {
		double x, z; bool doX, doZ;
	} static const collisionChecks[] = {
		{  0.0,  0.0,  false,  false }, // Y only
		{  0.1,  0.0,  true,   false }, // Positive X
		{ -0.1,  0.0,  true,   false }, // Negative X
		{  0.0,  0.1,  false,  true  }, // Positive Z
		{  0.0, -0.1,  false,  true  }, // Negative Z
		{ -0.1,  0.1,  true,   true  }, // Forward left
		{  0.1,  0.1,  true,   true  }, // Forward right
		{  0.1, -0.1,  true,   true  }, // Backward right
		{ -0.1, -0.1,  true,   true  }, // Backward left
	};
	
	// Reset specific velocity values if they make the player intersect with a block;
	// reset Y velocity if a block is detected below or above instead

	enum VelocityResetState { NoneReset, XReset, ZReset, XAndZ } resetState = NoneReset;
	WorldPos collidedBlockPos;

	const auto ResetVelocities = [&](const CollisionCheck &col) {
		// Only change velocity if its sign matches with the direction checked:
		// e.g. if the player moves in the +X direction, a block close 
		// enough in the -X direction should not stop their movement.
		
		// TODO: Proper velocity value after collision

		// X axis check
		if (col.doX && std::signbit(m_velocity.x) == std::signbit(col.x)) {
			m_velocity.x = 0.0;
			if (resetState == ZReset) { resetState = XAndZ; return true; } else { resetState = XReset; return false; }
		}

		// Z axis check
		if (col.doZ && std::signbit(m_velocity.z) == std::signbit(col.z)) {
			m_velocity.z = 0.0;
			if (resetState == XReset) { resetState = XAndZ; return true; } else { resetState = ZReset; return false; }
		}

		return false;
	};
	
	const auto CheckDirections = [&](const glm::dvec3 &newPos, double yPos, bool resetY = false, bool doGrounded = false) {
		bool first = true;
		for (const CollisionCheck &col : collisionChecks) {
			if (!resetY && first) { first = false; continue; } // The 'Y only' check only applies to ground and ceiling checks
			collidedBlockPos = ChunkSettings::ToWorld(newPos + glm::dvec3(col.x, yPos, col.z)); // World position of block to check
			if (!ChunkSettings::GetBlockData(world->GetBlock(collidedBlockPos)).isSolid) continue; // Check for a solid block

			// Reset Y velocity and possibly change grounded state for floor and ceiling checks
			if (resetY) {
				m_velocity.y = 0.0;
				if (doGrounded) player.grounded = true;
				break;
			}

			if (ResetVelocities(col)) break; // Stop early if either Y or (X and Z) velocities are reset
		}
	};

	const double groundPos = -1.8;
	const double legsPos = -1.6;
	const double abovePos = 0.2;

	player.grounded = false;

	// Smoothly stop movement
	m_velocity.x = Math::lerp(m_velocity.x, 0.0, movementLerp);
	m_velocity.z = Math::lerp(m_velocity.z, 0.0, movementLerp);

	// Ignore collision checks if noclip is enabled
	if (!player.noclip) {
		const glm::dvec3 newPosition = player.position + m_velocity; // The new position to check collisions for
		// All collision checks
		CheckDirections(newPosition, abovePos, true); // Blocks above player
		CheckDirections(newPosition, groundPos, true, true); // Blocks on the ground beneath
		CheckDirections(newPosition, legsPos); // Blocks around 'legs' position
		if (resetState != XAndZ) CheckDirections(newPosition, 0.0); // Blocks around camera position (only if velocity hasn't been reset already)
	}

	player.position += m_velocity; // Add new velocity to position
	PositionVariables(); // Update variables and other data affected by position
}

void Player::SetPosition(const glm::dvec3 &newPos) noexcept
{
	// Set the new position of the player
	player.position = newPos;
	PositionVariables();
}

const glm::dvec3 &Player::GetVelocity() const noexcept
{
	return m_velocity;
}

void Player::PositionVariables() noexcept
{
	player.moved = true;
	player.posMagnitude = glm::length(player.position);

	// Determine block at head position (air, water, etc) and the one the player is standing on
	const WorldPos playerBlockPos = ChunkSettings::ToWorld(player.position);
	player.headBlock = world->GetBlock(playerBlockPos);
	player.feetBlock = world->GetBlock({playerBlockPos.x, playerBlockPos.y - 2, playerBlockPos.z});

	UpdateOffset();
	RaycastBlock();
}

void Player::BreakBlock() noexcept
{
	// Check if the selected block is solid/valid
	if (ChunkSettings::GetBlockData(player.targetBlock).isSolid) {
		// Set broken block to air and redo raycast
		world->SetBlock(player.targetBlockPosition, ObjectID::Air, true);
		const int slotIndex = SearchForFreeMatchingSlot(player.targetBlock);
		if (slotIndex != -1) UpdateSlot(slotIndex, player.targetBlock, player.inventory[slotIndex].count + 1u);
		RaycastBlock();
	}
}

void Player::PlaceBlock() noexcept
{
	// Determine selected block in inventory
	PlayerObject::InventorySlot &useSlot = player.inventory[selected];
	const ObjectID placeBlock = useSlot.objectID;

	// Only place if the current selected block is valid
	if (placeBlock != ObjectID::Air && ChunkSettings::GetBlockData(player.targetBlock).isSolid) {
		const WorldPos placePosition = player.targetBlockPosition + static_cast<WorldPos>(placeBlockRelPosition);
		if (ChunkSettings::GetBlockData(world->GetBlock(placePosition)).isSolid) return; // Don't replace already existing blocks

		const WorldPos playerBlockPos = ChunkSettings::ToWorld(player.position);
		const WorldPos playerLegsBlockPos = { playerBlockPos.x, playerBlockPos.y - static_cast<PosType>(1), playerBlockPos.z };
		if (placePosition == playerBlockPos || placePosition == playerLegsBlockPos) return; // Don't place blocks inside the player

		world->SetBlock(placePosition, placeBlock, true); // Place block on side of selected block
		UpdateSlot(selected, placeBlock, useSlot.count - 1); // Update inventory slot with new count
		RaycastBlock(); // Raycast again to update selection
	}
}

void Player::AddVelocity(PlayerDirection direction) noexcept
{
	const double directionMultiplier = player.currentSpeed * game.deltaTime;

	// Add to velocity based on frame time and input
	switch (direction) {
		// Player movement
		case PlayerDirection::Forwards:
			m_velocity += m_NPcamFront * directionMultiplier;
			break;
		case PlayerDirection::Backwards:
			m_velocity -= m_NPcamFront * directionMultiplier;
			break;
		case PlayerDirection::Left:
			m_velocity -= m_NPcamRight * directionMultiplier;
			break;
		case PlayerDirection::Right:
			m_velocity += m_NPcamRight * directionMultiplier;
			break;
		// Flying movement
		case PlayerDirection::Fly_Up:
			m_velocity.y += directionMultiplier;
			break;
		case PlayerDirection::Fly_Down:
			m_velocity.y -= directionMultiplier;
			break;
		// Jumping movement
		case PlayerDirection::Jump:
			if (player.grounded) m_velocity.y = player.jumpPower;
			break;
		default:
			break;
	}
}

int Player::SearchForItem(ObjectID item, bool includeFull) noexcept
{
	// Return index of item in inventory or -1 if none was found, taking 'fullness' into account
	const int numSlots = static_cast<int>(Math::size(player.inventory));
	for (int i = 0; i < numSlots; ++i) { 
		const PlayerObject::InventorySlot &slot = player.inventory[i];
		if (slot.objectID == item) {
			if (includeFull && slot.count >= player.maxSlotCount) continue;
			return i;
		}
	}
	return -1;
}

int Player::SearchForFreeMatchingSlot(ObjectID item) noexcept
{
	// Returns the earliest air slot or non-full slot with the same object
	const int matchingSlot = SearchForItem(item, true);
	if (matchingSlot != -1) return matchingSlot;
	const int airSlot = SearchForItem(ObjectID::Air, true);
	if (airSlot != -1) return airSlot;
	return -1;
}

void Player::RenderBlockOutline() const noexcept
{
	// Only show outline when the player is looking at a breakable block (e.g. can't break water or air)
	if (!ChunkSettings::GetBlockData(player.targetBlock).isSolid) return;

	// Enable outline shader and VAO to use correct shaders and buffers
	game.shader.UseShader(Shader::ShaderID::Outline);
	glBindVertexArray(m_outlineVAO);

	// GL_LINES - each pair of indices determine the line's start and end position
	glDrawElements(GL_LINES, 48, GL_UNSIGNED_BYTE, nullptr);
}

void Player::RenderPlayerGUI() const noexcept
{
	// Bind VAO and use inventory shader
	game.shader.UseShader(Shader::ShaderID::Inventory);
	glBindVertexArray(m_inventoryVAO);
	
	if (player.inventoryOpened) {
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, m_noInventoryInstances - 1); // Draw hotbar instances (without crosshair)
		for (int i = 0; i < 9; ++i) world->textRenderer.RenderText(player.inventory[i].slotText, !i, true); // Render hotbar text under inventory background
		game.shader.UseShader(Shader::ShaderID::Inventory); // Switch back to inventory shader
		glBindVertexArray(m_inventoryVAO); // Use inventory VAO and draw rest of inventory
		glDrawArraysInstancedBaseInstance(GL_TRIANGLE_STRIP, 0, 4, m_totalInventoryInstances, 0u);
		for (int i = 0; i < 9; ++i) world->textRenderer.RenderText(m_inventoryHotbarText[i], !i, true); // Render inventory hotbar text (shader switches on first)
		for (int i = 9; i < 36; ++i) world->textRenderer.RenderText(player.inventory[i].slotText, false, true); // Render inventory text
	}
	// Draw hotbar instances 
	else glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, m_noInventoryInstances);
}

void Player::SetViewMatrix(glm::mat4 &result) const noexcept
{
	// TODO: 'lookAt' function but using reference instead of a new matrix
	result = glm::lookAt(player.position, player.position + m_camFront, m_camUp);
}

void Player::UpdateCameraDirection(double x, double y) noexcept
{
	if (player.inventoryOpened) return;

	// Looking straight up causes camera to flip, so stop just before 90 degrees
	const double not90 = 89.9999;

	// Calculate camera angles
	player.yaw += x * game.sensitivity;
	player.yaw = player.yaw > 180.0 ? (-180.0 + (player.yaw - 180.0)) : player.yaw < -180.0 ? (180.0 + (player.yaw + 180.0)) : player.yaw;

	player.pitch = glm::clamp(player.pitch + (y * game.sensitivity), -not90, not90);
	player.moved = true; // Trigger update for other camera-related variables

	// Determine camera direction
	player.lookDirectionPitch = player.pitch >= 0.00 ? WldDir_Up : WldDir_Down;
	player.lookDirectionYaw =
		(player.yaw >= -45.0 && player.yaw <  45.00) ? WldDir_Front :
		(player.yaw >= 45.00 && player.yaw <  135.0) ? WldDir_Right :
		(player.yaw >= 135.0 || player.yaw < -135.0) ? WldDir_Back  : WldDir_Left;

	UpdateCameraVectors(); // Update camera vectors
	RaycastBlock(); // Do raycasting to find the block the player is looking at
}

void Player::RaycastBlock() noexcept
{
	// DDA (Digital Differential Analyser) algorithm for raycasting:
	// Rather than moving in steps and potentially missing a block, this algorithm
	// traverses in a grid-like pattern, ensuring all blocks in the way are detected.
	// See https://lodev.org/cgtutor/raycasting.html for the source code.

	// Determine steps in each axis
	glm::dvec3 deltaRay = { 
		glm::abs(1.0 / m_camFront.x), 
		glm::abs(1.0 / m_camFront.y), 
		glm::abs(1.0 / m_camFront.z) 
	};

	// Store data about block player was previously looking at
	const WorldPos initialSelectedPosition = player.targetBlockPosition;
	const ObjectID initialSelectedBlock = player.targetBlock;

	// Initial values for raycasting
	player.targetBlockPosition = ChunkSettings::ToWorld(player.position);
	glm::dvec3 sideDistance;
	WorldPos step;

	// Determine directions and magnitude in each axis
	// X axis
	if (m_camFront.x < 0.0) {
		step.x = static_cast<PosType>(-1);
		sideDistance.x = (player.position.x - static_cast<PosType>(player.position.x)) * deltaRay.x;
	} else {
		step.x = static_cast<PosType>(1);
		sideDistance.x = (static_cast<PosType>(player.position.x) + 1.0 - player.position.x) * deltaRay.x;
	}
	// Y axis
	if (m_camFront.y < 0.0) {
		step.y = static_cast<PosType>(-1);
		sideDistance.y = (player.position.y - static_cast<PosType>(player.position.y)) * deltaRay.y;
	} else {
		step.y = static_cast<PosType>(1);
		sideDistance.y = (static_cast<PosType>(player.position.y) + 1.0 - player.position.y) * deltaRay.y;
	}
	// Z axis
	if (m_camFront.z < 0.0) {
		step.z = static_cast<PosType>(-1);
		sideDistance.z = (player.position.z - static_cast<PosType>(player.position.z)) * deltaRay.z;
	} else {
		step.z = static_cast<PosType>(1);
		sideDistance.z = (static_cast<PosType>(player.position.z) + 1.0 - player.position.z) * deltaRay.z;
	}

	enum RayAxis : int { Raycast_X, Raycast_Y, Raycast_Z };

	// Used to calculate where the 'side' of the block is depending on where the player looks at the block
	WorldPos previousTargetLocation = -game.constants.worldDirections[player.lookDirectionYaw]; // Default value

	for (int i = 0; i < 6; ++i) {
		player.targetBlock = world->GetBlock(player.targetBlockPosition); // Determine block at current raycast position
		if (ChunkSettings::GetBlockData(player.targetBlock).isSolid) break; // Stop if a solid block is in the way
		
		// Check shortest axis and move in that direction
		RayAxis shortestAxis = RayAxis::Raycast_X;
		if (sideDistance.y < sideDistance.x && sideDistance.y < sideDistance.z) shortestAxis = RayAxis::Raycast_Y;
		if (sideDistance.z < sideDistance.y && sideDistance.z < sideDistance.x) shortestAxis = RayAxis::Raycast_Z;

		previousTargetLocation = player.targetBlockPosition;

		switch (shortestAxis)
		{
			case RayAxis::Raycast_X:
				player.targetBlockPosition.x += step.x;
				sideDistance.x += deltaRay.x;
				break;
			case RayAxis::Raycast_Y:
				player.targetBlockPosition.y += step.y;
				sideDistance.y += deltaRay.y;
				break;
			case RayAxis::Raycast_Z:
				player.targetBlockPosition.z += step.z;
				sideDistance.z += deltaRay.z;
				break;
			default:
				break;
		}
	}

	// Possible: Use 'world direction' index instead of storing difference
	placeBlockRelPosition = static_cast<glm::i8vec3>(previousTargetLocation - player.targetBlockPosition); 

	// Update block text if it's a different block than previous (position or type)
	if (player.targetBlock != initialSelectedBlock || player.targetBlockPosition != initialSelectedPosition) UpdateBlockInfoText();
}

void Player::UpdateBlockInfoText() noexcept
{
	const WorldBlockData &blockData = ChunkSettings::GetBlockData(player.targetBlock); // Get block properties

	// Format string with block information text
	const WorldPos offset = {
		ChunkSettings::WorldToOffset(player.targetBlockPosition.x),
		ChunkSettings::WorldToOffset(player.targetBlockPosition.y),
		ChunkSettings::WorldToOffset(player.targetBlockPosition.z)
	};
	bool isFarAway = Math::isLargeOffset(offset);

	const std::string informationText = fmt::format(
		"Selected:\n{} {} {}{}({} {} {})\n\"{}\" (ID {})\nLight emission: {}\nStrength: {}\nSolid: {}\nTransparency: {}\nTextures: {} {} {} {} {} {}", 
		player.targetBlockPosition.x, player.targetBlockPosition.y, player.targetBlockPosition.z,
		isFarAway ? "\n" : " ", offset.x, offset.y, offset.z,
		blockData.name, blockData.id, blockData.lightEmission, blockData.strength, blockData.isSolid, blockData.hasTransparency,
		blockData.textures[0], blockData.textures[1], blockData.textures[2], blockData.textures[3], blockData.textures[4], blockData.textures[5]
	);

	// Determine the width of the information text
	const float textWidth = world->textRenderer.GetTextScreenWidth(informationText, world->textRenderer.GetUnitSizeMultiplier(m_blockText->GetUnitSize()));

	// Update the block info text
	world->textRenderer.ChangePosition(m_blockText, { 0.99f - textWidth, 0.99f }, false); // Update position to ensure text fits
	world->textRenderer.ChangeText(m_blockText, informationText); // Update text with block information
}

void Player::UpdateFrustum() noexcept
{
	// Calculate frustum values
	const double halfVside = player.farPlaneDistance * glm::tan(player.fov);
	const double halfHside = halfVside * game.aspect;
	const glm::dvec3 frontMultFar = m_camFront * player.farPlaneDistance;

	// Set each of the frustum planes of the camera
	// Near plane
	player.frustum.near = { player.position + (player.nearPlaneDistance * m_camFront), m_camFront };

	// Right and left planes
	glm::dvec3 planeCalc = m_camRight * halfHside;
	player.frustum.right = { player.position, glm::cross(frontMultFar - planeCalc, m_camUp) };
	player.frustum.left = { player.position, glm::cross(m_camUp, frontMultFar + planeCalc) };

	// Top and bottom planesw
	planeCalc = m_camUp * halfVside;
	player.frustum.top = { player.position, glm::cross(m_camRight, frontMultFar - planeCalc) };
	player.frustum.bottom = { player.position, glm::cross(frontMultFar + planeCalc, m_camRight) };
}

void Player::UpdateOffset() noexcept
{
	const WorldPos initial = player.offset; // Get initial offset for comparison
	player.offset = ChunkSettings::WorldToOffset(player.position); // Calculate new player offset

	// Check if offset changed
	if ((initial.x != player.offset.x || initial.z != player.offset.z)) { 
		world->DebugChunkBorders(false); // DEBUG - update chunk borders
		if (!game.noGeneration) world->OffsetUpdate(); // Update chunks (generation, deletion, etc)
	}
	// Also update debug chunk borders to match player Y chunk offset
	else if (initial.y != player.offset.y) world->DebugChunkBorders(false);
}

void Player::UpdateCameraVectors() noexcept
{
	// Trig values from yaw and pitch radians
	const double yawRad = glm::radians(player.yaw);
	const double pitchRad = glm::radians(player.pitch);
	const double cosPitchRad = std::cos(pitchRad);
	
	// Get front and right camera vectors
	const glm::dvec3 front = glm::normalize(glm::dvec3(std::cos(yawRad) * cosPitchRad, std::sin(pitchRad), std::sin(yawRad) * cosPitchRad));
	const glm::dvec3 crossUp = glm::dvec3(0.0, 1.0, 0.0);
	const glm::dvec3 right = glm::normalize(glm::cross(front, crossUp));

	// Update front and right camera vectors and calculate up vector using the two
	m_camUp = glm::normalize(glm::cross(right, front));
	m_camFront = static_cast<glm::dvec3>(front);
	m_camRight = static_cast<glm::dvec3>(right);

	// Set the front and right vectors excluding Y axis/pitch for movement ignoring camera pitch
	m_NPcamFront = glm::normalize(glm::dvec3(m_camFront.x, 0.0, m_camFront.z));
	m_NPcamRight = glm::normalize(glm::dvec3(m_camRight.x, 0.0, m_camRight.z));
}

void Player::UpdateScroll(float yoffset) noexcept
{
	const int newSelection = static_cast<int>(selected) + (yoffset > 0.0f ? 1 : -1);
	selected = static_cast<std::uint8_t>(Math::loopAround(newSelection, 0, 9));
	UpdateInventory();
}

void Player::UpdateScroll(int slotIndex) noexcept
{
	selected = static_cast<std::uint8_t>(slotIndex);
	UpdateInventory();
}

glm::vec2 Player::GetSlotPos(SlotType stype, int id, bool isBlock) const noexcept
{
	const float slotWidth = 0.1f, slotHeight = slotWidth * game.aspect;
	const float slotBlockWidth = slotWidth * 0.75f, slotBlockHeight = slotHeight * 0.75f;
	const float slotBlockYOff = (slotHeight - slotBlockHeight) * 0.5f;
	const float slotsStartX = -0.45f;
	const float invHotbarYPos = -0.4f;

	float yPosition;

	switch (stype)
	{
		case SlotType::Hotbar:
			yPosition = -1.0f;
			break;
		case SlotType::InvHotbar:
			yPosition = invHotbarYPos;
			break;
		case SlotType::Inventory:
			yPosition = invHotbarYPos + (slotHeight * 1.1f) + (static_cast<float>((id - 9) / 9) * slotHeight * 0.98f);
			break;
		case SlotType::SlotSize:
			return { slotWidth, slotHeight };
		case SlotType::SlotBlockSize:
			return { slotBlockWidth, slotBlockHeight };
		default:
			break;
	}

	const float xPosition = slotsStartX + (static_cast<float>(id % 9) * slotWidth * 0.98f);
	if (isBlock) return { xPosition + ((slotWidth - slotBlockWidth) * 0.5f), yPosition + slotBlockYOff };
	else return { xPosition, yPosition };
}

glm::vec4 Player::GetSlotDims(SlotType stype, int id, bool isBlock) const noexcept
{
	const glm::vec2 slotPosition = GetSlotPos(stype, id, isBlock);
	if (isBlock) return { slotPosition, GetSlotPos(SlotType::SlotBlockSize, 0, false) };
	else return { slotPosition, GetSlotPos(SlotType::SlotSize, 0, false) };
}

void Player::UpdateInventory() noexcept
{
	// Bind VAO and VBO to ensure the correct buffers are edited
	glBindVertexArray(m_inventoryVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_inventoryIVBO);
	
	// Each texture index (when data is of an inventory texture rather than a block texture)
	// is used to index into an array that determines the texture positions in the vertex shader.
	enum TextureIndex : std::uint32_t { TI_Background, TI_Unequipped, TI_Equipped, TI_Crosshair };

	// Reset inventory instances count
	m_totalInventoryInstances = 0u;

	const auto AddHotbarSlot = [&](int hotbarID, TextureIndex texture) noexcept {
		inventoryData[m_totalInventoryInstances++] = { GetSlotDims(SlotType::Hotbar, hotbarID, false), texture };

		// Also add block texture if there is a valid item in the slot - use top texture
		const int itemSlotObject = static_cast<int>(player.inventory[hotbarID].objectID);
		if (itemSlotObject != static_cast<int>(ObjectID::Air)) {
			const std::uint32_t blockTexture = ChunkSettings::GetBlockData(itemSlotObject).textures[WldDir_Up];
			inventoryData[m_totalInventoryInstances++] = { GetSlotDims(SlotType::Hotbar, hotbarID, true), blockTexture, true };
		}
	};

	// ---- Hotbar slots calculation
	for (int hotbarID = 0; hotbarID < 9; ++hotbarID) {
		if (hotbarID == selected) continue;
		AddHotbarSlot(hotbarID, TI_Unequipped);
	}

	AddHotbarSlot(selected, TI_Equipped); // Make selected slot render on top of other slots
	const int hotbarSlotsInstances = m_totalInventoryInstances;

	// ---- Crosshair data
	const float chSize = 0.025f;
	const float chWidth = chSize / game.aspect;
	inventoryData[m_totalInventoryInstances++] = { { -chWidth * 0.5f, -chSize, chWidth, chSize }, TI_Crosshair }; // Crosshair at center of screen
	m_noInventoryInstances = m_totalInventoryInstances; // Determines what instances are rendered when inventory is not opened
	
	inventoryData[m_totalInventoryInstances++] = { { -1.00f, -1.00f, 2.0f, 2.0f }, TI_Background }; // Translucent background for inventory GUI (full screen)

	// ---- Inventory hotbar slots
	// Copy hotbar into inventory, with a different Y position
	for (int hotbarId = 0; hotbarId < hotbarSlotsInstances; ++hotbarId) {
		InventoryInstance hotbarInst = inventoryData[hotbarId];
		bool isBlock = hotbarInst.second & 1u; // 1st bit of uint data determines if this instance is a block texture
		hotbarInst.dims.y = GetSlotPos(SlotType::InvHotbar, hotbarId, isBlock).y;
		inventoryData[m_totalInventoryInstances++] = hotbarInst;
	}
	
	// ---- Inventory slots
	// Hotbar slots are stored in the first 9 indexes in player inventory
	for (int inventoryID = 9; inventoryID < 36; ++inventoryID) {
		inventoryData[m_totalInventoryInstances++] = { GetSlotDims(SlotType::Inventory, inventoryID, false), TI_Unequipped };

		// Same as hotbar, check for valid slot item
		const int itemSlotObject = static_cast<int>(player.inventory[inventoryID].objectID);
		if (itemSlotObject != static_cast<int>(ObjectID::Air)) {
			const std::uint32_t blockTexture = ChunkSettings::GetBlockData(itemSlotObject).textures[WldDir_Up];
			inventoryData[m_totalInventoryInstances++] = { GetSlotDims(SlotType::Inventory, inventoryID, true), blockTexture, true };
		}
	}

	// Buffer inventory data for use in the shader - only include the filled data rather than the whole array
	glBufferData(GL_ARRAY_BUFFER, sizeof(InventoryInstance) * m_totalInventoryInstances, inventoryData, GL_STATIC_DRAW);
}

void Player::UpdateInventoryTextPos() noexcept
{
	const glm::vec2 textOffset = { 0.02f, 0.05f * game.aspect };
	
	for (int i = 0; i < 9; ++i) {
		world->textRenderer.ChangePosition(player.inventory[i].slotText, GetSlotPos(SlotType::Hotbar, i, false) + textOffset);
		world->textRenderer.ChangePosition(m_inventoryHotbarText[i], GetSlotPos(SlotType::InvHotbar, i, false) + textOffset);
	}

	for (int i = 9; i < 36; ++i) world->textRenderer.ChangePosition(player.inventory[i].slotText, GetSlotPos(SlotType::Inventory, i, false) + textOffset);
}

void Player::UpdateSlot(int slotIndex, ObjectID object, std::uint8_t count)
{
	PlayerObject::InventorySlot &slot = player.inventory[slotIndex];
	bool needsUpdate = object != slot.objectID; // Determine if inventory elements need to be updated
	bool shouldMirror = slotIndex < 9; // Hotbar text changes should be mirrored to the one in the inventory

	// Update text and slot data with arguments, hiding it if the new count is 0 or the new object is air
	if (count <= 0u || object == ObjectID::Air) {
		// Zero count or air object - reset slot data and hide counter
		slot.slotText->textType = TextRenderer::TextType::Hidden;
		slot.objectID = ObjectID::Air;
		slot.count = 0u;

		needsUpdate = true; // Update block texture to none for air
		if (shouldMirror) m_inventoryHotbarText[slotIndex]->textType = TextRenderer::TextType::Hidden;
	} else {
		const std::string newText = fmt::to_string(count);
		// Positive count and non-air object - update text and slot data
		slot.objectID = object;
		slot.count = count;
		
		world->textRenderer.ChangeText(slot.slotText, newText, true);
		slot.slotText->textType = TextRenderer::TextType::Default;

		if (shouldMirror) {
			TextRenderer::ScreenText *text = m_inventoryHotbarText[slotIndex];
			world->textRenderer.ChangeText(text, newText, true);
			text->textType = TextRenderer::TextType::Default;
		}
	}

	// Update inventory GUI to display new block texture - do after slot calculation to use new values
	if (needsUpdate) UpdateInventory();
}

Player::~Player() noexcept
{
	// List of all buffers from outline and inventory to be deleted
	const GLuint deleteBuffers[] = {
		static_cast<GLuint>(m_outlineVBO),
		static_cast<GLuint>(m_inventoryIVBO),
		static_cast<GLuint>(m_inventoryQuadVBO),
	};

	// Delete the buffers from the above array
	glDeleteBuffers(static_cast<GLsizei>(Math::size(deleteBuffers)), deleteBuffers);

	// Delete both VAOs
	const GLuint deleteVAOs[] = { m_outlineVAO, m_inventoryVAO };
	glDeleteVertexArrays(static_cast<GLsizei>(Math::size(deleteVAOs)), deleteVAOs);

	// Delete inventory instances pointer
	delete[] inventoryData;
}
