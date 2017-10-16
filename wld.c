
#include "wld.h"
#include "wld_types.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#define WLD_SIGNATURE 0x54503d02
#define WLD_VERSION1 0x00015500
#define WLD_VERSION2 0x1000c800
#define WLD_VERSION3 0x1000c801

#define wld_is_pow2_or_zero(n) ((n == 0) || (((n) & ((n) - 1)) == 0))

typedef struct {
    uint32_t signature;
    uint32_t version;
    uint32_t fragCount;
    uint32_t unknownA[2];
    uint32_t stringsLength;
    uint32_t unknownB;
} WldHeader;

typedef struct {
    int         ref;
    uint32_t    hash;
} WldNameRef;

struct WLD {
    uint32_t            count;
    uint32_t            version;
    int                 negstrlen;
    bool                isCopy;
    char*               strings;
    WldFrag**           frags;
    WldNameRef*         nameRefs;
    uint8_t*            data;
};

static void wld_process_string(const void* src, void* dst, uint32_t len)
{
    static const uint8_t hash[] = {0x95, 0x3A, 0xC5, 0x2A, 0x95, 0x7A, 0x95, 0x6A};
    uint8_t* dsrc = (uint8_t*)src;
    uint8_t* ddst = (uint8_t*)dst;
    uint32_t i;
    
    for (i = 0; i < len; i++)
    {
        ddst[i] = dsrc[i] ^ hash[i & 7];
    }
}

static uint32_t wld_hash(const char* key, uint32_t len)
{
    uint32_t h = len;
    uint32_t step = (len >> 5) + 1;
    uint32_t i;
    
    for (i = len; i >= step; i -= step)
    {
        h = h ^ ((h << 5) + (h >> 2) + (key[i - 1]));
    }
    
    return h;
}

static int wld_add_frag(WLD* wld, WldFrag* frag, int negstrlen)
{
    WldNameRef name;
    uint32_t index = wld->count;
    
    if (wld_is_pow2_or_zero(index))
    {
        uint32_t cap = (index == 0) ? 1 : index * 2;
        WldFrag** frags;
        WldNameRef* nameRefs;
        
        frags = (WldFrag**)realloc(wld->frags, cap * sizeof(WldFrag*));
        if (!frags) return WLD_OUT_OF_MEMORY;
        wld->frags = frags;
        
        nameRefs = (WldNameRef*)realloc(wld->nameRefs, cap * sizeof(WldNameRef));
        if (!nameRefs) return WLD_OUT_OF_MEMORY;
        wld->nameRefs = nameRefs;
    }
    
    name.ref = 0;
    name.hash = 0;
    
    if (frag)
    {
        int nameref = frag->nameRef;
        
        if (nameref < 0 && nameref > negstrlen)
        {
            const char* str = wld->strings - nameref;
            
            name.ref = nameref;
            name.hash = wld_hash(str, strlen(str));
        }
    }
    
    wld->frags[index] = frag;
    wld->nameRefs[index] = name;
    wld->count = index + 1;
    return WLD_OK;
}

