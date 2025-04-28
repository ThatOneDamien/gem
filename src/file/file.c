#include "file.h"
#include "core/core.h"

bool gem_read_entire_file(const char* path, char** src, size_t* size)
{
    GEM_ASSERT(path != NULL);
    GEM_ASSERT(src != NULL);
    GEM_ASSERT(size != NULL);

    bool success = false;
    char* temp = NULL;
    *src = NULL;

    FILE* fp = fopen(path, "r");
    if(fp == NULL)
        return false;

    if(fseek(fp, 0, SEEK_END) != 0)
        goto end;

    long pos;
    if((pos = ftell(fp)) < 0)
        goto end;
    *size = (size_t)pos;

    if(fseek(fp, 0, SEEK_SET) != 0)
        goto end;

    temp = malloc(*size + 1);
    GEM_ENSURE(temp != NULL);

    if(fread(temp, 1, *size, fp) != *size)
    {
        free(temp);
        goto end;
    }

    temp[*size] = '\0';
    *src = temp;
    success = true;

end:
    fclose(fp);
    return success;
}
