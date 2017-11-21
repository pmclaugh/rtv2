#include "rt.h"

//various vector functions

void print_vec(const cl_float3 vec)
{
	printf("%f %f %f\n", vec.x, vec.y, vec.z);
}

void print_3x3(const t_3x3 mat)
{
	print_vec(mat.row1);
	print_vec(mat.row2);
	print_vec(mat.row3);
}

void print_ray(const t_ray ray)
{
	printf("ray from:\n");
	print_vec(ray.origin);
	printf("with direction\n");
	print_vec(ray.direction);
}

float vec_mag(const cl_float3 vec)
{
	return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

cl_float3 unit_vec(const cl_float3 vec)
{
	const float mag = vec_mag(vec);
	if (mag == 0)
		return (cl_float3){0, 0, 0};
	else
		return (cl_float3){vec.x/mag, vec.y/mag, vec.z/mag};
}

float dot(const cl_float3 a, const cl_float3 b)
{
	return (a.x * b.x + a.y * b.y + a.z * b.z);
}

cl_float3 cross(const cl_float3 a, const cl_float3 b)
{
	return (cl_float3){	a.y * b.z - a.z * b.y,
						a.z * b.x - a.x * b.z,
						a.x * b.y - a.y * b.x};
}

cl_float3 vec_sub(const cl_float3 a, const cl_float3 b)
{
	return (cl_float3){a.x - b.x, a.y - b.y, a.z - b.z};
}

cl_float3 vec_add(const cl_float3 a, const cl_float3 b)
{
	return (cl_float3){a.x + b.x, a.y + b.y, a.z + b.z};
}

cl_float3 vec_scale(const cl_float3 vec, const float scalar)
{
	return (cl_float3){vec.x * scalar, vec.y * scalar, vec.z * scalar};
}

cl_float3 mat_vec_mult(const t_3x3 mat, const cl_float3 vec)
{
	return (cl_float3){dot(mat.row1, vec), dot(mat.row2, vec), dot(mat.row3, vec)};
}

cl_float3 angle_axis_rot(const float angle, const cl_float3 axis, const cl_float3 vec)
{
	//there may be some issue with this function, it hasn't behaved as expected
	//and is currently unused
	cl_float3 a = vec_scale(vec, cos(angle)); 
	cl_float3 b = vec_scale(cross(axis, vec), sin(angle));
	cl_float3 c = vec_scale(vec_scale(axis, dot(axis, vec)), 1 - cos(angle));

	return vec_add(a, vec_add(b, c));
}

t_3x3 rotation_matrix(const cl_float3 a, const cl_float3 b)
{
	//returns a matrix that will rotate vector a to be parallel with vector b.

	const float angle = acos(dot(a,b) / (vec_mag(a) * vec_mag(b)));
	const cl_float3 axis = unit_vec(cross(a, b));
	t_3x3 rotation;
	rotation.row1 = (cl_float3){	cos(angle) + axis.x * axis.x * (1 - cos(angle)),
								axis.x * axis.y * (1 - cos(angle)) - axis.z * sin(angle),
								axis.x * axis.z * (1 - cos(angle)) + axis.y * sin(angle)};
	
	rotation.row2 = (cl_float3){	axis.y * axis.x * (1 - cos(angle)) + axis.z * sin(angle),
								cos(angle) + axis.y * axis.y * (1 - cos(angle)),
								axis.y * axis.z * (1 - cos(angle)) - axis.x * sin(angle)};

	rotation.row3 = (cl_float3){	axis.z * axis.x * (1 - cos(angle)) - axis.y * sin(angle),
								axis.z * axis.y * (1 - cos(angle)) + axis.x * sin(angle),
								cos(angle) + axis.z * axis.z * (1 - cos(angle))};
	return rotation;
}

cl_float3 bounce(const cl_float3 in, const cl_float3 norm)
{
	return unit_vec(vec_add(vec_scale(norm, -2.0 * dot(in, norm)), in));
}

float dist(const cl_float3 a, const cl_float3 b)
{
	return vec_mag(vec_sub(a, b));
}

cl_float3 vec_rev(const cl_float3 v)
{
	return (cl_float3){v.x * -1.0, v.y * -1.0, v.z * -1.0};
}

cl_float3 vec_inv(const cl_float3 v)
{
	return (cl_float3){1.0 / v.x, 1.0 / v.y, 1.0 / v.z};
}

int vec_equ(const cl_float3 a, const cl_float3 b)
{
	if (a.x == b.x)
		if (a.y == b.y)
			if (a.z == b.z)
				return (1);
	return (0);
}

void orthonormal(cl_float3 a, cl_float3 *b, cl_float3 *c)
{
	if (fabs(a.x) > fabs(a.y))
	{
		float invLen = 1.0 / sqrt(a.x * a.x + a.z * a.z);
		*b = (cl_float3){-1 * a.z * invLen, 0.0, a.x * invLen};
	}
	else
	{
		float invLen = 1.0 / sqrt(a.y * a.y + a.z * a.z);
		*b = (cl_float3){0.0, a.z * invLen, -1.0 * a.y * invLen};
	}
	*c = cross(a, *b);
}

cl_float3 vec_had(const cl_float3 a, const cl_float3 b)
{
	return (cl_float3){a.x * b.x, a.y * b.y, a.z * b.z};
}