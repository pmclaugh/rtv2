#include "rt.h"

static cl_float3 min;
static cl_float3 max;

typedef struct	s_faces_link_list
{
	int		num_faces;
	Face	*faces;
	struct s_faces_link_list *next;
}				Faces_list;

static Face *file_3ds_import(char *file_3ds, File_edits edit_info, int *face_count);
static int read_edit_chunk(File_info *file, int file_end, int machine_end, Faces_list **faces);
static int read_material_block(File_info *file, int file_end, int machine_end);
static int read_object_block(File_info *file, int file_end, int machine_end, Faces_list **faces);
static int read_triangular_mesh(File_info *file, int file_end, int machine_end, Faces_list **faces);
static int read_vertices_list(File_info *file, int file_end, int machine_end, cl_float3 **V);
static int read_faces_descript(File_info *file, int file_end, int machine_end, cl_float3 **V, Faces_list **faces);
static int read_map_coords(File_info *file, int file_end, int machine_end);
static int read_faces_material(File_info *file, int file_end, int machine_end);
static int read_smoothing_list(File_info *file, int file_end, int machine_end, int num_faces);
static Face *list_to_array(Faces_list **faces, int *face_count);
static void apply_edits(Face *faces, File_edits edit_info, int face_count);

Scene	*scene_from_3ds(char *rel_path, char *filename, File_edits edit_info)
{
	char *file = malloc(strlen(rel_path) + strlen(filename) + 1);

	strcpy(file, rel_path);
	strcat(file, filename);
	Scene *S = calloc(1, sizeof(Scene));
	min = (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX};
	max = (cl_float3){FLT_MIN, FLT_MIN, FLT_MIN};
	
	S->faces = file_3ds_import(file, edit_info, &S->face_count);
	free(file);

	S->materials = calloc(1, sizeof(Material));
	S->mat_count = 1;
	S->materials[0].friendly_name = NULL;
	S->materials[0].map_Ka_path = NULL;
	S->materials[0].map_Ka = NULL;
	S->materials[0].map_bump_path = NULL;
	S->materials[0].map_bump = NULL;
	S->materials[0].map_d_path = NULL;
	S->materials[0].map_d = NULL;
	S->materials[0].map_Kd = NULL;
	if ((S->materials[0].map_Kd_path = edit_info.map_Kd_path) != NULL)
	{
		;
	}
	S->materials[0].map_Ks = NULL;
	if ((S->materials[0].map_Ks_path = edit_info.map_Ks_path) != NULL)
	{
		;
	}
    S->materials[0].map_Ke = NULL;
    if ((S->materials[0].map_Ke_path = edit_info.map_Ke_path) != NULL)
	{
		;
	}
	S->materials[0].Ns.x = 10;
	S->materials[0].Ni = edit_info.ior;
	S->materials[0].roughness = edit_info.roughness;
	S->materials[0].Tr = edit_info.transparency;
	S->materials[0].d = 1 - edit_info.transparency;
	S->materials[0].Tf = (cl_float3){1, 1, 1};
	S->materials[0].illum = 2;
    S->materials[0].Ka = (cl_float3){0.58, 0.58, 0.58}; //ambient mask
    S->materials[0].Kd = edit_info.Kd; //diffuse mask
    S->materials[0].Ks = edit_info.Ks;
    S->materials[0].Ke = edit_info.Ke;
	printf("exiting from scene_from_3ds();\n\n");
//	exit(0);
	return S;
}

static Face *file_3ds_import(char *file_3ds, File_edits edit_info, int *face_count)
{
	File_info file;
	file.ptr = fopen(file_3ds, "rb");
	strcpy(file.name, file_3ds);
	file.header_size = 0;
	file.verts_size = 0;
	file.faces_size = 0;
	if (file.ptr == NULL)
	{
		printf("tried and failed to open file %s\n", file_3ds);
		exit(1);
	}

	unsigned int x = 1;//00001 || 10000
	char *ptr = (char*)&x;
	int endian = ((int)*ptr == 1) ? 0 : 1;// Big endian == 1; Little endian == 0;
	printf("this machine is %s endian\n\n", (endian) ? "big" : "little");
	
	int file_size;
	long counter = 6L;
	short id;	// hopefully short is 2 bytes on your system
	int length;	// and an int is 4 bytes :)
	long int chunk_beginning;

	// main3ds
	id = read_short(file.ptr, 0, endian);
	length = read_int(file.ptr, 0, endian);
	printf("id=%hx\n", id);
	printf("length=%d\n\n", length);
	file_size = length;

	Faces_list *faces = NULL;
	while (counter < file_size)
	{
		chunk_beginning = ftell(file.ptr);
		id = read_short(file.ptr, 0, endian);
		printf("id=%hx\n", id);
		if (id == 0x3d3d)
			counter += read_edit_chunk(&file, 0, endian, &faces);
		else
			counter += read_int(file.ptr, 0, endian);

		fseek(file.ptr, counter, SEEK_SET);
	}
	fclose(file.ptr);
	Face *face_arr =  list_to_array(&faces, face_count);
	apply_edits(face_arr, edit_info, *face_count);
	return face_arr;
}

