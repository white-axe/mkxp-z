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

template <typename T> static typename std::enable_if<std::is_constructible<T>::value, void *>::type construct() {
    return new T;
}

template <typename T> static typename std::enable_if<!std::is_constructible<T>::value && std::is_constructible<T, Exception &>::value, void *>::type construct() {
    Exception e;
    T *obj = new T(e);
    if (e.is_ok()) {
        return obj;
    } else {
        delete obj;
        return nullptr;
    }
}

template <typename T> static void destroy(void *self) {
    if (self != nullptr) {
        delete (T *)self;
    }
}

template <typename T> static typename std::enable_if<std::is_base_of<Disposable, T>::value>::type dispose(void *self) {
    if (self != nullptr) {
        ((T *)self)->dispose();
    }
}

template <typename T> static typename std::enable_if<!std::is_base_of<Disposable, T>::value>::type dispose(void *self) {}

template <typename T> static typename std::enable_if<std::is_base_of<Disposable, T>::value, bool>::type disposed(void *self) {
    return self == nullptr || ((T *)self)->isDisposed();
}

template <typename T> static typename std::enable_if<!std::is_base_of<Disposable, T>::value, bool>::type disposed(void *self) {
    return self == nullptr;
}

template <typename T> static bool serialize(const void *self, void *&data, wasm_size_t &max_size) {
    return ((const T *)self)->sandbox_serialize(data, max_size);
}

template <typename T> static bool deserialize(void *self, const void *&data, wasm_size_t &max_size) {
    return ((T *)self)->sandbox_deserialize(data, max_size);
}

template <typename T> using deserialize_begin_declaration_with_is_new = decltype(std::declval<T *>()->sandbox_deserialize_begin(std::declval<bool>()));
template <typename T> using deserialize_begin_declaration_without_is_new = decltype(std::declval<T *>()->sandbox_deserialize_begin());

