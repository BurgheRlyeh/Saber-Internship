#pragma once

#include "Headers.h"

struct VertexPositionColor {
    DirectX::XMFLOAT4 position{ 0.f, 0.f, 0.f, 1.f };
    DirectX::XMFLOAT4 color{};
};
