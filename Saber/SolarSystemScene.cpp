#include "SolarSystemScene.h"

#include <cassert>
#include <string>
#include <vector>

#include "Camera.h"
#include "MeshRenderObject.h"
#include "OrbitSystem.h"
#include "Scene.h"

using namespace DirectX;

namespace {
	struct BodyVisual {
		enum class Shape : uint8_t { Sphere, Box, Model };

		Shape shape{ Shape::Sphere };
		std::wstring albedo{};
		std::wstring normal{};
	};

	const std::filesystem::path PlanetMesh{ L"../../Resources/StaticModels/sphere.glb" };
}

void BuildSolarSystemScene(
	Scene& scene,
	std::shared_ptr<DeviceContext> pDeviceContext,
	const std::shared_ptr<CommandList>& pCommandList,
	std::shared_ptr<Texture> pGBuffer
) {
	auto pOrbits{ std::make_shared<OrbitSystem>() };
	auto pHandles{ std::make_shared<std::vector<RenderObjectHandle>>() };

	auto addBody = [&](
		const OrbitSystem::BodyDesc& desc,
		const BodyVisual& visual
	) -> size_t {
		const size_t body{ pOrbits->AddBody(desc) };

		std::shared_ptr<RenderObject> pObject{};
		switch (visual.shape) {
		case BodyVisual::Shape::Sphere:
			pObject = TestTextureRenderObject::CreateSphere(
				pDeviceContext, pCommandList, pGBuffer, visual.albedo, visual.normal
			);
			break;
		case BodyVisual::Shape::Box:
			pObject = TestTextureRenderObject::CreateBox(
				pDeviceContext, pCommandList, pGBuffer, visual.albedo, visual.normal
			);
			break;
		case BodyVisual::Shape::Model:
			pObject = TestTextureRenderObject::CreateModelFromGLTF(
				pDeviceContext, pCommandList, PlanetMesh, pGBuffer, visual.albedo, visual.normal
			);
			break;
		}

		pHandles->push_back(scene.AddObject(RenderSubsystemType::Default, pObject));
		assert(pHandles->size() == body + 1);	// handle index must track body index

		return body;
	};

	const size_t sun{ addBody(
		{ .spinSpeed{ 0.15f }, .scale{ 3.f } },
		{ BodyVisual::Shape::Sphere, L"Kitty.dds", L"defaultNM.dds" }
	) };

	addBody(
		{
			.parent{ sun },
			.orbitRadius{ 8.f }, .orbitSpeed{ 0.55f },
			.spinSpeed{ 1.6f }, .scale{ 0.6f }
		},
		{ BodyVisual::Shape::Model, L"p1.dds", L"defaultNM.dds" }
	);

	const size_t planetWithTwoMoons{ addBody(
		{
			.parent{ sun },
			.orbitRadius{ 14.f }, .orbitSpeed{ 0.38f }, .orbitAngle{ XM_PIDIV2 },
			.spinSpeed{ 1.1f }, .scale{ 0.9f }
		},
		{ BodyVisual::Shape::Model, L"p2.dds", L"defaultNM.dds" }
	) };
	addBody(
		{
			.parent{ planetWithTwoMoons },
			.orbitRadius{ 2.f }, .orbitSpeed{ 1.3f },
			.spinSpeed{ 0.6f }, .scale{ 0.25f }
		},
		{ BodyVisual::Shape::Model, L"p2m1.dds", L"defaultNM.dds" }
	);
	addBody(
		{
			.parent{ planetWithTwoMoons },
			.orbitRadius{ 3.2f }, .orbitSpeed{ 0.85f }, .orbitAngle{ XM_PI },
			.spinSpeed{ 1.4f }, .scale{ 0.18f }
		},
		{ BodyVisual::Shape::Model, L"p2m2.dds", L"defaultNM.dds" }
	);

	const size_t planetWithRockyMoon{ addBody(
		{
			.parent{ sun },
			.orbitRadius{ 20.f }, .orbitSpeed{ 0.26f }, .orbitAngle{ XM_PI },
			.spinSpeed{ 0.75f }, .scale{ 1.1f }
		},
		{ BodyVisual::Shape::Model, L"p3.dds", L"defaultNM.dds" }
	) };
	// A bare rock rather than a world of its own: no planet texture for it.
	addBody(
		{
			.parent{ planetWithRockyMoon },
			.orbitRadius{ 2.6f }, .orbitSpeed{ 1.05f },
			.spinSpeed{ 0.5f }, .scale{ 0.3f }
		},
		{ BodyVisual::Shape::Box, L"nugget1k.dds", L"BrickNM.dds" }
	);

	const size_t outerPlanet{ addBody(
		{
			.parent{ sun },
			.orbitRadius{ 27.f }, .orbitSpeed{ 0.17f }, .orbitAngle{ XM_PI * 1.5f },
			.spinSpeed{ 1.25f }, .scale{ 0.8f }
		},
		{ BodyVisual::Shape::Model, L"p4.dds", L"defaultNM.dds" }
	) };
	addBody(
		{
			.parent{ outerPlanet },
			.orbitRadius{ 2.2f }, .orbitSpeed{ 1.5f },
			.spinSpeed{ 0.4f }, .scale{ 0.22f }
		},
		{ BodyVisual::Shape::Model, L"p4m1.dds", L"defaultNM.dds" }
	);

	// The outermost world, and the only cube-shaped one.
	addBody(
		{
			.parent{ sun },
			.orbitRadius{ 34.f }, .orbitSpeed{ 0.11f },
			.spinSpeed{ 0.45f }, .scale{ 1.f }
		},
		{ BodyVisual::Shape::Box, L"cube.dds", L"defaultNM.dds" }
	);

	addBody(
		{
			.parent{ sun }, .orbitRadius{ 30.f }, .orbitSpeed{ 0.09f },
			.orbitAngle{ 0.8f }, .spinSpeed{ 2.2f }, .scale{ 0.3f }
		},
		{ BodyVisual::Shape::Box, L"nugget1k.dds", L"defaultNM.dds" }
	);
	addBody(
		{
			.parent{ sun },
			.orbitRadius{ 31.5f }, .orbitSpeed{ 0.08f }, .orbitAngle{ XM_PI },
			.spinSpeed{ 2.6f }, .scale{ 0.25f }
		},
		{ BodyVisual::Shape::Box, L"cube.dds", L"defaultNM.dds" }
	);

	scene.SetSimulation([pOrbits, pHandles](float deltaTime, Scene& simulatedScene) {
		pOrbits->Update(deltaTime);

		for (size_t body{}; body < pHandles->size(); ++body) {
			simulatedScene.UpdateObjectMatrix(
				(*pHandles)[body],
				pOrbits->GetWorldMatrix(body)
			);
		}
	});
}
