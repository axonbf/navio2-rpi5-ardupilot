/*
Provided to you by Emlid Ltd (c) 2015.
twitter.com/emlidtech || www.emlid.com || info@emlid.com

Example: Control servos connected to PWM driver onboard of Navio2 shield for Raspberry Pi.

Connect servo to Navio2's rc output and watch it work.
PWM_OUTPUT = 0 complies to channel number 1, 1 to channel number 2 and so on.
To use full range of your servo correct SERVO_MIN and SERVO_MAX according to it's specification.

To run this example navigate to the directory containing it and run following commands:
make
sudo ./Servo
*/

#include <unistd.h>
#include "Navio2/PWM.h"
#include "Navio2/RCOutput_Navio2.h"
#include "Common/Util.h"
#include <unistd.h>
#include <memory>

#define SERVO_MIN 1250 /*mS*/
#define SERVO_MAX 1750 /*mS*/

#define PWM_OUTPUT 0

int main(int argc, char *argv[])
{

        auto pwm = std::unique_ptr <RCOutput>{ new RCOutput_Navio2() };

        if (check_apm()) {
            return 1;
        }

        if (getuid()) {
            fprintf(stderr, "Not root. Please launch like this: sudo %s\n", argv[0]);
        }


        if( !(pwm->initialize(PWM_OUTPUT)) ) {
            return 1;
        }
        
	pwm->set_frequency(PWM_OUTPUT, 50);

	if ( !(pwm->enable(PWM_OUTPUT)) ) {
	    return 1;
	}

        while (true) {
            pwm->set_duty_cycle(PWM_OUTPUT, SERVO_MIN);
            sleep(1);
            pwm->set_duty_cycle(PWM_OUTPUT, SERVO_MAX);
            sleep(1);
        }

    return 0;
}