template <typename T> static typename std::enable_if<boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {
    ((T *)self)->sandbox_deserialize_begin(is_new);
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value && boost::is_detected<deserialize_begin_declaration_without_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {
    ((T *)self)->sandbox_deserialize_begin();
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_begin_declaration_with_is_new, T>::value && !boost::is_detected<deserialize_begin_declaration_without_is_new, T>::value>::type deserialize_begin(void *self, bool is_new) {}

template <typename T> using deserialize_end_declaration = decltype(std::declval<T *>()->sandbox_deserialize_end());

template <typename T> static typename std::enable_if<boost::is_detected<deserialize_end_declaration, T>::value>::type deserialize_end(void *self) {
    ((T *)self)->sandbox_deserialize_end();
}

template <typename T> static typename std::enable_if<!boost::is_detected<deserialize_end_declaration, T>::value>::type deserialize_end(void *self) {}

#define _SANDBOX_DEF_TYPENUM_TABLE_ENTRY(_r, _data, T) {construct<T>, destroy<T>, dispose<T>, disposed<T>, serialize<T>, deserialize<T>, deserialize_begin<T>, deserialize_end<T>},
extern const struct typenum_table_entry mkxp_sandbox::typenum_table[SANDBOX_NUM_TYPENUMS] = {BOOST_PP_SEQ_FOR_EACH(_SANDBOX_DEF_TYPENUM_TABLE_ENTRY, _, SANDBOX_TYPENUM_TYPES)};
extern const wasm_size_t mkxp_sandbox::typenum_table_size = SANDBOX_NUM_TYPENUMS;

std::vector<std::tuple<const void *, wasm_size_t>> mkxp_sandbox::extra_objects;
std::unordered_map<wasm_size_t, struct sandbox_object_deser_info> mkxp_sandbox::objects_deser;
std::unordered_map<wasm_size_t, struct sandbox_object_deser_info> mkxp_sandbox::extra_objects_deser;

sandbox_object_deser_info::sandbox_object_deser_info(void *ptr, wasm_size_t typenum) : ptr(ptr), typenum(typenum), ref_count(0), exists(true) {}

sandbox_object_deser_info::sandbox_object_deser_info(struct sandbox_object_deser_info &&info) noexcept : ptr(std::exchange(info.ptr, nullptr)), typenum(info.typenum), ref_count(std::exchange(info.ref_count, 1)), exists(std::exchange(info.exists, true)) {}

struct sandbox_object_deser_info &sandbox_object_deser_info::operator=(struct sandbox_object_deser_info &&info) noexcept {
    ptr = std::exchange(info.ptr, nullptr);
    typenum = info.typenum;
    ref_count = std::exchange(info.ref_count, 1);
    exists = std::exchange(info.exists, true);
    return *this;
}

sandbox_object_deser_info::~sandbox_object_deser_info() {
    if (!exists) {
        delete (std::vector<void **> *)ptr;
    }
}

wasm_size_t sandbox_object_deser_info::get_ref_count() const noexcept {
    return ref_count;
}

bool sandbox_object_deser_info::set_ptr(void *ptr, wasm_size_t typenum) {
    if (this->typenum != typenum) {
        return false;
    }
    if (exists && ptr != this->ptr) {
        return false;
    }
    if (!exists) {
        for (void **ref : *(std::vector<void **> *)this->ptr) {
            *ref = ptr;
        }
        delete (std::vector<void **> *)this->ptr;
        exists = true;
        this->ptr = ptr;
    }
    return true;
}

void *sandbox_object_deser_info::get_ptr() const {
    return exists ? ptr : nullptr;
}

wasm_size_t sandbox_object_deser_info::get_typenum() const {
    return typenum;
}

template <> bool mkxp_sandbox::sandbox_serialize(bool value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    *(uint8_t *)data = value;
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(bool &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    value = *(uint8_t *)data;
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int8_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int8_t));
    *(int8_t *)data = value;
    ADVANCE(sizeof(int8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(int8_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int8_t));
    value = *(int8_t *)data;
    ADVANCE(sizeof(int8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint8_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    *(uint8_t *)data = value;
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(uint8_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint8_t));
    value = *(uint8_t *)data;
    ADVANCE(sizeof(uint8_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int16_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int16_t));
    *(int16_t *)data = value;
    ADVANCE(sizeof(int16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(int16_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int16_t));
    value = *(int16_t *)data;
    ADVANCE(sizeof(int16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint16_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint16_t));
    *(uint16_t *)data = value;
    ADVANCE(sizeof(uint16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(uint16_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint16_t));
    value = *(uint16_t *)data;
    ADVANCE(sizeof(uint16_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int32_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int32_t));
    *(int32_t *)data = value;
    ADVANCE(sizeof(int32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(int32_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int32_t));
    value = *(int32_t *)data;
    ADVANCE(sizeof(int32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint32_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint32_t));
    *(uint32_t *)data = value;
    ADVANCE(sizeof(uint32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(uint32_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint32_t));
    value = *(uint32_t *)data;
    ADVANCE(sizeof(uint32_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(int64_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int64_t));
    *(int64_t *)data = value;
    ADVANCE(sizeof(int64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(int64_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(int64_t));
    value = *(int64_t *)data;
    ADVANCE(sizeof(int64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(uint64_t value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint64_t));
    *(uint64_t *)data = value;
    ADVANCE(sizeof(uint64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(uint64_t &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(uint64_t));
    value = *(uint64_t *)data;
    ADVANCE(sizeof(uint64_t));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(float value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(float));
    *(float *)data = value;
    ADVANCE(sizeof(float));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(float &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(float));
    value = *(float *)data;
    ADVANCE(sizeof(float));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(double value, void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(double));
    *(double *)data = value;
    ADVANCE(sizeof(double));
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(double &value, const void *&data, wasm_size_t &max_size) {
    RESERVE(sizeof(double));
    value = *(double *)data;
    ADVANCE(sizeof(double));
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const char *value, void *&data, wasm_size_t &max_size) {
    wasm_size_t size = std::strlen(value);
    if (!sandbox_serialize(size, data, max_size)) return false;
    RESERVE(size);
    std::memcpy(data, value, size);
    ADVANCE(size);
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const std::string &value, void *&data, wasm_size_t &max_size) {
    wasm_size_t size = value.length();
    if (!sandbox_serialize(size, data, max_size)) return false;
    RESERVE(size);
    std::memcpy(data, value.c_str(), size);
    ADVANCE(size);
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(std::string &value, const void *&data, wasm_size_t &max_size) {
    wasm_size_t size;
    if (!sandbox_deserialize(size, data, max_size)) return false;
    RESERVE(size);
    value.clear();
    value.resize(size);
    char *str = &value[0];
    std::memcpy(str, data, size);
    if (std::strlen(str) != size) {
        value.clear();
        return false;
    }
    ADVANCE(size);
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const NormValue &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.unNorm, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(NormValue &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize((int32_t &)value.unNorm, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec2 &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.x, data, max_size)) return false;
    if (!sandbox_serialize(value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Vec2 &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.x, data, max_size)) return false;
    if (!sandbox_deserialize(value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec4 &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.x, data, max_size)) return false;
    if (!sandbox_serialize(value.y, data, max_size)) return false;
    if (!sandbox_serialize(value.z, data, max_size)) return false;
    if (!sandbox_serialize(value.w, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Vec4 &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.x, data, max_size)) return false;
    if (!sandbox_deserialize(value.y, data, max_size)) return false;
    if (!sandbox_deserialize(value.z, data, max_size)) return false;
    if (!sandbox_deserialize(value.w, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vec2i &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.x, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Vec2i &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize((int32_t &)value.x, data, max_size)) return false;
    if (!sandbox_deserialize((int32_t &)value.y, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const IntRect &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize((int32_t)value.x, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.y, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.w, data, max_size)) return false;
    if (!sandbox_serialize((int32_t)value.h, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(IntRect &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize((int32_t &)value.x, data, max_size)) return false;
    if (!sandbox_deserialize((int32_t &)value.y, data, max_size)) return false;
    if (!sandbox_deserialize((int32_t &)value.w, data, max_size)) return false;
    if (!sandbox_deserialize((int32_t &)value.h, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Scene::Geometry &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.rect, data, max_size)) return false;
    if (!sandbox_serialize(value.orig, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Scene::Geometry &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.rect, data, max_size)) return false;
    if (!sandbox_deserialize(value.orig, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const SVertex &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.pos, data, max_size)) return false;
    if (!sandbox_serialize(value.texPos, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(SVertex &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.pos, data, max_size)) return false;
    if (!sandbox_deserialize(value.texPos, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const CVertex &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.pos, data, max_size)) return false;
    if (!sandbox_serialize(value.color, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(CVertex &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.pos, data, max_size)) return false;
    if (!sandbox_deserialize(value.color, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Vertex &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.pos, data, max_size)) return false;
    if (!sandbox_serialize(value.texPos, data, max_size)) return false;
    if (!sandbox_serialize(value.color, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Vertex &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.pos, data, max_size)) return false;
    if (!sandbox_deserialize(value.texPos, data, max_size)) return false;
    if (!sandbox_deserialize(value.color, data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Quad &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.vert[0], data, max_size)) return false;
    if (!sandbox_serialize(value.vert[1], data, max_size)) return false;
    if (!sandbox_serialize(value.vert[2], data, max_size)) return false;
    if (!sandbox_serialize(value.vert[3], data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Quad &value, const void *&data, wasm_size_t &max_size) {
    {
        Vertex old_vert = value.vert[0];
        if (!sandbox_deserialize(value.vert[0], data, max_size)) return false;
        if (value.vert[0].pos != old_vert.pos || value.vert[0].texPos != old_vert.texPos || value.vert[0].color != old_vert.color) {
            value.vboDirty = true;
        }
    }
    {
        Vertex old_vert = value.vert[1];
        if (!sandbox_deserialize(value.vert[1], data, max_size)) return false;
        if (value.vert[1].pos != old_vert.pos || value.vert[1].texPos != old_vert.texPos || value.vert[1].color != old_vert.color) {
            value.vboDirty = true;
        }
    }
    {
        Vertex old_vert = value.vert[2];
        if (!sandbox_deserialize(value.vert[2], data, max_size)) return false;
        if (value.vert[2].pos != old_vert.pos || value.vert[2].texPos != old_vert.texPos || value.vert[2].color != old_vert.color) {
            value.vboDirty = true;
        }
    }
    {
        Vertex old_vert = value.vert[3];
        if (!sandbox_deserialize(value.vert[3], data, max_size)) return false;
        if (value.vert[3].pos != old_vert.pos || value.vert[3].texPos != old_vert.texPos || value.vert[3].color != old_vert.color) {
            value.vboDirty = true;
        }
    }
    return true;
}

template <> bool mkxp_sandbox::sandbox_serialize(const Transform &value, void *&data, wasm_size_t &max_size) {
    if (!sandbox_serialize(value.getPosition(), data, max_size)) return false;
    if (!sandbox_serialize(value.getOrigin(), data, max_size)) return false;
    if (!sandbox_serialize(value.getScale(), data, max_size)) return false;
    if (!sandbox_serialize(value.getGlobalOffset(), data, max_size)) return false;
    if (!sandbox_serialize(value.getRotation(), data, max_size)) return false;
    return true;
}

template <> bool mkxp_sandbox::sandbox_deserialize(Transform &value, const void *&data, wasm_size_t &max_size) {
    if (!sandbox_deserialize(value.getPosition(), data, max_size)) return false;
    if (!sandbox_deserialize(value.getOrigin(), data, max_size)) return false;
    if (!sandbox_deserialize(value.getScale(), data, max_size)) return false;
    if (!sandbox_deserialize(value.getGlobalOffset(), data, max_size)) return false;
    float rotation;
    if (!sandbox_deserialize(rotation, data, max_size)) return false;
    value.setRotation(rotation);
    return true;
}
