#pragma once

#include <cstdint>

namespace DxvUI {


class ITexture  {
public:
    virtual ~ITexture() = default;


    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};


}