static int wld_open_impl(WLD** wld, void* vdata, uint32_t length, int isCopy)
{
    WLD* ptr;
    uint8_t* data = (uint8_t*)vdata;
    WldHeader* h = (WldHeader*)data;
    uint32_t p = sizeof(WldHeader);
    uint32_t ver, n, i;
    int stringsLength;
    char* strings;
    int rc = WLD_CORRUPTED;
    
    ptr = (WLD*)malloc(sizeof(WLD));
    if (!ptr) return WLD_OUT_OF_MEMORY;
    
    memset(ptr, 0, sizeof(WLD));
    
    ptr->isCopy = (bool)isCopy;
    ptr->data = data;
    
    if (p > length) goto fail;
    
    if (h->signature != WLD_SIGNATURE)
        goto fail;
    
    ver = h->version;
    
    if (ver != WLD_VERSION1 && ver != WLD_VERSION2 && ver != WLD_VERSION3)
        goto fail;
    
    ptr->version = ver;
    
    ptr->strings = (char*)malloc(h->stringsLength);
    if (!ptr->strings)
    {
        rc = WLD_OUT_OF_MEMORY;
        goto fail;
    }
    
    stringsLength = -((int)h->stringsLength);
    ptr->negstrlen = stringsLength;
    strings = (char*)(data + p);
    
    p += h->stringsLength;
    if (p > length) goto fail;
    
    wld_process_string(strings, ptr->strings, h->stringsLength);
    
    if (wld_add_frag(ptr, NULL, stringsLength))
    {
        rc = WLD_OUT_OF_MEMORY;
        goto fail;
    }
    
    n = h->fragCount;
    
    for (i = 0; i < n; i++)
    {
        WldFrag* frag = (WldFrag*)(data + p);
        
        p += sizeof(WldFrag);
        
        if (p > length) goto fail;
        
        p += frag->length - sizeof(uint32_t);
        
        if (p > length) goto fail;
        
        if (wld_add_frag(ptr, frag, stringsLength))
        {
            rc = WLD_OUT_OF_MEMORY;
            goto fail;
        }
    }
    
    *wld = ptr;
    return WLD_OK;
    
fail:
    wld_close(ptr);
    return rc;
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
            buf = (uint8_t*)realloc(data, cap);
            
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

int wld_open_from_memory_no_copy(WLD** wld, void* data, uint32_t length)
{
    if (!wld || !data || !length)
        return WLD_MISUSE;
    
    return wld_open_impl(wld, data, length, 0);
}

void wld_close(WLD* wld)
{
    if (wld)
    {
        if (wld->strings)
        {
            free(wld->strings);
            wld->strings = NULL;
        }
        
        if (wld->frags)
        {
            free(wld->frags);
            wld->frags = NULL;
        }
        
        if (wld->nameRefs)
        {
            free(wld->nameRefs);
            wld->nameRefs = NULL;
        }
        
        if (wld->isCopy && wld->data)
        {
            free(wld->data);
            wld->data = NULL;
        }
        
        free(wld);
    }
}

WldFrag** wld_frags(WLD* wld, uint32_t* count)
{
    if (!wld) return NULL;
    
    if (count)
        *count = wld->count - 1;
    return wld->frags + 1;
}

static int wld_index_by_nameref(WLD* wld, int ref)
{
    WldNameRef* nameRefs = wld->nameRefs;
    int n = (int)wld->count;
    int i;
    
    for (i = 0; i < n; i++)
    {
        if (nameRefs[i].ref == ref)
            return i;
    }
    
    return -1;
}

WldFrag* wld_frag_by_ref(WLD* wld, int ref)
{
    if (wld)
    {
        if (ref >= 0)
        {
            if (ref < (int)wld->count)
                return wld->frags[ref];
        }
        else
        {
            int idx = wld_index_by_nameref(wld, ref);
            
            if (idx >= 0)
                return wld->frags[idx];
        }
    }
    
    return NULL;
}

static int wld_index_by_name(WLD* wld, const char* name)
{
    WldNameRef* nameRefs = wld->nameRefs;
    int n = (int)wld->count;
    uint32_t hash = wld_hash(name, strlen(name));
    int i;
    
    for (i = 0; i < n; i++)
    {
        WldNameRef* ref = &nameRefs[i];
        
        if (ref->hash == hash)
        {
            const char* str = wld->strings - ref->ref;
            
            if (strcmp(str, name) == 0)
                return i;
        }
    }
    
    return -1;
}

WldFrag* wld_frag_by_name(WLD* wld, const char* name)
{
    if (wld && name)
    {
        int idx = wld_index_by_name(wld, name);
        
        if (idx >= 0)
            return wld->frags[idx];
    }
    
    return NULL;
}

char* wld_string_by_frag(WLD* wld, WldFrag* frag)
{
    return wld_string_by_ref(wld, frag->nameRef);
}

char* wld_string_by_ref(WLD* wld, int ref)
{
    if (wld)
    {
        if (ref < 0 && ref > wld->negstrlen)
            return wld->strings - ref;
    }
    
    return NULL;
}
