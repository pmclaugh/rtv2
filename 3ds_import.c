#include "rt.h"
#define VERBOSE 0

static cl_float3 min;
static cl_float3 max;

typedef struct	s_faces_link_list
{
	int		num_faces;
	Face	*faces;
	struct s_faces_link_list *next;
}				Faces_list;

typedef struct	s_material_link_list
{
	Material mat;
	struct s_material_link_list *next;
}				Mats_list;

static Face *file_3ds_import(char *file_3ds, File_edits edit_info, int *face_count, Material **mats, int *mat_count);
static int read_edit_chunk(File_info *file, int file_end, int machine_end, Faces_list **faces, Mats_list **mats, char *rel_path);
static int read_material_block(File_info *file, int file_end, int machine_end, Mats_list **mats, char *rel_path);
static int read_texture(File_info *file, int file_end, int machine_end, Material *mat, char *rel_path);
static int read_diff_rgb(File_info *file, int file_end, int machine_end, Material *mat);
static int read_object_block(File_info *file, int file_end, int machine_end, Faces_list **faces, Mats_list *mats);
static int read_triangular_mesh(File_info *file, int file_end, int machine_end, Faces_list **faces, Mats_list *mats);
static int read_vertices_list(File_info *file, int file_end, int machine_end, cl_float3 **V, unsigned short *num_vert);
static int read_faces_descript(File_info *file, int file_end, int machine_end, cl_float3 **V, cl_float3 **VT, unsigned short num_coord, Faces_list **faces, Mats_list *mats);
static int read_map_coords(File_info *file, int file_end, int machine_end, cl_float3 **VT, unsigned short *num_coord);
static int read_local_coords(File_info *file, int file_end, int machine_end, cl_float3 **V, unsigned short num_vert);
static int read_faces_material(File_info *file, int file_end, int machine_end, Face *faces, Mats_list *mats);
static int read_smoothing_list(File_info *file, int file_end, int machine_end, int num_faces);
static Face *list_to_array(Faces_list **faces, int *face_count);
static void list_to_array_mats(Mats_list **mats_list, Material **mats, int *mat_count);
static void apply_edits(Face *faces, File_edits edit_info, int face_count);

Scene	*scene_from_3ds(char *rel_path, char *filename, File_edits edit_info)
{
	char *file = malloc(strlen(rel_path) + strlen(filename) + 1);

	strcpy(file, rel_path);
	strcat(file, filename);
	Scene *S = calloc(1, sizeof(Scene));
	min = (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX};
	max = (cl_float3){FLT_MIN, FLT_MIN, FLT_MIN};
	
	S->faces = file_3ds_import(file, edit_info, &S->face_count, &S->materials, &S->mat_count);
	free(file);

	if (S->mat_count != 0)
		return S;
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
	S->materials[0].map_Kd_path = NULL;
	S->materials[0].map_Ks = NULL;
	S->materials[0].map_Ks_path = NULL;
    S->materials[0].map_Ke = NULL;
    S->materials[0].map_Ke_path = NULL;
	S->materials[0].Ns.x = 10;
	S->materials[0].Ni = edit_info.ior;
	S->materials[0].roughness = edit_info.roughness;
	S->materials[0].Tr = edit_info.transparency;
	S->materials[0].d = 1 - edit_info.transparency;
	S->materials[0].Tf = (cl_float3){1, 1, 1};
	S->materials[0].illum = 2;
    S->materials[0].Ka = (cl_float3){0.58f, 0.58f, 0.58f}; //ambient mask
    S->materials[0].Kd = edit_info.Kd; //diffuse mask
    S->materials[0].Ks = edit_info.Ks;
    S->materials[0].Ke = edit_info.Ke;
    S->materials[0].metallic = edit_info.metallic;
    S->materials[0].scatter = edit_info.scatter;
    S->materials[0].Kss = edit_info.Kss;
	return S;
}

