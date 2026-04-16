#ifdef ENABLE_FUSE
/* Full FUSE implementation would go here */
#include <stdio.h>
#include "fuse_ops.h"
int cougfs_fuse_main(int argc, char *argv[], const char *disk_path)
{
    (void)argc; (void)argv; (void)disk_path;
    fprintf(stderr, "FUSE stub.\n");
    return 1;
}
#else
#include <stdio.h>
#include "fuse_ops.h"
int cougfs_fuse_main(int argc, char *argv[], const char *disk_path)
{
    (void)argc; (void)argv; (void)disk_path;
    fprintf(stderr, "FUSE support not compiled. Rebuild with ENABLE_FUSE=1.\n");
    return 1;
}
#endif
