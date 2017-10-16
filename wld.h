
#ifndef WLD_H
#define WLD_H

#include <stdint.h>
#include <stdio.h>

#define WLD_OK 0
#define WLD_NOT_FOUND -1
#define WLD_OUT_OF_MEMORY -2
#define WLD_FILE_ERROR -3
#define WLD_MISUSE -4
#define WLD_CORRUPTED -5

#ifdef _WIN32
# ifdef __cplusplus
#  define WLD_API extern "C" __declspec(dllexport)
# else
#  define WLD_API __declspec(dllexport)
# endif
#else
# define WLD_API extern
#endif

typedef struct WLD WLD;

struct WldFrag;

WLD_API int wld_open(WLD** wld, const char* path);
WLD_API int wld_open_from_file(WLD** wld, FILE* fp);
WLD_API int wld_open_from_memory(WLD** wld, const void* data, uint32_t length);
WLD_API int wld_open_from_memory_no_copy(WLD** wld, void* data, uint32_t length);
WLD_API void wld_close(WLD* wld);

WLD_API struct WldFrag** wld_frags(WLD* wld, uint32_t* count);
WLD_API struct WldFrag* wld_frag_by_ref(WLD* wld, int ref);
WLD_API struct WldFrag* wld_frag_by_name(WLD* wld, const char* name);
WLD_API char* wld_string_by_frag(WLD* wld, struct WldFrag* frag);
WLD_API char* wld_string_by_ref(WLD* wld, int ref);

#endif/*WLD_H*/
