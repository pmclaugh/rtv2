#include "rt.h"

float rand_unit(void)
{
	return (float)rand()/(float)RAND_MAX;
}

t_float3 hemisphere(void)
{
	float u1 = rand_unit();
	float u2 = rand_unit();
	const float r = sqrt(1.0 - u1 * u1);
	const float phi = 2 * M_PI * u2;
	return (t_float3){cos(phi) * r, sin(phi) * r, u1};
}