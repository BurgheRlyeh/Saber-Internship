#pragma once

#include "Headers.h"

struct VertexPositionColor {
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f };
    DirectX::XMFLOAT4 color{};
};

struct VertexPositionUV {
    DirectX::XMFLOAT3 position{ 0.f, 0.f, 0.f };
    DirectX::XMFLOAT2 uv{ 0.f, 0.f };
};
