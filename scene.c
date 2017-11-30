#include "rt.h"

void tesselate(Face *faces)
{
	//split shapes of dim > 3 down to triangles

	//also optionally split up big triangles
}

void flatten(Scene *S)
{
	//count S->faces and then flatten the list to an array.
	//this is for BVH construction, S will be flattened even more in gpu_launch (separated V, N, T, etc)
}