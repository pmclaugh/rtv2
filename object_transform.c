#include "rt.h"

//functions to transform objects (ie lists of whole faces) before merging them into the scene

//going to implement these later, but basically:
/*

void center(Face *list, cl_float3 center)

void translate(Face *list, cl_float3 disp)

void scale(Face *list, float factor)

void rotate(Face *list, t_3x3 matrix)

Face *duplicate(Face *list)

*/

cl_float3 find_center(Face *list)
{
	//measure center of list
	float max_x = -1.0 * FLT_MAX;
	float max_y = -1.0 * FLT_MAX;
	float max_z = -1.0 * FLT_MAX;

	float min_x = FLT_MAX;
	float min_y = FLT_MAX;
	float min_z = FLT_MAX; //never forget

	for (Face *f = list; f; f = f->next)
		for (int i = 0; i < f->shape; i++)
		{
			max_x = fmax(f->verts[i].x, max_x);
			max_y = fmax(f->verts[i].y, max_y);
			max_z = fmax(f->verts[i].z, max_z);

			min_x = fmin(f->verts[i].x, min_x);
			min_y = fmin(f->verts[i].y, min_y);
			min_z = fmin(f->verts[i].z, min_z);
		}

	cl_float3 min = (cl_float3){min_x - ERROR, min_y - ERROR, min_z - ERROR};
	cl_float3 max = (cl_float3){max_x + ERROR, max_y + ERROR, max_z + ERROR};

	return vec_scale(vec_add(min, max), 0.5f);
}

void translate(Face *list, cl_float3 displacement)
{
	//translate each face by displacement vector
	for (Face *f = list; f; f = f->next)
		for (int i = 0; i < f->shape; i++)
			f->verts[i] = vec_add(f->verts[i], displacement);
}

void center(Face *list, cl_float3 center)
{
	translate(list, vec_sub(center, find_center(list)));
}

//scale and rotate are slightly more complex and aren't worth writing til i can render