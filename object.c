#include "rt.h"

void print_object(const t_object *object)
{
	printf("=====obj=====\n");
	printf("type: %d\n", object->shape);
	print_vec(object->position);
	print_vec(object->normal);
	print_vec(object->corner);
	print_vec(object->color);
}

void new_sphere(t_scene *scene, cl_float3 center, float r, cl_float3 color, enum mat material, float emission)
{
	t_object *sphere = calloc(1, sizeof(t_object));
	sphere->shape = SPHERE;
	sphere->material = material;
	sphere->position = center;
	sphere->normal = (cl_float3){r, 0, 0};
	sphere->color = color;
	sphere->emission = emission;

	sphere->next = scene->objects;
	scene->objects = sphere;
}

void new_plane(t_scene *scene, cl_float3 corner_A, cl_float3 corner_B, cl_float3 corner_C, cl_float3 color, enum mat material, float emission)
{
	new_triangle(scene, corner_A, corner_B, corner_C, color, material, emission);
	new_triangle(scene, corner_A, vec_sub(corner_C, vec_sub(corner_B, corner_A)), corner_C, color, material, emission);
}

void new_cylinder(t_scene *scene, cl_float3 center, cl_float3 radius, cl_float3 extent, cl_float3 color)
{
	t_object *cylinder = calloc(1, sizeof(t_object));
	cylinder->shape = CYLINDER;
	cylinder->position = center;
	cylinder->normal = radius;
	cylinder->corner = extent;
	cylinder->color = color;
	cylinder->emission = 0.0;

	cylinder->next = scene->objects;
	scene->objects = cylinder;
}

void new_triangle(t_scene *scene, cl_float3 vertex0, cl_float3 vertex1, cl_float3 vertex2, cl_float3 color, enum mat material, float emission)
{
	t_object *triangle = calloc(1, sizeof(t_object));
	triangle->shape = TRIANGLE;
	triangle->material = material;
	triangle->position = vertex0;
	triangle->normal = vertex1;
	triangle->corner = vertex2;
	triangle->color = color;
	triangle->emission = emission;

	triangle->next = scene->objects;
	scene->objects = triangle;
}

//Source: https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection
int intersect_sphere(const t_ray *ray, const t_object *sphere, float *t)
{
	const cl_float3 o_min_c = vec_sub(ray->origin, sphere->position);

	float a = dot(ray->direction, o_min_c);
	float b = vec_mag(o_min_c);
	float c = sphere->normal.x;

	float radicand = a * a - b * b + c * c;

	if (radicand < 0)
		return 0;
	float d = -1 * a;
	if (radicand > 0)
	{
		float dplus, dminus;
		radicand = sqrt(radicand);
		dplus = d + radicand;
		dminus = d - radicand;
		if (dplus > ERROR && dminus > ERROR)
			d = dplus > dminus ? dminus : dplus;
		else if (dplus > ERROR)
			d = dplus;
		else if (dminus > ERROR)
			d = dminus;
		else
			return (0);
	}
	if (t)
		*t = d;
	return (1);
}

int intersect_plane(const t_ray *ray, const t_object *plane, float *d)
{
	float a = dot(vec_sub(plane->position, ray->origin), plane->normal);
	float b = dot(ray->direction, plane->normal);
	if (b == 0)
		return (0);
	float t = a / b;
	if (t < 0)
		return (0);
	cl_float3 intersection = vec_add(ray->origin, vec_scale(ray->direction, t));
	//above intersects an infinite plane, now we take bounds into consideration
	cl_float3 bound = vec_sub(plane->position, plane->corner);
	
	if (fabs(intersection.x - plane->position.x) <= fabs(bound.x) + ERROR &&
		fabs(intersection.y - plane->position.y) <= fabs(bound.y) + ERROR &&
		fabs(intersection.z - plane->position.z) <= fabs(bound.z) + ERROR)
			{
				*d = t;
				return (1);
			}
	else
		return (0);
}

int intersect_triangle(const t_ray *ray, const t_object *triangle, float *d)
{
	//variable name change
	const cl_float3 v0 = triangle->position;
	const cl_float3 v1 = triangle->normal;
	const cl_float3 v2 = triangle->corner;

	cl_float3 edge1 = vec_sub(v1, v0);
	cl_float3 edge2 = vec_sub(v2, v0);

	cl_float3 h = cross(ray->direction, edge2);
	float a = dot(h, edge1);

	if (fabs(a) < ERROR)
		return 0;
	float f = 1.0f /a;
	cl_float3 s = vec_sub(ray->origin, v0);
	float u = f * (dot(s, h));
	if (u < 0.0 || u > 1.0)
		return 0;
	cl_float3 q = cross(s, edge1);
	float v = f * dot(ray->direction, q);
	if (v < 0.0 || u + v > 1.0)
		return 0;

	float t = f * dot(edge2, q);
	if (t > ERROR)
	{
		*d = t;
		return (1);
	}
	else
		return (0);
}

int intersect_object(const t_ray *ray, const t_object *object, float *d)
{
	enum type shape = object->shape;
	if (shape == SPHERE)
		return (intersect_sphere(ray, object, d));
	else if (shape == PLANE)
		return (intersect_plane(ray, object, d));
	// else if (shape == CYLINDER)
	// 	return (intersect_cylinder(ray, object, hit));
	else if (shape == TRIANGLE)
		return (intersect_triangle(ray, object, d));
	else
		return (0);
}

cl_float3 norm_triangle(const t_object *triangle, const cl_float3 dir)
{
	const cl_float3 v0 = triangle->position;
	const cl_float3 v1 = triangle->normal;
	const cl_float3 v2 = triangle->corner;

	cl_float3 edge1 = vec_sub(v1, v0);
	cl_float3 edge2 = vec_sub(v2, v0);
	cl_float3 N = unit_vec(cross(edge1, edge2));
	if (dot(dir, N) < 0)
		return N;
	else
		return vec_rev(N);
}

cl_float3 norm_cylinder(const t_object *cyl, cl_float3 hit)
{
	float scalar_proj = dot(hit, cyl->corner); // "corner" here is actually "extent"
	cl_float3 vector_proj = vec_scale(unit_vec(cyl->corner), scalar_proj);
	return (unit_vec(vec_sub(hit, vector_proj)));
}

cl_float3 norm_object(const t_object *object, const t_ray *ray)
{
	//the ray's origin has been updated to the point of intersection
	enum type shape = object->shape;
	if (shape == SPHERE)
		return (unit_vec(vec_sub(ray->origin, object->position)));
	else if (shape == CYLINDER)
		return (norm_cylinder(object, ray->origin));
	else if (shape == PLANE)
	{
		if (dot(ray->direction, object->normal) < 0)
			return (object->normal);
		else
			return (vec_rev(object->normal));
	}
	else if (shape == TRIANGLE)
		return (norm_triangle(object, ray->direction));
	else
		return (cl_float3){0,0,0};
}