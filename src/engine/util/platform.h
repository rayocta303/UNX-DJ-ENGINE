#pragma once

#ifdef __ANDROID__
#define M_RESTRICT __restrict
#else
#define M_RESTRICT
#endif
