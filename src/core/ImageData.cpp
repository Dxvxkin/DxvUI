#include "DxvUI/core/ImageData.h"

#include <stb_image.h>

#include <cstring>

namespace DxvUI {

ImageData ImageData::loadFromFile(const std::string& path, int desiredChannels) {
    int w = 0, h = 0, comp = 0;
    int req = desiredChannels;
    if (req != 1 && req != 3 && req != 4) req = 4;
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &comp, req);
    if (!data) {
        return ImageData{};
    }
    ImageData img;
    img.width = w;
    img.height = h;
    img.channels = req;
    size_t sz = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(req);
    img.pixels.resize(sz);
    std::memcpy(img.pixels.data(), data, sz);
    stbi_image_free(data);
    return img;
}

ImageData ImageData::loadFromMemory(const uint8_t* data, int len, int desiredChannels) {
    if (!data || len <= 0) return ImageData{};
    int w = 0, h = 0, comp = 0;
    int req = desiredChannels;
    if (req != 1 && req != 3 && req != 4) req = 4;
    stbi_uc* out = stbi_load_from_memory(data, len, &w, &h, &comp, req);
    if (!out) {
        return ImageData{};
    }
    ImageData img;
    img.width = w;
    img.height = h;
    img.channels = req;
    size_t sz = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(req);
    img.pixels.resize(sz);
    std::memcpy(img.pixels.data(), out, sz);
    stbi_image_free(out);
    return img;
}

ImageData ImageData::loadFromMemory(const std::vector<uint8_t>& data, int desiredChannels) {
    if (data.empty()) return ImageData{};
    return loadFromMemory(data.data(), static_cast<int>(data.size()), desiredChannels);
}

const char* ImageData::lastFailureReason() {
    return stbi_failure_reason();
}

} // namespace DxvUI
