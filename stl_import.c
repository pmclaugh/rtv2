#include "rt.h"

Face *stl_import(char *stl_file)
{
	//import functions should return linked lists of faces.

	FILE *fp = fopen(stl_file, "r");
	if (fp == NULL)
	{
		printf("tried and failed to open file %s\n", stl_file);
		exit(1);
	}

	char *header = malloc(81);
	fread(header, 80, 1, fp);

	int faces;
	fread(&faces, sizeof(int), 1, fp);
	printf("%d faces\n", faces);

	Face *list = NULL;
	for (int i = 0; i < faces; i++)
	{

		float coords[12];
		short attrib;
		bzero(coords, sizeof(float) * 12);
		fread(coords, sizeof(float), 12, fp);
		fread(&attrib, sizeof(short), 1, fp);

		Face *face = calloc(1, sizeof(Face));
		face->shape = 3;
		face->mat_ind = 0;
		face->verts[0] = (cl_float3){coords[0], coords[1], coords[2]};
		face->verts[1] = (cl_float3){coords[3], coords[4], coords[5]};
		face->verts[2] = (cl_float3){coords[6], coords[7], coords[8]};

		cl_float3 N = (cl_float3){coords[9], coords[10], coords[11]};
		face->norms[0] = N;
		face->norms[1] = N;
		face->norms[2] = N;

		face->tex[0] = (cl_float3){0.0f, 0.0f, 0.0f};
		face->tex[1] = (cl_float3){0.0f, 0.0f, 0.0f};
		face->tex[2] = (cl_float3){0.0f, 0.0f, 0.0f};

		face->next = list;
		list = face;
	}
	return list;
}