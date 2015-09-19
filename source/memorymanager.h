#pragma once


void *secureAllocate(size_t bytesize);
int secureFree(void *memory, size_t bytesize); //NOTE memory is really a void**