static Face *file_3ds_import(char *file_3ds, File_edits edit_info, int *face_count, Material **mats, int *mat_count)
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
	if (VERBOSE)
		printf("this machine is %s endian\n\n", (endian) ? "big" : "little");
	
	int file_size;
	long counter = 6L;
	unsigned short id;
	int length;
	long int chunk_beginning;

	// main3ds
	id = read_ushort(file.ptr, 0, endian);
	length = read_int(file.ptr, 0, endian);
	if (VERBOSE)
		printf("id=%hx\n", id);
	char rel_path[256];
	strncpy(rel_path, file.name, strrchr(file.name, '/') + 1 - file.name);
	file_size = length;

	Faces_list *faces = NULL;
	Mats_list *mats_list = NULL;
	while (counter < file_size)
	{
		chunk_beginning = ftell(file.ptr);
		id = read_ushort(file.ptr, 0, endian);
		if (VERBOSE)
			printf("id=%hx\n", id);
		if (id == 0x3d3d)
			counter += read_edit_chunk(&file, 0, endian, &faces, &mats_list, rel_path);
		else
			counter += read_int(file.ptr, 0, endian);

		fseek(file.ptr, counter, SEEK_SET);
	}
	fclose(file.ptr);
	Face *face_arr =  list_to_array(&faces, face_count);
	list_to_array_mats(&mats_list, mats, mat_count);
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
		
		faces[i].center = vec_scale(vec_add(vec_add(faces[i].verts[0], faces[i].verts[1]), faces[i].verts[2]), 1.0f / 3.0f);
		cl_float3 N = unit_vec(cross(vec_sub(faces[i].verts[1], faces[i].verts[0]), vec_sub(faces[i].verts[2], faces[i].verts[0])));
		faces[i].norms[0] = N;
		faces[i].norms[1] = N;
		faces[i].norms[2] = N;
	}
}

static Face *list_to_array(Faces_list **faces, int *face_count)
{
	Faces_list *temp = *faces;

	while (temp != NULL)
	{
		*face_count += temp->num_faces;
		temp = temp->next;
	}
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

static void list_to_array_mats(Mats_list **mats_list, Material **mats, int *mat_count)
{
	Mats_list *temp = *mats_list;
	
	while (temp != NULL)
	{
		(*mat_count)++;
		temp = temp->next;
	}
	if (*mat_count == 0)
		return ;
	*mats = calloc(*mat_count, sizeof(Material));
	temp = *mats_list;
	for (int i = 0; i < *mat_count; i++)
	{
		memcpy((void*)&(*mats)[i], (void*)&temp->mat, sizeof(Material));
		temp = temp->next;
	}
	while (*mats_list != NULL)
	{
		temp = (*mats_list)->next;
		free(*mats_list);
		*mats_list = temp;
	}
}

static int read_edit_chunk(File_info *file, int file_end, int machine_end, Faces_list **faces, Mats_list **mats, char *rel_path)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);

	while (counter + 6L < length)
	{
		id = read_ushort(file->ptr, file_end, machine_end);
		if (VERBOSE)
			printf("id=%hx\n", id);
		if (id == 0x4000)
			counter += read_object_block(file, file_end, machine_end, faces, *mats);
		else if (id == 0xafff)
			counter += read_material_block(file, file_end, machine_end, mats, rel_path);
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_material_block(File_info *file, int file_end, int machine_end, Mats_list **mats, char *rel_path)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);

	Mats_list *link = calloc(1, sizeof(Mats_list));
	link->mat.friendly_name = calloc(256, sizeof(char));
	link->mat.map_Ka_path = NULL;
	link->mat.map_Ka = NULL;
	link->mat.map_Kd_path = NULL;
	link->mat.map_Kd = NULL;
	link->mat.map_bump_path = NULL;
	link->mat.map_bump = NULL;
	link->mat.map_d_path = NULL;
	link->mat.map_d = NULL;
	link->mat.map_Ks_path = NULL;
	link->mat.map_Ks = NULL;
	link->mat.map_Ke_path = NULL;
	link->mat.map_Ke = NULL;
	while (counter + 6L < length)
	{
		id = read_ushort(file->ptr, file_end, machine_end);
		if (VERBOSE)
			printf("  id=%hx\n", id);
		if (id == 0xa000)
		{
			counter += read_int(file->ptr, file_end, machine_end);
			for (int i = 0; (link->mat.friendly_name[i] = read_char(file->ptr, file_end, machine_end)) != '\0'; i++)
				;
			if (VERBOSE)
				printf("    mat.name=%s\n", link->mat.friendly_name);
		}
		else if (id == 0xa200 || id == 0xa33a)
			counter += read_texture(file, file_end, machine_end, &(link->mat), rel_path);
		else if (id == 0xa020)
			counter += read_diff_rgb(file, file_end, machine_end, &(link->mat));
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	link->next = *mats;
	*mats = link;
	return length;
}

