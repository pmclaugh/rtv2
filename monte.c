#include "rt.h"

float rand_unit(void)
{
	return (float)rand()/(float)RAND_MAX;
}

t_float3 hemisphere(float u1, float u2)
{
	const float r = sqrt(1.0 - u1 * u1);
	const float phi = 2 * M_PI * u2;
	return (t_float3){cos(phi) * r, sin(phi) * r, u1};
}

t_float3 better_hemisphere(float u1, float u2)
{
	const float r = sqrt(u1);
    const float theta = 2 * M_PI * u2;
 
    const float x = r * cos(theta);
    const float y = r * sin(theta);
 
    return (t_float3){x, y, sqrt(fmax(0.0f, 1 - u1))};
}

t_halton setup_halton(int i, int base)
{
	t_halton hal;
	float f = 1.0 / (float)base;
	hal.inv_base = f;
	hal.value = 0.0;
	while(i > 0)
	{
		hal.value += f * (float)(i % base);
		i /= base;
		f *= hal.inv_base;
	}
	return hal;
}

float next_hal(t_halton *hal)
{
	float r = 1.0 - hal->value - 0.0000001;
	if (hal->inv_base < r)
		hal->value += hal->inv_base;
	else
	{
		float h = hal->inv_base;
		float hh;
		do {hh = h; h *= hal->inv_base;} while(h >= r);
		hal->value += hh + h - 1.0;
	}
	return hal->value;
}