static void apply_edits(Face *faces, File_edits edit_info, int face_count)
{
	cl_float3 center = vec_add(vec_scale(vec_sub(max, min), 0.5), min);

	for (int i = 0; i < face_count; i++)
	{
		faces[i].verts[0] = vec_sub(faces[i].verts[0], center);
		faces[i].verts[0] = vec_scale(faces[i].verts[0], edit_info.scale);
		vec_rot(edit_info.rotate, &(faces[i].verts[0]));
		faces[i].verts[0] = vec_add(faces[i].verts[0], edit_info.translate);
		
		faces[i].verts[1] = vec_sub(faces[i].verts[1], center);
		faces[i].verts[1] = vec_scale(faces[i].verts[1], edit_info.scale);
		vec_rot(edit_info.rotate, &(faces[i].verts[1]));
		faces[i].verts[1] = vec_add(faces[i].verts[1], edit_info.translate);
		
		faces[i].verts[2] = vec_sub(faces[i].verts[2], center);
		faces[i].verts[2] = vec_scale(faces[i].verts[2], edit_info.scale);
		vec_rot(edit_info.rotate, &(faces[i].verts[2]));
		faces[i].verts[2] = vec_add(faces[i].verts[2], edit_info.translate);
	}
}

static Face *list_to_array(Faces_list **faces, int *face_count)
{
	Faces_list *temp = *faces;

	for ( ; temp != NULL; temp = temp->next)
		*face_count += temp->num_faces;
	temp = *faces;
	Face *faces_arr = calloc(*face_count, sizeof(Face));
	for (int i = 0; i < *face_count; )
	{
		for (int j = 0; j < temp->num_faces; j++)
			faces_arr[i + j] = temp->faces[j];
		i += temp->num_faces;
		temp = temp->next;
	}
	while (*faces != NULL)
	{
		temp = (*faces)->next;
		free((*faces)->faces);
		free(*faces);
		*faces = temp;
	}
	return faces_arr;
}

