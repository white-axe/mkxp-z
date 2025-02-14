/*
** wasm-rt.cpp
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

#include <cstdlib>
#include <vector>
#include "core.h"
#include "wasm-rt.h"

#define WASM_PAGE_SIZE 65536U

#define WASM_MIN_PAGES 1024U // tentative

extern "C" bool wasm_rt_is_initialized(void) {
    return true;
}

extern "C" void wasm_rt_trap(wasm_rt_trap_t error) {
    mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error %d\n", error);
    std::abort();
}

extern "C" void wasm_rt_allocate_memory(wasm_rt_memory_t *memory, uint32_t initial_pages, uint32_t max_pages, bool is64) {
    memory->data = (uint8_t *)std::malloc(std::max(initial_pages, WASM_MIN_PAGES) * WASM_PAGE_SIZE);
    if (memory->data == NULL) {
        throw std::bad_alloc();
    }
    memory->pages = initial_pages;
    memory->size = initial_pages * WASM_PAGE_SIZE;
}

extern "C" uint32_t wasm_rt_grow_memory(wasm_rt_memory_t *memory, uint32_t pages) {
    uint32_t new_pages;
    if (__builtin_add_overflow(memory->pages, pages, &new_pages)) {
        return -1;
    }
    uint8_t *new_data = new_pages <= WASM_MIN_PAGES ? memory->data : (uint8_t *)std::realloc(memory->data, new_pages * WASM_PAGE_SIZE);
    if (new_data == NULL) {
        return -1;
    }
#ifdef MKXPZ_BIG_ENDIAN
    std::memmove(new_data + pages * WASM_PAGE_SIZE, new_data, memory->size);
#endif // MKXPZ_BIG_ENDIAN
    uint32_t old_pages = memory->pages;
    memory->pages = new_pages;
    memory->size = new_pages * WASM_PAGE_SIZE;
    memory->data = new_data;
    return old_pages;
}

extern "C" void wasm_rt_free_memory(wasm_rt_memory_t *memory) {
    std::free(memory->data);
}

extern "C" void wasm_rt_allocate_funcref_table(wasm_rt_funcref_table_t *table, uint32_t elements, uint32_t max_elements) {
    table->private_data = new std::vector<wasm_rt_funcref_t>(elements);
    table->data = ((std::vector<wasm_rt_funcref_t> *)table->private_data)->data();
    table->size = elements;
}

extern "C" void wasm_rt_free_funcref_table(wasm_rt_funcref_table_t *table) {
    delete (std::vector<wasm_rt_funcref_t> *)table->private_data;
}

extern "C" uint32_t wasm_rt_push_funcref(wasm_rt_funcref_table_t *table, wasm_rt_funcref_t funcref) {
    if (table->size == (uint32_t)-1) {
        throw std::bad_alloc();
    }
    ((std::vector<wasm_rt_funcref_t> *)table->private_data)->push_back(funcref);
    table->data = ((std::vector<wasm_rt_funcref_t> *)table->private_data)->data();
    return table->size++;
}
