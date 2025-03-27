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

#include <cassert>
#include <cstdlib>
#include <vector>
#include "core.h"
#include "wasm-rt.h"

#define WASM_PAGE_SIZE ((uint64_t)65536U)

#define WASM_MIN_PAGES ((uint32_t)1536U)

extern "C" bool wasm_rt_is_initialized(void) {
    return true;
}

extern "C" WASM_RT_NO_RETURN void wasm_rt_trap(wasm_rt_trap_t error) {
    switch (error) {
        case WASM_RT_TRAP_OOB:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error 1: OOB (out-of-bounds memory access)\n");
            break;
        case WASM_RT_TRAP_INT_OVERFLOW:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error 2: INT_OVERFLOW (arithmetic overflow)\n");
            break;
        case WASM_RT_TRAP_DIV_BY_ZERO:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error 3: DIV_BY_ZERO (division by zero)\n");
            break;
        case WASM_RT_TRAP_INVALID_CONVERSION:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error 4: INVALID_CONVERSION (invalid integer cast)\n");
            break;
        case WASM_RT_TRAP_UNREACHABLE:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error 5: UNREACHABLE (hit an `unreachable' instruction in WebAssembly)\n");
            break;
        case WASM_RT_TRAP_CALL_INDIRECT:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error 6: CALL_INDIRECT (attempted to call a function pointer with the wrong call signature)\n");
            break;
        case WASM_RT_TRAP_UNCAUGHT_EXCEPTION:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error 7: UNCAUGHT_EXCEPTION (uncaught WebAssembly exception)\n");
            break;
        case WASM_RT_TRAP_EXHAUSTION:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error 8: EXHAUSTION (out of stack space)\n");
            break;
        default:
            mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox error %d (unknown error)\n", error);
            break;
    }
    std::abort();
}

extern "C" void wasm_rt_allocate_memory(wasm_rt_memory_t *memory, uint32_t initial_pages, uint32_t max_pages, bool is64) {
    if ((memory->size = (uint64_t)initial_pages * WASM_PAGE_SIZE) > SIZE_MAX) {
        throw std::bad_alloc();
    }
    memory->private_data = (uint8_t *)std::malloc(std::max((size_t)memory->size, (size_t)WASM_MIN_PAGES * (size_t)WASM_PAGE_SIZE));
    if (memory->private_data == NULL) {
        throw std::bad_alloc();
    }
#ifdef MKXPZ_BIG_ENDIAN
    memory->data = memory->private_data + std::max((size_t)memory->size, (size_t)WASM_MIN_PAGES * (size_t)WASM_PAGE_SIZE) - (size_t)memory->size;
#else
    memory->data = memory->private_data;
#endif
    memory->pages = initial_pages;
    std::memset(memory->data, 0, memory->size);
}

extern "C" uint32_t wasm_rt_grow_memory(wasm_rt_memory_t *memory, uint32_t pages) {
    uint32_t new_pages = memory->pages + pages;
    if (new_pages < memory->pages) { // Unsigned integer overflow
        return -1;
    }

    uint64_t new_size = (uint64_t)new_pages * WASM_PAGE_SIZE;
    if (new_size > SIZE_MAX) {
        return -1;
    }

    mkxp_retro::log_printf(RETRO_LOG_INFO, "VM memory grown to %u pages\n", new_pages);

    if (new_pages > WASM_MIN_PAGES) {
        mkxp_retro::log_printf(RETRO_LOG_ERROR, "Sandbox memory growth is not implemented yet\n");
        std::abort();
    }

#ifdef MKXPZ_BIG_ENDIAN
    memory->data = memory->private_data + std::max((size_t)new_size, (size_t)WASM_MIN_PAGES * (size_t)WASM_PAGE_SIZE) - (size_t)new_size;
    std::memset(memory->data, 0, new_size - memory->size);
#else
    memory->data = memory->private_data;
    std::memset(memory->data + memory->size, 0, new_size - memory->size);
#endif // MKXPZ_BIG_ENDIAN

    uint32_t old_pages = memory->pages;
    memory->pages = new_pages;
    memory->size = new_size;
    return old_pages;
}

extern "C" void wasm_rt_free_memory(wasm_rt_memory_t *memory) {
    std::free(memory->private_data);
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
