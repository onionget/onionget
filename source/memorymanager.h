#pragma once
#include <stdint.h>

void *secureAllocate(uint64_t bytesize);
int secureFree(void *memory, uint64_t bytesize); //NOTE memory is really a void**