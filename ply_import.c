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

	printf("%s: %d vertices, %d faces\n", ply_file, v_total, f_total);

	cl_float3 *V = calloc(v_total, sizeof(cl_float3));

	for (int i = 0; i < v_total; i++)
	{
		float duh, buh;
		fgets(line, 512, fp);
		sscanf(line, "%f %f %f %f %f\n", &V[i].x, &V[i].y, &V[i].z, &duh, &buh);
	}

	//now faces
	for (int i = 0; i < f_total; i++)
	{
		int a, b, c, d;
		fgets(line, 512, fp);
		sscanf(line, "%d %d %d %d", &d, &a, &b, &c);
		Face *face = calloc(1, sizeof(Face));
		face->shape = d;
		face->mat_type = GPU_MAT_DIFFUSE;
		face->mat_ind = 0; //some default material
		face->verts[0] = V[a];
		face->verts[1] = V[b];
		face->verts[2] = V[c];

		face->center = vec_scale(vec_add(vec_add(V[a], V[b]), V[c]), 1.0 / 3.0);

		cl_float3 N = cross(vec_sub(V[b], V[a]), vec_sub(V[c], V[a]));
		face->norms[0] = N;
		face->norms[1] = N;
		face->norms[2] = N;

		face->tex[0] = (cl_float3){0.0f, 0.0f, 0.0f};
		face->tex[1] = (cl_float3){0.0f, 0.0f, 0.0f};
		face->tex[2] = (cl_float3){0.0f, 0.0f, 0.0f};

		face->next = list;
		list = face;
	}
	free(V);
	return list;
}