static int read_texture(File_info *file, int file_end, int machine_end, Material *mat, char *rel_path)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);

	while (counter + 6L < length)
	{
		id = read_ushort(file->ptr, file_end, machine_end);
		if (VERBOSE)
			printf("    id=%hx\n", id);
		if (id == 0xa300)
		{
			mat->map_Kd_path = calloc(512, sizeof(char));
			counter += read_int(file->ptr, file_end, machine_end);
			for (int i = 0; (mat->map_Kd_path[i] = read_char(file->ptr, file_end, machine_end)) != '\0'; i++)
				;
			if (VERBOSE)
				printf("      mat.name=%s\n", mat->map_Kd_path);
			if (strstr(mat->map_Kd_path, ".bmp") || strstr(mat->map_Kd_path, ".BMP"))
				mat->map_Kd = load_bmp_map(rel_path, mat->map_Kd_path);
			else if (strstr(mat->map_Kd_path, ".tga") || strstr(mat->map_Kd_path, ".TGA"))
				mat->map_Kd = load_tga_map(rel_path, mat->map_Kd_path);
			else if (strstr(mat->map_Kd_path, ".jpg") || strstr(mat->map_Kd_path, ".jpeg") || strstr(mat->map_Kd_path, ".JPG") || strstr(mat->map_Kd_path, ".JPEG"))
				mat->map_Kd = load_jpg_map(rel_path, mat->map_Kd_path);
		}
		else if (id == 0xa351)
		{
			counter += read_int(file->ptr, file_end, machine_end);
			unsigned short flag = read_ushort(file->ptr, file_end, machine_end);
		}
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_diff_rgb(File_info *file, int file_end, int machine_end, Material *mat)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);

	while (counter + 6L < length)
	{
		id = read_ushort(file->ptr, file_end, machine_end);
		if (VERBOSE)
			printf("    id=%hx\n", id);
		if (id == 0x0011)
		{
			counter += read_int(file->ptr, file_end, machine_end);
			unsigned char r, g, b;
			r = read_uchar(file->ptr, file_end, machine_end);
			g = read_uchar(file->ptr, file_end, machine_end);
			b = read_uchar(file->ptr, file_end, machine_end);
			if (VERBOSE)
				printf("      r=%3u, g=%3u, b=%3u \n", r, g, b);
			mat->Kd = (cl_float3){(float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f};
		}
		else if (id == 0x0010)
		{
			counter += read_int(file->ptr, file_end, machine_end);
			float r, g, b;
			r = read_float(file->ptr, file_end, machine_end);
			g = read_float(file->ptr, file_end, machine_end);
			b = read_float(file->ptr, file_end, machine_end);
			if (VERBOSE)
				printf("      r=%3.2f, g=%3.2f, b=%3.2f \n", r, g, b);
			mat->Kd = (cl_float3){r, g, b};
		}
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_object_block(File_info *file, int file_end, int machine_end, Faces_list **faces, Mats_list *mats)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);
	char name[50];

	for (int i = 0; (name[i] = read_char(file->ptr, file_end, machine_end)) != '\0'; i++)
		;
	if (VERBOSE)
		printf("obj.name=%s\n", name);
	counter += strlen(name) + 1;

	while (counter + 6L < length)
	{
		id = read_ushort(file->ptr, file_end, machine_end);
		if (VERBOSE)
			printf("  id=%hx\n", id);
		if (id == 0x4100)
			counter += read_triangular_mesh(file, file_end, machine_end, faces, mats);
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_triangular_mesh(File_info *file, int file_end, int machine_end, Faces_list **faces, Mats_list *mats)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);
	cl_float3 *V = NULL;
	cl_float3 *VT = NULL;
	unsigned short num_vert = 0;
	unsigned short num_coord = 0;

	while (counter + 6L < length)
	{
		id = read_ushort(file->ptr, file_end, machine_end);
		if (VERBOSE)
			printf("    id=%hx\n", id);
		if (id == 0x4110)
			counter += read_vertices_list(file, file_end, machine_end, &V, &num_vert);// vertices list
		else if (id == 0x4120)
			counter += read_faces_descript(file, file_end, machine_end, &V, &VT, num_coord, faces, mats);// Faces description
		else if (id == 0x4140)
			counter += read_map_coords(file, file_end, machine_end, &VT, &num_coord);// Mapping coordinates list
		else if (id == 0x4160)
			counter += read_local_coords(file, file_end, machine_end, &V, num_vert);// Local coordinates system
		else
			counter += read_int(file->ptr, file_end, machine_end);

		fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}
	return length;
}

