#include "rt.h"

void tesselate(Face *faces)
{
	//split shapes of dim > 3 down to triangles

	//also optionally split up big triangles
}

void flatten(Scene *S)
{
	int count = 0;
	for (Face *f = S->faces; f; f = f->next)
		count++;
	
	S->face_count = count;
	Face *flat = calloc(count, sizeof(Face));

	Face *f = S->faces;
	for (int i = 0; i < count; i++)
	{
		flat[i] = *f;
		Face *tmp = f;
		f = f->next;
		free(tmp);	
	}
	S->faces = flat;
}