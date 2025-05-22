/*
** sandbox-serial-util.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sandbox-serial-util.h"
#include "etc-internal.h"
#include "quad.h"
#include "scene.h"
#include "transform.h"
#include "vertex.h"

using namespace mkxp_sandbox;

#define RESERVE(bytes) do { \
    if (max_size < (bytes)) { \
        return false; \
    } \
} while (0)

#define ADVANCE(bytes) do { \
    data = (uint8_t *)data + (bytes); \
    max_size -= (bytes); \
} while (0)

std::vector<std::tuple<const void *, wasm_size_t>> mkxp_sandbox::extra_objects;

template <> bool mkxp_sandbox::sandbox_serialize(bool value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    *(uint8_t *)data = value;
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int8_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int8_t));
    *(int8_t *)data = value;
    ADVANCE(sizeof(int8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint8_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    *(uint8_t *)data = value;
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int16_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int16_t));
    *(int16_t *)data = value;
    ADVANCE(sizeof(int16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint16_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint16_t));
    *(uint16_t *)data = value;
    ADVANCE(sizeof(uint16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int32_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int32_t));
    *(int32_t *)data = value;
    ADVANCE(sizeof(int32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint32_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint32_t));
    *(uint32_t *)data = value;
    ADVANCE(sizeof(uint32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int64_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int64_t));
    *(int64_t *)data = value;
    ADVANCE(sizeof(int64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint64_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint64_t));
    *(uint64_t *)data = value;
    ADVANCE(sizeof(uint64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(float value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(float));
    *(float *)data = value;
    ADVANCE(sizeof(float));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(double value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(double));
    *(double *)data = value;
    ADVANCE(sizeof(double));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const std::string &value, void *&data, wasm_size_t &max_size) {
    wasm_size_t size = value.length() + 1;
    RESERVE(size);
    std::memcpy(data, value.c_str(), size);
    ADVANCE(size);
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const NormValue &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.unNorm, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec2 &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.x, data, max_size)) return false;
    if (!sandbox_serialize(value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec4 &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.x, data, max_size)) return false;
    if (!sandbox_serialize(value.y, data, max_size)) return false;
    if (!sandbox_serialize(value.z, data, max_size)) return false;
    if (!sandbox_serialize(value.w, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec2i &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.x, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const IntRect &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.x, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.y, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.w, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.h, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Scene::Geometry &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.rect, data, max_size)) return false;
    if (!sandbox_serialize(value.orig, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const SVertex &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.pos, data, max_size)) return false;
    if (!sandbox_serialize(value.texPos, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const CVertex &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.pos, data, max_size)) return false;
    if (!sandbox_serialize(value.color, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vertex &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.pos, data, max_size)) return false;
    if (!sandbox_serialize(value.texPos, data, max_size)) return false;
    if (!sandbox_serialize(value.color, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Quad &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.vert[0], data, max_size)) return false;
    if (!sandbox_serialize(value.vert[1], data, max_size)) return false;
    if (!sandbox_serialize(value.vert[2], data, max_size)) return false;
    if (!sandbox_serialize(value.vert[3], data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Transform &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.getPosition(), data, max_size)) return false;
    if (!sandbox_serialize(value.getOrigin(), data, max_size)) return false;
    if (!sandbox_serialize(value.getScale(), data, max_size)) return false;
    if (!sandbox_serialize(value.getGlobalOffset(), data, max_size)) return false;
    return true;
}
