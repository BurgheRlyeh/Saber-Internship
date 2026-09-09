#pragma once

#include <memory>

class CommandList;
class DeviceContext;
class Scene;
class Texture;

void BuildSolarSystemScene(
	Scene& scene,
	std::shared_ptr<DeviceContext> pDeviceContext,
	const std::shared_ptr<CommandList>& pCommandList,
	std::shared_ptr<Texture> pGBuffer
);
