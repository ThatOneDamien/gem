#include "app.h"

#include <stdio.h>

void gem_app_init(const char* file_to_open);
void gem_app_run(void);

int main(int argc, char* argv[])
{
    if(argc < 1)
    {
        fprintf(stderr, "Unknown argument error, argc was less than 1.\n");
        return 1;
    }

    gem_app_init(argc > 1 ? argv[1] : NULL);
    gem_app_run();
    return 0;
}