static int read_edit_chunk(File_info *file, int file_end, int machine_end, Faces_list **faces)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);

	while (counter + 6L < length)
	{
		id = read_short(file->ptr, file_end, machine_end);
		printf("id=%hx\n", id);
		if (id == 0x4000)
			counter += read_object_block(file, file_end, machine_end, faces);
		else if (id == 0xafff)
			counter += read_material_block(file, file_end, machine_end);
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_material_block(File_info *file, int file_end, int machine_end)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);

	while (counter + 6L < length)
	{
		id = read_short(file->ptr, file_end, machine_end);
		printf("id=%hx\n", id);
		counter += read_int(file->ptr, file_end, machine_end);
		if (id == 0xa000)
		{
			char name[50];
			for (int i = 0; (name[i] = read_char(file->ptr, file_end, machine_end)) != '\0'; i++)
				;
			printf("name=%s\n", name);
		}

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_object_block(File_info *file, int file_end, int machine_end, Faces_list **faces)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	short id;
	long chunk_beginning = ftell(file->ptr);
	char name[50];

	for (int i = 0; (name[i] = read_char(file->ptr, file_end, machine_end)) != '\0'; i++)
		;
	printf("name=%s\n", name);
	counter += strlen(name) + 1;

	while (counter + 6L < length)
	{
		id = read_short(file->ptr, file_end, machine_end);
		printf("id=%hx\n", id);
		if (id == 0x4100)
			counter += read_triangular_mesh(file, file_end, machine_end, faces);
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_triangular_mesh(File_info *file, int file_end, int machine_end, Faces_list **faces)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	short id;
	long chunk_beginning = ftell(file->ptr);
	cl_float3 *V = NULL;

	while (counter + 6L < length)
	{
		id = read_short(file->ptr, file_end, machine_end);
		printf("id=%hx\n", id);
		if (id == 0x4110)
			counter += read_vertices_list(file, file_end, machine_end, &V);// vertices list
		else if (id == 0x4120)
			counter += read_faces_descript(file, file_end, machine_end, &V, faces);// Faces description
		else if (id == 0x4140)
			counter += read_map_coords(file, file_end, machine_end);// Mapping coordinates list
		else if (id == 0x4160)
			counter += read_int(file->ptr, file_end, machine_end);// Local coordinates system
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_vertices_list(File_info *file, int file_end, int machine_end, cl_float3 **V)
{
	int length = read_int(file->ptr, file_end, machine_end);
	short id;
	long chunk_beginning = ftell(file->ptr);
	unsigned short num_vertices = read_short(file->ptr, file_end, machine_end);

	if (num_vertices <= 0)
		return length;
	*V = calloc(num_vertices, sizeof(cl_float3));
	printf("\t%hu vertices\n", num_vertices);
	for (int i = 0; i < num_vertices; i++)
	{
		(*V)[i].x = read_float(file->ptr, file_end, machine_end);
		(*V)[i].y = read_float(file->ptr, file_end, machine_end);
		(*V)[i].z = read_float(file->ptr, file_end, machine_end);
		if ((*V)[i].x < min.x)
			min.x = (*V)[i].x;
		if ((*V)[i].y < min.y)
			min.y = (*V)[i].y;
		if ((*V)[i].z < min.z)
			min.z = (*V)[i].z;
		if ((*V)[i].x > max.x)
			max.x = (*V)[i].x;
		if ((*V)[i].y > max.y)
			max.y = (*V)[i].y;
		if ((*V)[i].z > max.z)
			max.z = (*V)[i].z;
	}
	return length;
}

static int read_faces_descript(File_info *file, int file_end, int machine_end, cl_float3 **V, Faces_list **faces)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	short id;
	long chunk_beginning = ftell(file->ptr);
	unsigned short num_faces = read_short(file->ptr, file_end, machine_end);

	if (num_faces <= 0)
		return length;
	Faces_list *link = calloc(1, sizeof(Faces_list));
	link->faces = calloc(num_faces, sizeof(Face));
	link->num_faces = num_faces;
	printf("\t%hu faces\n", num_faces);
	unsigned short a, b, c, flags;
	for (int i = 0; i < num_faces; i++)
	{
		a = read_short(file->ptr, file_end, machine_end);
		b = read_short(file->ptr, file_end, machine_end);
		c = read_short(file->ptr, file_end, machine_end);
		flags = read_short(file->ptr, file_end, machine_end);
/*		if ((flags & 0x4))
			printf("a->b\n");
		if ((flags & 0x2))
			printf("b->c\n");
		if ((flags & 0x1))
			printf("a->c\n");*/

		link->faces[i].verts[0] = (*V)[a];
		link->faces[i].verts[1] = (*V)[b];
		link->faces[i].verts[2] = (*V)[c];
		link->faces[i].shape = 3;
		link->faces[i].mat_type = GPU_MAT_DIFFUSE;
		link->faces[i].mat_ind = 0;
		link->faces[i].center = vec_scale(vec_add(vec_add((*V)[a], (*V)[b]), (*V)[c]), 1.0f / 3.0f);
		cl_float3 N = unit_vec(cross(vec_sub((*V)[b], (*V)[a]), vec_sub((*V)[c], (*V)[a])));
		link->faces[i].norms[0] = N;
		link->faces[i].norms[1] = N;
		link->faces[i].norms[2] = N;
		link->faces[i].N = N;
		link->faces[i].tex[0] = (cl_float3){0, 0, 0};
		link->faces[i].tex[1] = (cl_float3){0, 0, 0};
		link->faces[i].tex[2] = (cl_float3){0, 0, 0};
		link->faces[i].smoothing = 0;
		link->faces[i].next = NULL;
	//	printf("%hu %hu %hu\n", a, b, c);
	}
	link->next = *faces;
	*faces = link;
	free(*V);

	counter += (2 * 4 * num_faces) + 2;
	while (counter + 6L < length)
	{
		id = read_short(file->ptr, file_end, machine_end);
		printf("  id=%hx\n", id);
		if (id == 0x4130)
			counter += read_faces_material(file, file_end, machine_end);
		else if (id == 0x4150)
			counter += read_smoothing_list(file, file_end, machine_end, num_faces);
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}

	return length;
}

static int read_map_coords(File_info *file, int file_end, int machine_end)
{
	int length = read_int(file->ptr, file_end, machine_end);
	long chunk_beginning = ftell(file->ptr);
	unsigned short num_coords = read_short(file->ptr, file_end, machine_end);

	if (num_coords <= 0)
		return length;
	printf("\t%hu coords\n", num_coords);
	float u, v;
	for (int i = 0; i < num_coords; i++)
	{
		u = read_float(file->ptr, file_end, machine_end);
		v = read_float(file->ptr, file_end, machine_end);
	}
	return length;
}

static int read_faces_material(File_info *file, int file_end, int machine_end)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	short id;
	long chunk_beginning = ftell(file->ptr);
	char name[50];

	for (int i = 0; (name[i] = read_char(file->ptr, file_end, machine_end)) != '\0'; i++)
		;
	printf("name=%s\n", name);
	counter += strlen(name) + 1;
	unsigned short faces_of_obj = read_short(file->ptr, file_end, machine_end);
	printf("\t%hu faces of obj\n", faces_of_obj);
	unsigned short index;
	for (int i = 0; i < faces_of_obj; i++)
		index = read_short(file->ptr, file_end, machine_end);
	return length;
}

static int read_smoothing_list(File_info *file, int file_end, int machine_end, int num_faces)
{
	int length = read_int(file->ptr, file_end, machine_end);

	unsigned long smoothing;
	for (int i = 0; i < num_faces; i++)
	{
		smoothing = read_int(file->ptr, file_end, machine_end);
	//	if (smoothing != 1)
	//		printf("smoothing group for face %d is %lu\n", i, smoothing);
	}
	return length;
}
