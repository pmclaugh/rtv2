#include "rt.h"

typedef union	s_union
{
	uint8_t	bytes[4];
	float	f_val;
	int		i_val;
}				t_union;

Scene	*scene_from_ply(char *rel_path, char *filename, File_edits edit_info)
{
	char *file = malloc(strlen(rel_path) + strlen(filename) + 1);

	strcpy(file, rel_path);
	strcat(file, filename);
	Scene *S = calloc(1, sizeof(Scene));
	S->faces = ply_import(file, edit_info, &S->face_count);
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

static Face *read_binary_file(FILE *fp, int f_total, int v_total, File_edits edit_info, int file_endian)
{
	Face *list = NULL;
	list = calloc(f_total, sizeof(Face));
	cl_float3 *V = calloc(v_total, sizeof(cl_float3));
	cl_float3 min = (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX};
	cl_float3 max = (cl_float3){FLT_MIN, FLT_MIN, FLT_MIN};
	cl_float3 center;

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
				//reorganize bytes to fit machine endian architecture
				tmp = u.bytes[0];
				u.bytes[0] = u.bytes[3];
				u.bytes[3] = tmp;
				tmp = u.bytes[1];
				u.bytes[1] = u.bytes[2];
				u.bytes[2] = tmp;
			}
			if (xyz == 0)
				V[i].x = (float)u.f_val;
			else if (xyz == 1)
				V[i].y = (float)u.f_val;
			else
				V[i].z = (float)u.f_val;
		}
		if (V[i].x < min.x)
			min.x = V[i].x;
		if (V[i].y < min.y)
			min.y = V[i].y;
		if (V[i].z < min.z)
			min.z = V[i].z;
		if (V[i].x > max.x)
			max.x = V[i].x;
		if (V[i].y > max.y)
			max.y = V[i].y;
		if (V[i].z > max.z)
			max.z = V[i].z;
	}
	center = vec_add(vec_scale(vec_sub(max, min), 0.5), min);
	for (int i = 0; i < v_total; i++)
	{
		V[i] = vec_sub(V[i], center);
		V[i] = vec_scale(V[i], edit_info.scale);
		vec_rot(edit_info.rotate, &V[i]);
		V[i] = vec_add(V[i], edit_info.translate);
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
				//reorganize bytes to fit machine endian architecture
				tmp = u.bytes[0];
				u.bytes[0] = u.bytes[3];
				u.bytes[3] = tmp;
				tmp = u.bytes[1];
				u.bytes[1] = u.bytes[2];
				u.bytes[2] = tmp;
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

static Face *read_ascii_file(FILE *fp, int f_total, int v_total, File_edits edit_info)
{
	Face *list = NULL;
	list = calloc(f_total, sizeof(Face));
	cl_float3 *V = calloc(v_total, sizeof(cl_float3));
	char *line = malloc(512);
	cl_float3 min = (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX};
	cl_float3 max = (cl_float3){FLT_MIN, FLT_MIN, FLT_MIN};
	cl_float3 center;

	for (int i = 0; i < v_total; i++)
	{
		fgets(line, 512, fp);
		sscanf(line, "%f %f %f", &V[i].x, &V[i].y, &V[i].z);
		if (V[i].x < min.x)
			min.x = V[i].x;
		if (V[i].y < min.y)
			min.y = V[i].y;
		if (V[i].z < min.z)
			min.z = V[i].z;
		if (V[i].x > max.x)
			max.x = V[i].x;
		if (V[i].y > max.y)
			max.y = V[i].y;
		if (V[i].z > max.z)
			max.z = V[i].z;
	}
	center = vec_add(vec_scale(vec_sub(max, min), 0.5), min);
	for (int i = 0; i < v_total; i++)
	{
		V[i] = vec_sub(V[i], center);
		V[i] = vec_scale(V[i], edit_info.scale);
		vec_rot(edit_info.rotate, &V[i]);
		V[i] = vec_add(V[i], edit_info.translate);
	}

	//now faces
	for (int i = 0; i < f_total; i++)
	{
		int a, b, c, d, n;
		fgets(line, 512, fp);
		sscanf(line, "%d %d %d %d %d", &n, &a, &b, &c, &d);
		Face face;
		face.shape = 3;
		face.mat_type = GPU_MAT_DIFFUSE;
		face.mat_ind = 0; //some default material
		face.verts[0] = V[a];
		face.verts[1] = V[b];
		face.verts[2] = V[c];

		face.center = vec_scale(vec_add(vec_add(V[a], V[b]), V[c]), 1.0 / (float)face.shape);

		cl_float3 N = cross(vec_sub(V[b], V[a]), vec_sub(V[c], V[a]));
		face.norms[0] = N;
		face.norms[1] = N;
		face.norms[2] = N;

		face.next = NULL;
		list[i] = face;

		if (n == 4)
		{
			Face face;
			face.shape = 3;
			face.mat_type = GPU_MAT_DIFFUSE;
			face.mat_ind = 0; //some default material
			face.verts[0] = V[a];
			face.verts[1] = V[c];
			face.verts[2] = V[d];

			face.center = vec_scale(vec_add(vec_add(V[a], V[c]), V[d]), 1.0 / (float)face.shape);

			cl_float3 N = cross(vec_sub(V[c], V[a]), vec_sub(V[d], V[a]));
			face.norms[0] = N;
			face.norms[1] = N;
			face.norms[2] = N;

			face.next = NULL;
			list[++i] = face;
		}
	}
	free(V);
	free(line);
	return list;
}

static void check_ascii_face_count(File_info *file, int *face_count, int vert_total)
{
	char *line = calloc(512, sizeof(char));

//	skip vertices while recording the size of this part of file
	while (vert_total-- > 0)
	{
		fgets(line, 512, file->ptr);
		file->verts_size += strlen(line);
		//printf("%s\n", line);
	}

//	now evaluate faces in file to see if any polygons; if so, increase face_count
	int	vertices;
	while(fgets(line, 512, file->ptr))
	{
		file->faces_size += strlen(line);
		vertices = atoi(line);
		if (vertices == 4)
			++(*face_count);
	}
	free(line);
	fseek(file->ptr, file->header_size, SEEK_SET);
}

Face *ply_import(char *ply_file, File_edits edit_info, int *face_count)
{
	//import functions should return linked lists of faces.
	File_info file;	
	file.ptr = fopen(ply_file, "r");
	strcpy(file.name, ply_file);
	file.header_size = 0;
	file.verts_size = 0;
	file.faces_size = 0;
	if (file.ptr == NULL)
	{
		printf("tried and failed to open file %s\n", ply_file);
		exit(1);
	}

	char *line = malloc(512);
	int v_total;
	int f_total;
	int	file_type = 2;

	while(fgets(line, 512, file.ptr))
	{
		file.header_size += strlen(line);
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

	if (file_type == 2)
		check_ascii_face_count(&file, &f_total, v_total);
	else
		;//check_binary_face_count(&file, &f_total, v_total);
	free(line);
	*face_count = f_total;
	printf("%s: %d vertices, %d faces\n", ply_file, v_total, f_total);

	if (file_type == 2)
		return read_ascii_file(file.ptr, f_total, v_total, edit_info);
	else
		return read_binary_file(file.ptr, f_total, v_total, edit_info, file_type);
}
