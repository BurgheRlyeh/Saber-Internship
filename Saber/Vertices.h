#pragma once

#include "Headers.h"

struct VertexPosNormCl {
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f };
    DirectX::XMFLOAT3 norm{ 0.f, 0.f, 0.f };
    DirectX::XMFLOAT4 color{};
};

struct VertexPosUV {
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f };
    DirectX::XMFLOAT2 uv{ 0.f, 0.f };
};

struct VertexPosNormUV {
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f };
    DirectX::XMFLOAT3 norm{ 0.f, 0.f, 0.f };
    DirectX::XMFLOAT2 uv{ 0.f, 0.f };
};
