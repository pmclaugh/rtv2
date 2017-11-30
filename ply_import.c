#include "rt.h"

Face *ply_import(char *ply_file)
{
	//import functions should return linked lists of faces.

	Face *list = NULL;

	FILE *fp = fopen(ply_file, "r");
	if (fp == NULL)
	{
		printf("tried and failed to open file %s\n", ply_file);
		exit(1);
	}

	char *line = malloc(512);
	int v_total;
	int f_total;

	while(fgets(line, 512, fp))
	{
		if (strncmp(line, "element vertex", 14) == 0)
			sscanf(line, "element vertex %d\n", &v_total);
		else if (strncmp(line, "element face", 12) == 0)
			sscanf(line, "element face %d\n", &f_total);
		else if (strncmp(line, "end_header", 10) == 0)
			break ;
	}

	cl_float3 *V = calloc(v_total, sizeof(cl_float3));
	//the ply files i'm using only have triangles
	for (int i = 0; i < v_total; i++)
	{
		fgets(line, 512, fp);
		sscanf(line, "%f %f %f", &V[i].x, &V[i].y, &V[i].z);
	}

	//now faces
	for (int i = 0; i < f_total; i++)
	{
		int a, b, c;
		fgets(line, 512, fp);
		sscanf(line, "%d %d %d", &a, &b, &c);
		Face *face = calloc(1, sizeof(Face));
		face->shape = 3;
		face->mat_type = GPU_MAT_DIFFUSE;
		face->mat_ind = 0; //some default material
		face->verts[0] = V[a];
		face->verts[1] = V[b];
		face->verts[2] = V[c];
		cl_float3 N = cross(vec_sub(V[b], V[a]), vec_sub(V[c], V[a]));
		face->norms[0] = N;
		face->norms[1] = N;
		face->norms[2] = N;

		face->next = list;
		list = face;
	}

	return list;
}