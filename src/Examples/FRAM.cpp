#include <stdio.h>
#include <unistd.h>
#include <Common/Util.h>

int main()
{

    fprintf(stderr, "No FRAM on NAVIO2. Skipping test.\n");
    return 1;
}
