#include "alloc.h"
#include "compiler.h"
#include "config.h"
#include "diag.h"

int
main(int argc, char **argv)
{
    jik_arena_init();
    jik_diag_init();

    JikConfig conf = jik_config_make(argc, argv);
    jik_compiler_run(conf);

    jik_arena_free();
}
