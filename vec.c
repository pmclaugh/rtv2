#include "rt.h"

//various vector functions

void print_vec(const t_float3 vec)
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

float vec_mag(const t_float3 vec)
{
	return sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

t_float3 unit_vec(const t_float3 vec)
{
	const float mag = vec_mag(vec);
	if (mag == 0)
		return (t_float3){0, 0, 0};
	else
		return (t_float3){vec.x/mag, vec.y/mag, vec.z/mag};
}

float dot(const t_float3 a, const t_float3 b)
{
	return (a.x * b.x + a.y * b.y + a.z * b.z);
}

t_float3 cross(const t_float3 a, const t_float3 b)
{
	return (t_float3){	a.y * b.z - a.z * b.y,
						a.z * b.x - a.x * b.z,
						a.x * b.y - a.y * b.x};
}

t_float3 vec_sub(const t_float3 a, const t_float3 b)
{
	return (t_float3){a.x - b.x, a.y - b.y, a.z - b.z};
}

t_float3 vec_add(const t_float3 a, const t_float3 b)
{
	return (t_float3){a.x + b.x, a.y + b.y, a.z + b.z};
}

t_float3 vec_scale(const t_float3 vec, const float scalar)
{
	return (t_float3){vec.x * scalar, vec.y * scalar, vec.z * scalar};
}

t_float3 mat_vec_mult(const t_3x3 mat, const t_float3 vec)
{
	return (t_float3){dot(mat.row1, vec), dot(mat.row2, vec), dot(mat.row3, vec)};
}

t_float3 angle_axis_rot(const float angle, const t_float3 axis, const t_float3 vec)
{
	//there may be some issue with this function, it hasn't behaved as expected
	//and is currently unused
	t_float3 a = vec_scale(vec, cos(angle)); 
	t_float3 b = vec_scale(cross(axis, vec), sin(angle));
	t_float3 c = vec_scale(vec_scale(axis, dot(axis, vec)), 1 - cos(angle));

	return vec_add(a, vec_add(b, c));
}

t_3x3 rotation_matrix(const t_float3 a, const t_float3 b)
{
	//returns a matrix that will rotate vector a to be parallel with vector b.

	const float angle = acos(dot(a,b) / (vec_mag(a) * vec_mag(b)));
	const t_float3 axis = unit_vec(cross(a, b));
	t_3x3 rotation;
	rotation.row1 = (t_float3){	cos(angle) + axis.x * axis.x * (1 - cos(angle)),
								axis.x * axis.y * (1 - cos(angle)) - axis.z * sin(angle),
								axis.x * axis.z * (1 - cos(angle)) + axis.y * sin(angle)};
	
	rotation.row2 = (t_float3){	axis.y * axis.x * (1 - cos(angle)) + axis.z * sin(angle),
								cos(angle) + axis.y * axis.y * (1 - cos(angle)),
								axis.y * axis.z * (1 - cos(angle)) - axis.x * sin(angle)};

	rotation.row3 = (t_float3){	axis.z * axis.x * (1 - cos(angle)) - axis.y * sin(angle),
								axis.z * axis.y * (1 - cos(angle)) + axis.x * sin(angle),
								cos(angle) + axis.z * axis.z * (1 - cos(angle))};
	return rotation;
}

t_float3 bounce(const t_float3 in, const t_float3 norm)
{
	return unit_vec(vec_add(vec_scale(norm, -2.0 * dot(in, norm)), in));
}

float dist(const t_float3 a, const t_float3 b)
{
	return vec_mag(vec_sub(a, b));
}

t_float3 vec_rev(const t_float3 v)
{
	return (t_float3){v.x * -1.0, v.y * -1.0, v.z * -1.0};
}