#include "fileio.h"
#include "core/core.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_FILE_SIZE (1ull << 25) // This is 32MiB, temporary

bool read_entire_file(const char* path, char** src, size_t* size, bool* readonly)
{
    GEM_ASSERT(path != NULL);
    GEM_ASSERT(src != NULL);

    bool success = false;
    char* buf = NULL;
    off_t total_read = 0;
    int fd;

    *src = NULL;
    fd = open(path, O_RDWR);
    if(fd < 0)
    {
        if(readonly != NULL)
            *readonly = true;
        fd = open(path, O_RDONLY);
        if(fd < 0)
            return false;
    }
    else if(readonly != NULL)
        *readonly = false;

    off_t file_size = lseek(fd, 0, SEEK_END);
    if(file_size < 0 || (size_t)file_size > MAX_FILE_SIZE)
        goto end;
    if(file_size == 0)
    {
        *size = 0;
        return true;
    }

    if(lseek(fd, 0, SEEK_SET) < 0)
        goto end;

    buf = malloc(file_size + 1);
    GEM_ENSURE(buf != NULL);

    do
    {
        ssize_t temp = read(fd, buf + total_read, file_size - total_read);
        if(temp <= 0)
            break;
        total_read += temp;
    } while(file_size > total_read);
        

    if(total_read != file_size)
    {
        free(buf);
        goto end;
    }

    buf[file_size] = '\0';
    *src = buf;
    success = true;
    if(size != NULL)
        *size = (size_t)file_size;
end:
    close(fd);
    return success;
}

bool save_buffer_as(BufNr bufnr, const char* path)
{
    Buffer* buf = buffer_get(bufnr);
    if(buf->modified == false)
        return true;
    if(path == NULL)
    {
        if(buf->filepath == NULL)
            return false;
        path = buf->filepath;
    }

    bool success = false;
    int fd;

    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if(fd == -1)
        return false;

    const PieceTree* pt = &buf->contents; 
    const PTNode* node = piece_tree_next_inorder(pt, NULL);
    size_t total_written = 0;
    while(node != NULL)
    {
        const char* buf = piece_tree_get_node_start(pt, node);
        size_t written = 0;
        do
        {
            ssize_t temp = write(fd, buf + written, node->length - written);
            if(temp <= 0)
                break;
            written += temp;
        } while(node->length > written);
        if(node->length != written)
            break;
        total_written += written;
        node = piece_tree_next_inorder(pt, node);
    }

    if(total_written == pt->size)
    {
        char c = '\n';
        write(fd, &c, 1);
        fsync(fd);
        success = true;
        buf->modified = false;
    }

    close(fd);
    return success;

}
