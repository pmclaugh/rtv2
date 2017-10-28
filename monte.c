#include "rt.h"

float rand_unit(void)
{
	return (float)rand() / (float)RAND_MAX;
}

float rand_unit_sin(void)
{
	return sin(M_PI * rand_unit());
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
	double f = 1.0 / (double)base;
	hal.inv_base = f;
	hal.value = 0.0;
	while(i > 0)
	{
		hal.value += f * (double)(i % base);
		i /= base;
		f *= hal.inv_base;
	}
	return hal;
}

double next_hal(t_halton *hal)
{
	double r = 1.0 - hal->value - 0.0000001;
	if (hal->inv_base < r)
		hal->value += hal->inv_base;
	else
	{
		double h = hal->inv_base;
		double hh;
		do {hh = h; h *= hal->inv_base;} while(h >= r);
		hal->value += hh + h - 1.0;
	}
	return hal->value;
}

// int main(void)
// {
// 	t_halton h1, h2;
// 	h1 = setup_halton(8500000,2);
// 	h2 = setup_halton(8500000,3);
// 	for (int i = 0; i < 10000; i++)
// 	{
// 		printf("%f, %f\n", next_hal(&h1), next_hal(&h2));
// 	}
// }