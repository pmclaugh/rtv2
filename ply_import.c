#include "rt.h"

typedef union	s_union
{
	uint8_t	bytes[4];
	float	f_val;
	int		i_val;
}				t_union;

Scene	*scene_from_ply(char *rel_path, char *filename)
{
	char *file = malloc(strlen(rel_path) + strlen(filename) + 1);

	strcpy(file, rel_path);
	strcat(file, filename);
	Scene *S = calloc(1, sizeof(Scene));
	S->faces = ply_import(file, &S->face_count);
	free(file);
	S->materials = calloc(1, sizeof(Material));
	S->mat_count = 1;
	S->materials[0].Ns = 10;
	S->materials[0].Ni = 1.5;
	S->materials[0].d = 1;
	S->materials[0].Tr = 0;
	S->materials[0].Tf = (cl_float3){1, 1, 1};
	S->materials[0].illum = 2;
    S->materials[0].Ka = (cl_float3){0.58, 0.58, 0.58}; //ambient mask
    S->materials[0].Kd = (cl_float3){0.58, 0.58, 0.58}; //diffuse mask
    S->materials[0].Ks = (cl_float3){0, 0, 0};
    S->materials[0].Ke = (cl_float3){0, 0, 0};
	S->materials[0].friendly_name = calloc(1, sizeof(char));
	S->materials[0].map_Ka_path = calloc(1, sizeof(char));
	S->materials[0].map_Kd_path = calloc(1, sizeof(char));
	S->materials[0].map_bump_path = calloc(1, sizeof(char));
	S->materials[0].map_d_path = calloc(1, sizeof(char));
	S->materials[0].map_Ks_path = calloc(1, sizeof(char));
	S->materials[0].map_Ka = calloc(1, sizeof(Map));
	S->materials[0].map_Kd = calloc(1, sizeof(Map));
	S->materials[0].map_bump = calloc(1, sizeof(Map));
	S->materials[0].map_d = calloc(1, sizeof(Map));
	S->materials[0].map_Ks = calloc(1, sizeof(Map));
	S->materials[0].map_Ka->pixels = calloc(1, sizeof(1));
	S->materials[0].map_Kd->pixels = calloc(1, sizeof(1));
	S->materials[0].map_bump->pixels = calloc(1, sizeof(1));
	S->materials[0].map_d->pixels = calloc(1, sizeof(1));
	S->materials[0].map_Ks->pixels = calloc(1, sizeof(1));
	S->materials[0].map_Ka->height = 0;
	S->materials[0].map_Ka->width = 0;
	S->materials[0].map_Kd->height = 0;
	S->materials[0].map_Kd->width = 0;
	S->materials[0].map_bump->height = 0;
	S->materials[0].map_bump->width = 0;
	S->materials[0].map_d->height = 0;
	S->materials[0].map_d->width = 0;
	S->materials[0].map_Ks->height = 0;
	S->materials[0].map_Ks->width = 0;
	return S;
}

