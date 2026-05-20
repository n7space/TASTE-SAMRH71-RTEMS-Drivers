#include "SamRH71Core.h"

#define MEGA_HZ 1000000U

uint64_t SamRH71Core_GetMainClockFrequency(void)
{
	return 50 * MEGA_HZ;
}
