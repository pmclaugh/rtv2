static float get_random(unsigned int *seed0, unsigned int *seed1) {

	/* hash the seeds using bitwise AND operations and bitshifts */
	*seed0 = 36969 * ((*seed0) & 65535) + ((*seed0) >> 16);  
	*seed1 = 18000 * ((*seed1) & 65535) + ((*seed1) >> 16);

	unsigned int ires = ((*seed0) << 16) + (*seed1);

	/* use union struct to convert int to float */
	union {
		float f;
		unsigned int ui;
	} res;

	res.ui = (ires & 0x007fffff) | 0x40000000;  /* bitwise AND, bitwise OR */
	return (res.f - 2.0f) / 2.0f;
}

void orthonormal(float3 z, float3 *x, float3 *y)
{
	//local orthonormal system
	float3 axis = fabs(z.x) > fabs(z.y) ? (float3)(0.0f, 1.0f, 0.0f) : (float3)(1.0f, 0.0f, 0.0f);
	axis = cross(axis, N);
	*x = axis;
	*y = cross(N, axis);
}

static float3 direction_uniform_hemisphere(float3 x, float3 y, float u1, float theta, float3 normal)
{
	float r = native_sqrt(1.0f - u1 * u1);
	return normalize(x * r * native_cos(theta) + y * r * native_sin(theta) + normal * u1);
}

static float3 direction_cos_hemisphere(float3 x, float3 y, float u1, float theta, float3 normal)
{
	float r = native_sqrt(u1);
	return normalize(x * r * native_cos(theta) + y * r * native_sin(theta) + normal * native_sqrt(max(0.0f, 1.0f - u1)));
}

float3 diffuse_BDRF_sample(float3 normal, int way)
{
	float3 x, y;
	orthonormal(N, &x, &y);

	float u1 = get_random(seed0, seed1);
	float u2 = get_random(seed0, seed1);
	float theta = 2.0f * PI * u2;
	if (way)
		return direction_uniform_hemisphere(x, y, u1, theta, normal);
	else
		return direction_cos_hemisphere(x, y, u1, theta, normal);
}