static int read_vertices_list(File_info *file, int file_end, int machine_end, cl_float3 **V, unsigned short *num_vert)
{
	int length = read_int(file->ptr, file_end, machine_end);
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);
	unsigned short num_vertices = read_ushort(file->ptr, file_end, machine_end);
	*num_vert = 0;

	if (num_vertices == 0)
		return length;
	*num_vert = num_vertices;
	*V = calloc(num_vertices, sizeof(cl_float3));
	if (VERBOSE)
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

static int read_faces_descript(File_info *file, int file_end, int machine_end, cl_float3 **V, cl_float3 **VT, unsigned short num_coord, Faces_list **faces, Mats_list *mats)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	unsigned short id;
	long chunk_beginning = ftell(file->ptr);
	unsigned short num_faces = read_ushort(file->ptr, file_end, machine_end);

	if (num_faces == 0)
		return length;
	Faces_list *link = calloc(1, sizeof(Faces_list));
	link->faces = calloc(num_faces, sizeof(Face));
	link->num_faces = num_faces;
	if (VERBOSE)
		printf("    \t%hu faces\n", num_faces);
	unsigned short a, b, c, flags;
	for (int i = 0; i < num_faces; i++)
	{
		a = read_ushort(file->ptr, file_end, machine_end);
		b = read_ushort(file->ptr, file_end, machine_end);
		c = read_ushort(file->ptr, file_end, machine_end);
		flags = read_ushort(file->ptr, file_end, machine_end);
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

		if (num_coord == 0)
		{
			link->faces[i].tex[0] = (cl_float3){0, 0, 0};
			link->faces[i].tex[1] = (cl_float3){0, 0, 0};
			link->faces[i].tex[2] = (cl_float3){0, 0, 0};
		}
		else
		{
			link->faces[i].tex[0] = (*VT)[a];
			link->faces[i].tex[1] = (*VT)[b];
			link->faces[i].tex[2] = (*VT)[c];
		}
		
		link->faces[i].smoothing = 0;
		link->faces[i].next = NULL;
	}
	link->next = *faces;
	*faces = link;
	free(*V);
	free(*VT);

	counter += (2 * 4 * num_faces) + 2;
	while (counter + 6L < length)
	{
		id = read_ushort(file->ptr, file_end, machine_end);
		if (VERBOSE)
			printf("      id=%hx\n", id);
		if (id == 0x4130)
			counter += read_faces_material(file, file_end, machine_end, link->faces, mats);
		else if (id == 0x4150)
			counter += read_smoothing_list(file, file_end, machine_end, num_faces);
		else
			counter += read_int(file->ptr, file_end, machine_end);

	fseek(file->ptr, chunk_beginning + counter, SEEK_SET);
	}

	return length;
}

