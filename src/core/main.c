#include "app.h"

void gem_app_init(int argc, char* argv[]);
void gem_app_run(void);

int main(int argc, char* argv[])
{
    gem_app_init(argc, argv);
    gem_app_run();
    return 0;
}
