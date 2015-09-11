#pragma once
#include <stdint.h>

void* secureAllocate(uint64_t bytesize);
void secureFree(void* memory, uint64_t bytesize); //NOTE really void**