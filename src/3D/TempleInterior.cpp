/******************************************************************************
 * Copyright (c) 2018-2023 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TempleInterior.h"

#include <unordered_map>

#include <glm/gtx/euler_angles.hpp>

#include "3D/Camera.h"
#include "Common/EventManager.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/RenderingSystemTemple.h"
#include "Game.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;

using Indoors = ecs::components::TempleRoom;
const std::unordered_multimap<Indoors, std::string_view> k_TempleInteriorParts {
    {Indoors::ChallengeRoom, "challenge_l3d"},
    // {"challengelo_l3d", Indoors::ChallengeLO},
    {Indoors::ChallengeRoom, "challengedome_l3d"},
    {Indoors::ChallengeRoom, "challengefloor_l3d"},
    // {"challengefloorlo_l3d", Indoors::ChallengeFloorLO},
    {
        Indoors::CreatureCave,
        "creature_l3d",
    },
    // {"creaturelo_l3d", Indoors::CreatureCaveLO},
    {Indoors::CreatureCave, "creaturewater_l3d"},
    // {"creaturewaterlo_l3d", Indoors::CreatureCaveWaterLO},
    {Indoors::CreditsRoom, "credits_l3d"},
    // {"creditslo_l3d", Indoors::CreditsLO},
    {Indoors::CreditsRoom, "creditsdome_l3d"},
    {Indoors::CreditsRoom, "creditsfloor_l3d"},
    // {"creditsfloorlo_l3d", Indoors::CreditsFloorLO},
    {Indoors::MainRoom, "main_l3d"},
    // {"mainlo_l3d", Indoors::MainLO},
    {Indoors::MainRoom, "mainfloor_l3d"},
    // {"mainfloorlo_l3d", Indoors::MainFloorLO},
    {Indoors::MainRoom, "mainwater_l3d"},
    // {"mainwaterlo_l3d", Indoors::MainWaterLO},
    // {"movement_l3d", Indoors::Movement}, // Navmesh
    {Indoors::MultiplayerRoom, "multi_l3d"},
    // {"multilo_l3d", Indoors::MultiLO},
    {Indoors::MultiplayerRoom, "multidome_l3d"},
    {Indoors::MultiplayerRoom, "multifloor_l3d"},
    // {"multifloorlo_l3d", Indoors::MultiFloorLO},
    {Indoors::OptionsRoom, "options_l3d"},
    // {"optionslo_l3d", Indoors::OptionsLO},
    {Indoors::OptionsRoom, "optionsdome_l3d"},
    {Indoors::OptionsRoom, "optionsfloor_l3d"},
    // {"optionsfloorlo_l3d", Indoors::OptionsFloorLO},
    {Indoors::SaveGameRoom, "savegame_l3d"},
    // {"savegamelo_l3d", Indoors::SaveGameLO},
    {Indoors::SaveGameRoom, "savegamedome_l3d"},
    {Indoors::SaveGameRoom, "savegamefloor_l3d"},
    // {"savegamefloorlo_l3d", Indoors::SaveGameFloorLO},
};

void TempleInterior::Activate()
{
	if (_active)
	{
		return;
	}

	auto& registry = Locator::entitiesRegistry::value();
	auto& config = Game::Instance()->GetConfig();
	auto& camera = Game::Instance()->GetCamera();

	_playerPositionOutside = camera.GetPosition();
	_playerRotationOutside = camera.GetRotation();

	config.drawIsland = false;
	config.drawWater = false;

	// Create temple entities
	auto rotation = glm::eulerAngleY(_templeRotation.y);
	auto scale = glm::vec3(1.0f);

	auto roomType = Indoors::MainRoom;
	auto bucket = k_TempleInteriorParts.bucket(roomType);
	for (auto it = k_TempleInteriorParts.begin(bucket); it != k_TempleInteriorParts.end(bucket); it++)
	{
		auto assetName = it->second;
		auto meshId = entt::hashed_string(fmt::format("temple/interior/{}", assetName).c_str());
		auto entity = registry.Create();
		registry.Assign<ecs::components::TempleInteriorPart>(entity, roomType);
		registry.Assign<ecs::components::Transform>(entity, _templePosition, rotation, scale);
		registry.Assign<ecs::components::Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}

	Locator::rendereringSystem::emplace<ecs::systems::RenderingSystemTemple>();
	camera.SetPosition(_templePosition + glm::vec3(0.0f, 1.0f, 0.0f));
	camera.SetRotation(_templeRotation);
	_active = true;
	_currentRoom = TempleRoom::Main;
}

void TempleInterior::Deactivate()
{
	if (!_active)
	{
		return;
	}

	auto& registry = Locator::entitiesRegistry::value();
	auto& config = Game::Instance()->GetConfig();
	config.drawIsland = true;
	config.drawWater = true;
	registry.Each<const ecs::components::TempleInteriorPart>(
	    [&registry](const entt::entity entity, auto&&...) { registry.Destroy(entity); });

	auto& camera = Game::Instance()->GetCamera();
	Locator::rendereringSystem::emplace<ecs::systems::RenderingSystem>();
	camera.SetPosition(_playerPositionOutside);
	camera.SetRotation(_playerRotationOutside);
	_active = false;
}

const std::map<TempleRoom, Indoors> k_RoomComponentMap = {
    {TempleRoom::Challenge, Indoors::ChallengeRoom}, {TempleRoom::CreatureCave, Indoors::CreatureCave},
    {TempleRoom::Credits, Indoors::CreditsRoom},     {TempleRoom::Main, Indoors::MainRoom},
    {TempleRoom::Multi, Indoors::MultiplayerRoom},   {TempleRoom::Options, Indoors::OptionsRoom},
    {TempleRoom::SaveGame, Indoors::SaveGameRoom}};

void TempleInterior::ChangeRoom(TempleRoom nextRoom)
{
	auto rotation = glm::eulerAngleY(_templeRotation.y);
	auto scale = glm::vec3(1.0f);
	auto& camera = Game::Instance()->GetCamera();
	auto& registry = Locator::entitiesRegistry::value();

	// auto currentRoomComponent = k_RoomComponentMap.find(_currentRoom)->second;
	auto nextRoomComponent = k_RoomComponentMap.find(nextRoom)->second;

	registry.Each<const ecs::components::TempleInteriorPart>(
	    [&registry](const entt::entity entity, auto&&...) { registry.Destroy(entity); });
	auto bucket = k_TempleInteriorParts.bucket(nextRoomComponent);
	for (auto it = k_TempleInteriorParts.begin(bucket); it != k_TempleInteriorParts.end(bucket); it++)
	{
		auto assetName = it->second;
		auto meshId = entt::hashed_string(fmt::format("temple/interior/{}", assetName).c_str());
		auto entity = registry.Create();
		registry.Assign<ecs::components::TempleInteriorPart>(entity, nextRoomComponent);
		registry.Assign<ecs::components::Transform>(entity, _templePosition, rotation, scale);
		registry.Assign<ecs::components::Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}

	camera.SetPosition(_templePosition);
	camera.SetRotation(_templeRotation);
	_currentRoom = nextRoom;
}

TempleRoom TempleInterior::GetCurrentRoom() const
{
	return _currentRoom;
}
