#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <Common/Util.h>
#include <Navio2/ADC_Navio2.h>
#include <memory>

#define READ_FAILED -1

int main(int argc, char *argv[])
{
    if (check_apm()) {
        return 1;
    }
    auto adc = std::unique_ptr <ADC>{ new ADC_Navio2() };
    adc->initialize();
    float results[adc->get_channel_count()] = {0.0f};
    while (true)
    {
        for (int i = 0; i < adc->get_channel_count(); i++)
        {
            results[i] = adc->read(i);
            if (results[i] == READ_FAILED)
                return EXIT_FAILURE;
            printf("A%d: %.4fV ", i, results[i] / 1000);
        }
        printf("\n");

        usleep(500000);
    }


    return 0;
}
