#include "shim_memory__set_limit.h"

static size_t mem_limit = 0;
void memory__set_limit(size_t lim)
{
  mem_limit = lim;
}
