#include "rt.h"

//various vector functions

cl_float3 get_vec(const char *line)
{
	cl_float3 vec = (cl_float3){0, 0, 0};

	char *ptr = strchr(line, '=') + 1;
	vec.x = strtof(ptr, &ptr);
	if (*ptr == ',')
		++ptr;
	vec.y = strtof(ptr, &ptr);
	if (*ptr == ',')
		++ptr;
	vec.z = strtof(ptr, &ptr);
	return vec;
}

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

	const float angle = acos(dot(a,b));
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

cl_float3	vec_rotate_xy(const cl_float3 a, const float angle)
{
	cl_float3	res;
	res.x = (a.x * cos(angle)) + (a.y * -sin(angle));
	res.y = (a.x * sin(angle)) + (a.y * cos(angle));
	res.z = a.z;
	return res;
}

cl_float3	vec_rotate_yz(const cl_float3 a, const float angle)
{
	cl_float3	res;
	res.x = a.x;
	res.y = (a.y * cos(angle)) + (a.z * -sin(angle));
	res.z = (a.y * sin(angle)) + (a.z * cos(angle));
	return res;
}

cl_float3	vec_rotate_xz(const cl_float3 a, const float angle)
{
	cl_float3	res;
	res.x = (a.x * cos(angle)) + (a.z * sin(angle));
	res.y = a.y;
	res.z = (a.x * -sin(angle)) + (a.z * cos(angle));
	return res;
}

cl_float3 vec_rev(const cl_float3 v)
{
	return (cl_float3){v.x * -1.0, v.y * -1.0, v.z * -1.0};
}

void vec_rot(const cl_float3 rotate, cl_float3 *V)
{
	float	angle;
	float	tmp[2];

	angle = (M_PI * rotate.x) / 180.0;
	tmp[0] = V->z * cos(angle) + V->y * sin(angle);
	tmp[1] = V->y * cos(angle) - V->z * sin(angle);
	V->z = tmp[0];
	V->y = tmp[1];
	angle = (M_PI * rotate.y) / 180.0;
	tmp[0] = V->x * cos(angle) + V->z * sin(angle);
	tmp[1] = V->z * cos(angle) - V->x * sin(angle);
	V->x = tmp[0];
	V->z = tmp[1];
	angle = (M_PI * rotate.z) / 180.0;
	tmp[0] = V->x * cos(angle) + V->y * sin(angle);
	tmp[1] = V->y * cos(angle) - V->x * sin(angle);
	V->x = tmp[0];
	V->y = tmp[1];
}