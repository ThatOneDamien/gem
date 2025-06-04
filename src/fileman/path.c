#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE   1
#define _DEFAULT_SOURCE 1
#include "path.h"
#include "core/core.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

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

static inline char tolow(char c)
{
    return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int dir_cmp(const void* ent1, const void* ent2)
{
    const DirEntry* e1 = (const DirEntry*)ent1;
    const DirEntry* e2 = (const DirEntry*)ent2;
    uint32_t type1 = e1->stats.st_mode & S_IFMT;
    uint32_t type2 = e2->stats.st_mode & S_IFMT;
    if((type1 == S_IFREG || type2 == S_IFREG) &&
       (type1 != type2))
        return (type1 == S_IFREG) - (type2 == S_IFREG);

    const char* a = e1->name;
    const char* b = e2->name;
    while(*a && (tolow(*a) == tolow(*b)))
    {
        a++;
        b++;
    }
    return tolow(*a) - tolow(*b);
}

void scan_bufwin_dir(BufferWin* bufwin)
{
    EntryDA* entries = &bufwin->dir_entries;
    entries->size = 0;
    int fd = open(bufwin->local_dir ? bufwin->local_dir : ".", O_RDONLY);
    if(fd < 0)
        return;

    DIR* dir = fdopendir(fd);
    if(dir == NULL)
        return;

    struct dirent* dire;
    while((dire = readdir(dir)) != NULL)
    {
        da_reserve(entries, entries->size + 1);
        DirEntry* e = entries->data + entries->size;
        struct stat* s = &entries->data[entries->size].stats;

        fstatat(fd, dire->d_name, s, 0);
        uint32_t type = s->st_mode & S_IFMT;
        if((dire->d_name[0] != '.' || dire->d_name[1] != '\0') &&
           (type == S_IFREG || type == S_IFDIR))
        {
            int len = strlen(dire->d_name);
            memcpy(e->name, dire->d_name, len + 1);
            if(len > entries->largest_name)
                entries->largest_name = len;
            entries->size++;
        }
    }

    qsort(entries->data, entries->size, sizeof(DirEntry), dir_cmp);
    closedir(dir);
}

