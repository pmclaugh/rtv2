#include "rt.h"

void tesselate(Face *faces)
{
	//split shapes of dim > 3 down to triangles

	//also optionally split up big triangles
}

Face *object_flatten(Face *faces, int *face_count)
{
	int count = 0;
	for (Face *f = faces; f; f = f->next)
		count++;

	Face *flat = calloc(count, sizeof(Face));

	Face *f = faces;
	for (int i = 0; i < count; i++)
	{
		flat[i] = *f;
		Face *tmp = f;
		f = f->next;
		free(tmp);	
	}
	*face_count = count;
	return flat;
}

//also want a scene_flatten function