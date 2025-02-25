#ifndef MKXPZ_STB_IMAGE_MALLOC_H
#define MKXPZ_STB_IMAGE_MALLOC_H

#include <cstdlib>

#define STBI_MALLOC std::malloc
#define STBI_REALLOC std::realloc
#define STBI_FREE std::free

#endif // MKXPZ_STB_IMAGE_MALLOC_H
