#define _POSIX_C_SOURCE 200809L
#include "path.h"
#include "core/core.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

char* resolve_path(const char* path)
{
    const char* file;
    char* buf;
    size_t file_len;
    size_t buf_len;
    int fd;

    GEM_ASSERT(path != NULL);
    if(path[0] == '\0')
        return NULL;

    buf = malloc(sizeof(char) * GEM_PATH_MAX);
    GEM_ENSURE(buf != NULL);

    file = strrchr(path, '/');
    if(file == NULL)
        file = path;
    else if(strcmp(file, "/.") == 0)
        file += 2;
    else if(strcmp(file, "/..") == 0)
        file += 3;
    else
        file++;

    file_len = strlen(file);
    fd = -1;

    if(file != path)
    {
        fd = open(".", O_RDONLY); // Attempt to open current directory
        if(fd < 0 || fchdir(fd) < 0)
            goto clean;

        strncpy(buf, path, file - path);
        buf[file - path] = '\0';
        if(chdir(buf) < 0)
            goto clean;
    }

    if(getcwd(buf, GEM_PATH_MAX - 1 - file_len) == NULL)
        goto clean;

    buf_len = strlen(buf);
    if(strcmp(buf, "/") != 0)
    {
        buf[buf_len] = '/';
        buf_len++;
    }
    if(file_len > 0)
        strcpy(buf + buf_len, file);

    if(buf[buf_len + file_len - 1] == '/')
        buf[buf_len + file_len - 1] = '\0';

    if(fd >= 0)
    {
        if(fchdir(fd) < 0)
            goto clean;
        close(fd);
    }
    return buf;
clean:
    if(fd >= 0)
        close(fd);
    free(buf);
    return NULL;
}

char* get_cwd_path(void)
{
    char* res = malloc(GEM_PATH_MAX);
    GEM_ENSURE(res != NULL);
    if(getcwd(res, GEM_PATH_MAX) == NULL)
    {
        free(res);
        return NULL;
    }
    return res;
}
