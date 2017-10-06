
#include "wld.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    uint32_t signature;
    uint32_t version;
    uint32_t fragCount;
    uint32_t unknownA[2];
    uint32_t stringsLength;
    uint32_t unknownB;
} WldHeader;

struct WLD {
    uint32_t    count;
    uint32_t    version;
    int         strlen;
    uint32_t    length;
    const char* strings;
    uint32_t*   hashes;
    uint8_t*    data;
};

static void wld_process_string(void* str, uint32_t len)
{
    static const uint8_t hash[] = {0x95, 0x3A, 0xC5, 0x2A, 0x95, 0x7A, 0x95, 0x6A};
    uint8_t* data = (uint8_t*)str;
    uint32_t i;
    
    for (i = 0; i < len; i++)
    {
        data[i] = tolower(data[i] ^ hash[i & 7]);
    }
}

static int wld_open_impl(WLD** wld, const void* vdata, uint32_t length, int isCopy)
{
    
}

int wld_open(WLD** wld, const char* path)
{
    FILE* fp;
    int rc;
    
    if (!wld || !path || *path == 0)
        return WLD_MISUSE;
    
    fp = fopen(path, "rb");
    if (!fp) return WLD_NOT_FOUND;
    
    rc = wld_open_from_file(wld, fp);
    fclose(fp);
    return rc;
}

int wld_open_from_file(WLD** wld, FILE* fp)
{
    uint8_t* data = NULL;
    uint32_t cap = 512;
    uint32_t len = 0;
    uint32_t read;
    
    for (;;)
    {
        if (len + 1024 > cap)
        {
            uint8_t* buf;
            
            cap *= 2;
            buf = (uint8_t*)realloc(buf, cap);
            
            if (!buf)
            {
                if (data) free(data);
                return WLD_OUT_OF_MEMORY;
            }
            
            data = buf;
        }
        
        read = fread(data + len, sizeof(uint8_t), 1024, fp);
        len += read;
        
        if (read < 1024)
        {
            if (feof(fp)) break;
            
            free(data);
            return WLD_FILE_ERROR;
        }
    }
    
    return wld_open_impl(wld, data, len, 1);
}

int wld_open_from_memory(WLD** wld, const void* vdata, uint32_t length)
{
    uint8_t* data;
    
    if (!wld || !vdata || !length)
        return WLD_MISUSE;
    
    data = (uint8_t*)malloc(length);
    if (!data) return WLD_OUT_OF_MEMORY;
    
    memcpy(data, vdata, length);
    return wld_open_impl(wld, data, length, 1);
}

int wld_open_from_memory_no_copy(WLD** wld, const void* data, uint32_t length)
{
    if (!wld || !data || !length)
        return WLD_MISUSE;
    
    return wld_open_impl(wld, data, length, 0);
}

void wld_close(WLD* wld)
{
    if (wld)
    {
        free(wld);
    }
}
