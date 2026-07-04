#include <unistd.h>
#include <cstdio>
#include <cstdlib>

#include <Navio2/RCInput_Navio2.h>
#include <Common/Util.h>
#include <memory>

#define READ_FAILED -1

int main(int argc, char *argv[])
{
    if (check_apm()) {
        return 1;
    }

    auto rcin = std::unique_ptr <RCInput>{ new RCInput_Navio2() };

    rcin->initialize();

    while (true)
    {
        int period = rcin->read(2);
        if (period == READ_FAILED)
            return EXIT_FAILURE;
        printf("%d\n", period);
        
        sleep(1);
    }

    return 0;
}

