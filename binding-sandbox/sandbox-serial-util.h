/*
** sandbox-serial-util.h
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

#ifndef MKXPZ_SANDBOX_SERIAL_UTIL_H
#define MKXPZ_SANDBOX_SERIAL_UTIL_H

#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <boost/type_traits/is_detected.hpp>
#include "binding-util.h"
#include "quadarray.h"

namespace mkxp_sandbox {
    struct sandbox_object_deser_info {
        sandbox_object_deser_info();
        template <typename T> sandbox_object_deser_info(T *ptr) : ptr(ptr), typenum(get_typenum<T>::value), ref_count(0), exists(true) {}
        sandbox_object_deser_info(const struct sandbox_object_deser_info &) = delete;
        sandbox_object_deser_info(struct sandbox_object_deser_info &&) noexcept;
        struct sandbox_object_deser_info &operator=(const struct sandbox_object_deser_info &) = delete;
        struct sandbox_object_deser_info &operator=(struct sandbox_object_deser_info &&) noexcept;
        ~sandbox_object_deser_info();
        wasm_size_t get_ref_count() const noexcept;
        template <typename T> bool add_ref(T *&ref) {
            if (typenum == 0) {
                typenum = get_typenum<T>::value;
            } else if (typenum != get_typenum<T>::value) {
                return false;
            }
            if (exists) {
                ref = (T *)ptr;
            } else {
                ((std::vector<void **> *)ptr)->push_back((void **)&ref);
            }
            ++ref_count;
            return true;
        }
        template <typename T> bool set_ptr(T *ptr) {
            if (typenum == 0) {
                typenum = get_typenum<T>::value;
            } else if (typenum != get_typenum<T>::value) {
                return false;
            }
            if (exists && ptr != this->ptr) {
                return false;
            }
            if (!exists) {
                for (void **ref : *(std::vector<void **> *)this->ptr) {
                    *(T **)ref = ptr;
                }
                delete (std::vector<void **> *)this->ptr;
                exists = true;
                this->ptr = ptr;
            }
        }

    private:
        // If `exists` is true, this is a pointer to the object. Otherwise, this is a `std::vector<void **>` of pointers that are waiting to point to the object.
        void *ptr;
        // The type of the object.
        wasm_size_t typenum;
        // The number of times this object is referenced by other objects.
        wasm_size_t ref_count;
        // True if the object has been deserialized, otherwise false.
        bool exists;
    };

    extern std::vector<std::tuple<const void *, wasm_size_t>> extra_objects;
    extern std::unordered_map<wasm_size_t, struct sandbox_object_deser_info> objects_deser;
    extern std::unordered_map<wasm_size_t, struct sandbox_object_deser_info> extra_objects_deser;

    template <typename T> using sandbox_serialize_member_declaration = decltype(std::declval<const T &>().sandbox_serialize(std::declval<void *&>(), std::declval<wasm_size_t &>()));
    template <typename T> using sandbox_deserialize_member_declaration = decltype(std::declval<T &>().sandbox_deserialize(std::declval<const void *&>(), std::declval<wasm_size_t &>()));

    template <typename T> typename std::enable_if<std::is_same<T, bool>::value, bool>::type sandbox_serialize(T value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_enum<T>::value, bool>::type sandbox_serialize(T value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_same<T, bool>::value && !std::is_enum<T>::value && std::is_arithmetic<T>::value, bool>::type sandbox_serialize(T value, void *&data, wasm_size_t &max_size);
    template <typename T> bool sandbox_serialize(const std::vector<T> &value, void *&data, wasm_size_t &max_size);
    template <typename T> bool sandbox_serialize(const QuadArray<T> &value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_same<T, char>::value, bool>::type sandbox_serialize(const T *value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_class<T>::value, bool>::type sandbox_serialize(const T *value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_class<T>::value && boost::is_detected<sandbox_serialize_member_declaration, T>::value, bool>::type sandbox_serialize(const T &value, void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<std::is_class<T>::value && !boost::is_detected<sandbox_serialize_member_declaration, T>::value, bool>::type sandbox_serialize(const T &value, void *&data, wasm_size_t &max_size);

    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_same<T, bool>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_enum<T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && !std::is_same<T, bool>::value && !std::is_enum<T>::value && std::is_arithmetic<T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value, bool>::type sandbox_deserialize(std::vector<T> &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value, bool>::type sandbox_deserialize(QuadArray<T> &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value, bool>::type sandbox_deserialize(T *&value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value && boost::is_detected<sandbox_deserialize_member_declaration, T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);
    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value && !boost::is_detected<sandbox_deserialize_member_declaration, T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size);

    template <typename T> struct sandbox_ptr_map {
    private:
        struct info {
            mkxp_sandbox::wasm_objkey_t key;
            bool is_extra;
        };

        static std::unordered_map<const T *, struct info> map;
        static bool is_serializing;
        static bool is_deserializing;

    public:
        static void sandbox_serialize_begin() {
            using namespace mkxp_sandbox;

            if (is_deserializing) {
                std::abort();
            }

            if (is_serializing) {
                return;
            }

            is_serializing = true;
            map.clear();
            extra_objects.clear();

            wasm_objkey_t key = 0;
            for (const auto &object : sb()->get_objects()) {
                ++key;
                if (object.typenum == get_typenum<T>::value) {
                    map.emplace((T *)object.inner.ptr, (struct info){.key = key, .is_extra = false});
                }
            }
        }

        static bool sandbox_serialize(const T *ptr, void *&data, wasm_size_t &max_size) {
            using namespace mkxp_sandbox;

            if (is_deserializing) {
                std::abort();
            }

            if (ptr == nullptr) {
                if (!mkxp_sandbox::sandbox_serialize((uint8_t)2, data, max_size)) return false;
            } else {
                const auto &it = map.find(ptr);
                if (it != map.end()) {
                    if (!mkxp_sandbox::sandbox_serialize((uint8_t)(it->second.is_extra ? 1 : 0), data, max_size)) return false;
                    if (!mkxp_sandbox::sandbox_serialize(it->second.key, data, max_size)) return false;
                } else {
                    if (!mkxp_sandbox::sandbox_serialize((uint8_t)1, data, max_size)) return false;

                    constexpr wasm_size_t typenum = get_typenum<T>::value;
                    extra_objects.emplace_back((const void *)ptr, typenum);
                    map.emplace(ptr, (struct info){.key = (wasm_objkey_t)extra_objects.size(), .is_extra = true});

                    if (!mkxp_sandbox::sandbox_serialize((wasm_objkey_t)extra_objects.size(), data, max_size)) return false;
                }
            }

            return true;
        }

        static void sandbox_serialize_end() {
            if (is_deserializing) {
                std::abort();
            }

            is_serializing = false;
            map.clear();
            extra_objects.clear();
        }

        static void sandbox_deserialize_begin() {
            using namespace mkxp_sandbox;

            if (is_serializing) {
                std::abort();
            }

            if (is_deserializing) {
                return;
            }

            is_deserializing = true;
            objects_deser.clear();
            extra_objects_deser.clear();
        }

        static bool sandbox_deserialize(T *&ref, const void *&data, wasm_size_t &max_size) {
            using namespace mkxp_sandbox;

            if (is_serializing) {
                std::abort();
            }

            uint8_t type;
            if (!mkxp_sandbox::sandbox_deserialize(type, data, max_size)) return false;
            if (type > 2) return false;

            if (type == 2) {
                ref = nullptr;
                return true;
            }

            wasm_objkey_t key;
            if (!mkxp_sandbox::sandbox_deserialize(key, data, max_size)) return false;
            auto &deser_map = type != 0 ? extra_objects_deser : objects_deser;
            const auto it = deser_map.find(key);
            if (it == deser_map.end()) {
                return deser_map.emplace(key, sandbox_object_deser_info()).first->second.add_ref(ref);
            } else {
                return it->second.add_ref(ref);
            }
        }

        static void sandbox_deserialize_end() {
            if (is_serializing) {
                std::abort();
            }

            is_deserializing = false;
            objects_deser.clear();
            extra_objects_deser.clear();
        }
    };

    template <typename T> std::unordered_map<const T *, struct sandbox_ptr_map<T>::info> sandbox_ptr_map<T>::map;
    template <typename T> bool sandbox_ptr_map<T>::is_serializing = false;
    template <typename T> bool sandbox_ptr_map<T>::is_deserializing = false;

    template <typename T> typename std::enable_if<std::is_class<T>::value, bool>::type sandbox_serialize(const T *value, void *&data, wasm_size_t &max_size) {
        return sandbox_ptr_map<T>::sandbox_serialize(value, data, max_size);
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value, bool>::type sandbox_deserialize(T *&value, const void *&data, wasm_size_t &max_size) {
        return sandbox_ptr_map<T>::sandbox_deserialize(value, data, max_size);
    }

    template <typename T> bool sandbox_serialize(const std::vector<T> &value, void *&data, wasm_size_t &max_size) {
        if (!sandbox_serialize((wasm_size_t)value.size(), data, max_size)) return false;
        for (const T &item : value) {
            if (!sandbox_serialize(item, data, max_size)) return false;
        }
        return true;
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value, bool>::type sandbox_deserialize(std::vector<T> &value, const void *&data, wasm_size_t &max_size) {
        wasm_size_t size;
        if (!sandbox_deserialize(size, data, max_size)) return false;
        value.clear();
        value.reserve(size);
        while (size > 0) {
            value.emplace_back();
            if (!sandbox_deserialize(value.back(), data, max_size)) return false;
            --size;
        }
        for (const T &item : value) {
            if (!sandbox_serialize(item, data, max_size)) return false;
        }
        return true;
    }

    template <typename T> bool sandbox_serialize(const QuadArray<T> &value, void *&data, wasm_size_t &max_size) {
        return sandbox_serialize(value.vertices, data, max_size);
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value, bool>::type sandbox_deserialize(QuadArray<T> &value, const void *&data, wasm_size_t &max_size) {
        if (!sandbox_deserialize(value.vertices, data, max_size)) return false;
        value.quadCount = value.vertices.size() / 4;
        return true;
    }

    template <typename T> typename std::enable_if<std::is_enum<T>::value, bool>::type sandbox_serialize(T value, void *&data, wasm_size_t &max_size) {
        return sandbox_serialize((int32_t)value, data, max_size);
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_enum<T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size) {
        return sandbox_deserialize((int32_t &)value, data, max_size);
    }

    template <typename T> typename std::enable_if<std::is_class<T>::value && boost::is_detected<sandbox_serialize_member_declaration, T>::value, bool>::type sandbox_serialize(const T &value, void *&data, wasm_size_t &max_size) {
        return value.sandbox_serialize(data, max_size);
    }

    template <typename T> typename std::enable_if<!std::is_const<T>::value && std::is_class<T>::value && boost::is_detected<sandbox_deserialize_member_declaration, T>::value, bool>::type sandbox_deserialize(T &value, const void *&data, wasm_size_t &max_size) {
        return value.sandbox_deserialize(data, max_size);
    }
}

#endif // MKXPZ_SANDBOX_SERIAL_UTIL_H