static int read_map_coords(File_info *file, int file_end, int machine_end, cl_float3 **VT, unsigned short *num_coord)
{
	int length = read_int(file->ptr, file_end, machine_end);
	long chunk_beginning = ftell(file->ptr);
	*num_coord = read_ushort(file->ptr, file_end, machine_end);

	if (*num_coord == 0)
		return length;
	*VT = calloc(*num_coord, sizeof(cl_float3));
	if (VERBOSE)
		printf("\t%hu coords\n", *num_coord);
	for (int i = 0; i < *num_coord; i++)
	{
		(*VT)[i].x = read_float(file->ptr, file_end, machine_end);
		(*VT)[i].y = -1 * read_float(file->ptr, file_end, machine_end);
		(*VT)[i].z = 0;
	}
	return length;
}

static int read_local_coords(File_info *file, int file_end, int machine_end, cl_float3 **V, unsigned short num_vert)
{
	int length = read_int(file->ptr, file_end, machine_end);
	long chunk_beginning = ftell(file->ptr);
	t_3x3 mat;
	cl_float3 row4;
	float temp_row[3];

	for (int j = 0; j < 4; j++)
	{
		for (int i = 0; i < 3; i++)
			temp_row[i] = read_float(file->ptr, file_end, machine_end);

		if (j == 0)
			mat.row1 = (cl_float3){temp_row[0], temp_row[1], temp_row[2]};// X axis
		else if (j == 1)
			mat.row2 = (cl_float3){temp_row[0], temp_row[1], temp_row[2]};// Y axis
		else if (j == 2)
			mat.row3 = (cl_float3){temp_row[0], temp_row[1], temp_row[2]};// Z axis
		else if (j == 3)
			row4 = (cl_float3){temp_row[0], temp_row[1], temp_row[2]};// local center
	}
	if (VERBOSE)
	{
		printf ("     The translation matrix is:\n");
		print_3x3(mat);
		print_vec(row4);
	}
	return length;
}

static int read_faces_material(File_info *file, int file_end, int machine_end, Face *faces, Mats_list *mats)
{
	int length = read_int(file->ptr, file_end, machine_end);
	int counter = 0;
	long chunk_beginning = ftell(file->ptr);
	char name[50];

	for (int i = 0; (name[i] = read_char(file->ptr, file_end, machine_end)) != '\0'; i++)
		;
	if (VERBOSE)
		printf("      face.mat.name=%s\n", name);
	counter += strlen(name) + 1;
	
	Mats_list *temp = mats;
	int mat_i = -1;
	while (temp != NULL)
	{
		++mat_i;
		if (strcmp(name, temp->mat.friendly_name) == 0)
			break ;
		temp = temp->next;
	}
	
	unsigned short faces_of_obj = read_ushort(file->ptr, file_end, machine_end);
	if (VERBOSE)
		printf("      \t%hu faces of obj\n", faces_of_obj);
	unsigned short index;
	for (int i = 0; i < faces_of_obj; i++)
	{
		index = read_ushort(file->ptr, file_end, machine_end);
		faces[index].mat_ind = mat_i;
	}
	return length;
}

static int read_smoothing_list(File_info *file, int file_end, int machine_end, int num_faces)
{
	int length = read_int(file->ptr, file_end, machine_end);

	unsigned int smoothing;
	for (int i = 0; i < num_faces; i++)
		smoothing = read_uint(file->ptr, file_end, machine_end);
	return length;
}
