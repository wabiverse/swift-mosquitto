#ifndef __MOSQUITTO_SHIM_MEMORY__SET_LIMIT_H__
#define __MOSQUITTO_SHIM_MEMORY__SET_LIMIT_H__

#include <stdio.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif
  
void memory__set_limit(size_t lim);
  
#ifdef __cplusplus
}
#endif

#endif /* __MOSQUITTO_SHIM_MEMORY__SET_LIMIT_H__ */