static Face *read_binary_file(FILE *fp, int f_total, int v_total, int file_endian)
{
	Face *list = NULL;
	list = calloc(f_total, sizeof(Face));
	cl_float3 *V = calloc(v_total, sizeof(cl_float3));

	//check machine architecture, big endian or little endian?
	unsigned int x = 1;//00001 || 10000
	char *ptr = (char*)&x;
	int	endian = ((int)*ptr == 1) ? 0 : 1;// Big endian == 1; Little endian == 0;
	printf("this machine is %s endian\n", (endian) ? "big" : "little");

	t_union	u;
	uint8_t	tmp;
	for (int i = 0; i < v_total; i++)
	{
		for (int xyz = 0; xyz < 3; xyz++)
		{
			bzero(u.bytes, 4);
			fread(u.bytes, 1, 4, fp);
			if (endian != file_endian)
			{
				tmp = u.bytes[0];
				u.bytes[0] = u.bytes[3];
				u.bytes[3] = tmp;
				tmp = u.bytes[1];
				u.bytes[1] = u.bytes[2];
				u.bytes[2] = tmp;
				//reorganize bytes to fit machine endian architecture
			}
			if (xyz == 0)
				V[i].x = (float)u.f_val;
			else if (xyz == 1)
				V[i].y = (float)u.f_val;
			else
				V[i].z = (float)u.f_val;
		}
	}

	//now faces
	uint8_t n;
	for (int i = 0; i < f_total; i++)
	{
		int a, b, c;
		fread(&n, 1, 1, fp);
		if (n != 3)
			printf("this polygon has %hhu vertices\n", n);
		for (int xyz = 0; xyz < 3; xyz++)
		{
			bzero(u.bytes, 4);
			fread(u.bytes, 1, 4, fp);
			if (endian != file_endian)
			{
				tmp = u.bytes[0];
				u.bytes[0] = u.bytes[3];
				u.bytes[3] = tmp;
				tmp = u.bytes[1];
				u.bytes[1] = u.bytes[2];
				u.bytes[2] = tmp;
				//reorganize bytes to fit machine endian architecture
			}
			if (xyz == 0)
				a = (int)u.i_val;
			else if (xyz == 1)
				b = (int)u.i_val;
			else
				c = (int)u.i_val;
		}
		if (a < 0 || a >= v_total || b < 0 || b >= v_total || c < 0 || c >= v_total)
			printf("index is outside of array size\n");
		Face face;
		face.shape = 3;
		face.mat_type = GPU_MAT_DIFFUSE;
		face.mat_ind = 0; //some default material
		face.verts[0] = V[a];
		face.verts[1] = V[b];
		face.verts[2] = V[c];

		face.center = vec_scale(vec_add(vec_add(V[a], V[b]), V[c]), 1.0 / 3.0);

		cl_float3 N = cross(vec_sub(V[b], V[a]), vec_sub(V[c], V[a]));
		face.norms[0] = N;
		face.norms[1] = N;
		face.norms[2] = N;

		face.next = NULL;
		list[i] = face;
	}
	free(V);
	return list;
}

static Face *read_ascii_file(FILE *fp, int f_total, int v_total)
{
	Face *list = NULL;
	list = calloc(f_total, sizeof(Face));
	cl_float3 *V = calloc(v_total, sizeof(cl_float3));
	char *line = malloc(512);

	for (int i = 0; i < v_total; i++)
	{
		fgets(line, 512, fp);
		sscanf(line, "%f %f %f", &V[i].x, &V[i].y, &V[i].z);
	}

	//now faces
	for (int i = 0; i < f_total; i++)
	{
		int a, b, c, n;
		fgets(line, 512, fp);
		sscanf(line, "%d %d %d %d ", &n, &a, &b, &c);
		Face face;
		face.shape = 3;
		face.mat_type = GPU_MAT_DIFFUSE;
		face.mat_ind = 0; //some default material
		face.verts[0] = V[a];
		face.verts[1] = V[b];
		face.verts[2] = V[c];

		face.center = vec_scale(vec_add(vec_add(V[a], V[b]), V[c]), 1.0 / 3.0);

		cl_float3 N = cross(vec_sub(V[b], V[a]), vec_sub(V[c], V[a]));
		face.norms[0] = N;
		face.norms[1] = N;
		face.norms[2] = N;

		face.next = NULL;
		list[i] = face;
	}
	free(V);
	free(line);
	return list;
}

Face *ply_import(char *ply_file, int *face_count)
{
	//import functions should return linked lists of faces.
	
	FILE *fp = fopen(ply_file, "r");
	if (fp == NULL)
	{
		printf("tried and failed to open file %s\n", ply_file);
		exit(1);
	}

	char *line = malloc(512);
	int v_total;
	int f_total;
	int	file_type = 2;

	while(fgets(line, 512, fp))
	{
		if (strstr(line, "format"))
		{
			if (strstr(line, "ascii"))
				file_type = 2;
			else if (strstr(line, "big"))
				file_type = 1;
			else if (strstr(line, "little"))
				file_type = 0;
		}
		else if (strncmp(line, "element vertex", 14) == 0)
			sscanf(line, "element vertex %d\n", &v_total);
		else if (strncmp(line, "element face", 12) == 0)
			sscanf(line, "element face %d\n", &f_total);
		else if (strncmp(line, "end_header", 10) == 0)
			break ;
	}
	free(line);
	*face_count = f_total;
	printf("%s: %d vertices, %d faces\n", ply_file, v_total, f_total);

	if (file_type == 2)
		return read_ascii_file(fp, f_total, v_total);
	else
		return read_binary_file(fp, f_total, v_total, file_type);
}
