#include "rt.h"

enum DataType {Char, Uchar, Short, Ushort, Int, Uint, Float, Double};

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

	S->materials[0].metallic = edit_info.metallic;
	S->materials[0].scatter = edit_info.scatter;
	S->materials[0].Kss = edit_info.Kss;

	return S;
}

static Face *read_binary_file(FILE *fp, Elements *elems, int elem_total, int f_total, int v_total, File_edits edit_info, int file_endian)
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

	for (int j = 0; j < elem_total; j++)
	{
		if (strncmp(elems[j].name, "vertex", 6) == 0)
		{
			for (int i = 0; i < v_total; i++)
			{
				for (int k = 0; k < elems[j].property_total; k++)
				{
					if (strncmp(elems[j].props[k].name, "x", 1) == 0)
						V[i].x = read_float(fp, file_endian, endian);
					else if (strncmp(elems[j].props[k].name, "y", 1) == 0)
						V[i].y = read_float(fp, file_endian, endian);
					else if (strncmp(elems[j].props[k].name, "z", 1) == 0)
						V[i].z = read_float(fp, file_endian, endian);
					else
					{
						if (elems[j].props[k].n != 1)
						{
							if (elems[j].props[k].n_datatype == Char)
								elems[j].props[k].n = (uint32_t)read_char(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Uchar)
								elems[j].props[k].n = (uint32_t)read_uchar(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Short)
								elems[j].props[k].n = (uint32_t)read_short(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Ushort)
								elems[j].props[k].n = (uint32_t)read_ushort(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Int)
								elems[j].props[k].n = (uint32_t)read_int(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Uint)
								elems[j].props[k].n = (uint32_t)read_uint(fp, file_endian, endian);
						}
						for (int x = 0; x < elems[j].props[k].n; x++)
						{
							if (elems[j].props[k].datatype == Char)
							{
								char val = read_char(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Uchar)
							{
								unsigned char val = read_uchar(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Short)
							{
								int16_t val = read_short(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Ushort)
							{
								uint16_t val = read_ushort(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Int)
							{
								int32_t val = read_int(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Uint)
							{
								uint32_t val = read_uint(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Float)
							{
								float val = read_float(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Double)
							{
								double val = read_double(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
						}
					}
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
			for (int i =  0; i < v_total; i++)
			{
				V[i] = vec_sub(V[i], center);
				V[i] = vec_scale(V[i], edit_info.scale);
				vec_rot(edit_info.rotate, &V[i]);
				V[i] = vec_add(V[i], edit_info.translate);
			}
		}
		else if (strncmp(elems[j].name, "face", 4) == 0)
		{
			uint8_t n;
			for (int i = 0; i < f_total; i++)
			{
				for (int k = 0; k < elems[j].property_total; k++)
				{
					if (strstr(elems[j].props[k].name, "vertex_indices"))
					{
						int a, b, c;
						n = (uint8_t)read_uchar(fp, file_endian, endian);
						if (n != 3)
							printf("this polygon has %hhu vertices\n", n);
						for (int xyz = 0; xyz < n; xyz++)
						{
							if (xyz == 0)
								a = read_int(fp, file_endian, endian);
							else if (xyz == 1)
								b = read_int(fp, file_endian, endian);
							else
								c = read_int(fp, file_endian, endian);
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
					else
					{
						if (elems[j].props[k].n != 1)
						{
							if (elems[j].props[k].n_datatype == Char)
								elems[j].props[k].n = (uint32_t)read_char(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Uchar)
								elems[j].props[k].n = (uint32_t)read_uchar(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Short)
								elems[j].props[k].n = (uint32_t)read_short(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Ushort)
								elems[j].props[k].n = (uint32_t)read_ushort(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Int)
								elems[j].props[k].n = (uint32_t)read_int(fp, file_endian, endian);
							else if (elems[j].props[k].n_datatype == Uint)
								elems[j].props[k].n = (uint32_t)read_uint(fp, file_endian, endian);
						}
						for (int x = 0; x < elems[j].props[k].n; x++)
						{
							if (elems[j].props[k].datatype == Char)
							{
								char val = read_char(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Uchar)
							{
								unsigned char val = read_uchar(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Short)
							{
								int16_t val = read_short(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Ushort)
							{
								uint16_t val = read_ushort(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Int)
							{
								int32_t val = read_int(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Uint)
							{
								uint32_t val = read_uint(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Float)
							{
								float val = read_float(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
							else if (elems[j].props[k].datatype == Double)
							{
								double val = read_double(fp, file_endian, endian);
								elems[j].props[k].data = &val;
							}
						}
					}
				}
			}
		}
	}
	free(V);
	return list;
}

static Face *read_ascii_file(FILE *fp, Elements *elems, int elem_total, int f_total, int v_total, File_edits edit_info)
{
	Face *list = NULL;
	list = calloc(f_total, sizeof(Face));
	cl_float3 *V = calloc(v_total, sizeof(cl_float3));
	char *line = malloc(512);
	cl_float3 min = (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX};
	cl_float3 max = (cl_float3){FLT_MIN, FLT_MIN, FLT_MIN};
	cl_float3 center;

	for (int j = 0; j < elem_total; j++)
	{
		if (strncmp(elems[j].name, "vertex", 6) == 0)
		{
			for (int i = 0; i < v_total; i++)
			{
				fgets(line, 512, fp);
				char *ptr = line;
				for (int k = 0; k < elems[j].property_total; k++)
				{
					if (strncmp(elems[j].props[k].name, "x", 1) == 0)
					{
						char temp[256];
						sscanf(ptr, " %f %f %f", &V[i].x, &V[i].y, &V[i].z);
						sscanf(ptr, " %s", temp);
						ptr = strstr(ptr, temp) + strlen(temp);
						sscanf(ptr, " %s", temp);
						ptr = strstr(ptr, temp) + strlen(temp);
						sscanf(ptr, " %s", temp);
						ptr = strstr(ptr, temp) + strlen(temp);
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
				}
			}
			center = vec_add(vec_scale(vec_sub(max, min), 0.5), min);
			for (int i = 0; i < v_total; i++)
			{
				V[i] = vec_sub(V[i], center);
				V[i] = vec_scale(V[i], edit_info.scale);
				vec_rot(edit_info.rotate, &V[i]);
				V[i] = vec_add(V[i], edit_info.translate);
			}
		}
		else if (strncmp(elems[j].name, "face", 4) == 0)
		{
			for (int i = 0; i < f_total; i++)
			{
				fgets(line, 512, fp);
				char *ptr = line;
				int n = 0;
				//now faces
				// k is not initialized
				for (int k = 0; k < elems[j].property_total; k++)
				{
					if (strstr(elems[j].props[k].name, "vertex_indices"))
					{
						int a, b, c, d, e;
						sscanf(ptr, " %d", &n);
						ptr = move_str(ptr, itoa(n), FREE);
						if (n == 3)
							sscanf(ptr, " %d %d %d", &a, &b, &c);
						else if (n == 4)
							sscanf(ptr, " %d %d %d %d", &a, &b, &c, &d);
						else if (n == 5)
							sscanf(ptr, " %d %d %d %d %d", &a, &b, &c, &d, &e);
						else
							printf("can't handle %d vertices\n", n);
						ptr = move_str(ptr, itoa(a), FREE);
						ptr = move_str(ptr, itoa(b), FREE);
						ptr = move_str(ptr, itoa(c), FREE);
						if (n == 4 || n == 5)
						{
							ptr = move_str(ptr, itoa(d), FREE);
							if (n == 5)
								ptr = move_str(ptr, itoa(e), FREE);
						}
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

						face.tex[0] = list[i].tex[0];
						face.tex[1] = list[i].tex[1];
						face.tex[2] = list[i].tex[2];

						face.next = NULL;
						list[i] = face;

						if (n == 4 || n == 5)
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
						if (n == 5)
							{
							Face face;
							face.shape = 3;
							face.mat_type = GPU_MAT_DIFFUSE;
							face.mat_ind = 0; //some default material
							face.verts[0] = V[a];
							face.verts[1] = V[d];
							face.verts[2] = V[e];

							face.center = vec_scale(vec_add(vec_add(V[a], V[d]), V[e]), 1.0 / (float)face.shape);

							cl_float3 N = cross(vec_sub(V[d], V[a]), vec_sub(V[e], V[a]));
							face.norms[0] = N;
							face.norms[1] = N;
							face.norms[2] = N;

							face.next = NULL;
							list[++i] = face;
						}
					}
					else if (strstr(elems[j].props[k].name, "coord"))
					{
						if (elems[j].props[k].n != 1)
						{
							sscanf(ptr, " %d", &(elems[j].props[k].n));
							ptr = move_str(ptr, itoa(elems[j].props[k].n), FREE);
						}
						float *values = calloc(elems[j].props[k].n, sizeof(float));
						for (int x = 0; x < elems[j].props[k].n; x++)
						{
							char temp[256];
							sscanf(ptr, " %f", &(values[x]));
							sscanf(ptr, " %s", temp);
							ptr = move_str(ptr, temp, 0);
						}
						if (elems[j].props[k].n == n * 2)
						{
							list[i].tex[0] = (cl_float3){values[0], values[1], 0};
							list[i].tex[1] = (cl_float3){values[2], values[3], 0};
							list[i].tex[2] = (cl_float3){values[4], values[5], 0};
						}
						free(values);
					}
				}
			}
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
			*face_count += 1;
		else if (vertices == 5)
			*face_count += 2;
	}
	free(line);
	fseek(file->ptr, file->header_size, SEEK_SET);
}

static void get_properties(File_info *file, Elements *elem, int element_total)
{
	char *line = calloc(512, sizeof(char));
	int i = -1;
	int	size = 0;
	int properties = 0;
	int ret;

	fseek(file->ptr, 0L, SEEK_SET);
	while (fgets(line, 512, file->ptr) && element_total > 0)
	{
		while (strncmp(line, "element", 7) == 0)
		{
			--element_total;
			++i;
			sscanf(line, "element %s %d\n", elem[i].name, &(elem[i]).total);
			while (fgets(line, 512, file->ptr))
			{
				size += strlen(line);
				if (strncmp(line, "property", 8) == 0)
					++properties;
				else
				{
					elem[i].property_total = properties;
					elem[i].props = calloc(properties, sizeof(Property));
					fseek(file->ptr, (size * -1), SEEK_CUR);
					int j = -1;
					char *str1 = calloc(512, sizeof(char));
					char *str2 = calloc(512, sizeof(char));
					char *str3 = calloc(512, sizeof(char));
					while (fgets(line, 512, file->ptr))
					{
						if ((ret = sscanf(line, "property %s %s ", str1, str2)) == 0 || ret == EOF)
							break ;
						if (strncmp(str1, "list", 4) == 0)
						{
							++j;
							sscanf(line, "property list %s %s %s ", str1, str2, str3);
							if (strstr(str1, "char"))
								elem[i].props[j].n_datatype = (strstr(str1, "uchar")) ? Uchar : Char;
							else if (strstr(str1, "short"))
								elem[i].props[j].n_datatype = (strstr(str1, "ushort")) ? Ushort : Short;
							else if (strstr(str1, "int"))
								elem[i].props[j].n_datatype = (strstr(str1, "uint")) ? Uint : Int;
							else if (strstr(str1, "float"))
								elem[i].props[j].n_datatype = Float;
							else if (strstr(str1, "double"))
								elem[i].props[j].n_datatype = Double;
							else
							{
								printf("unknown data type\n");
								exit(EXIT_FAILURE);
							}
							strcpy(str1, str2);
							strcpy(str2, str3);
						}
						else
							elem[i].props[++j].n = 1;
						if (strstr(str1, "char"))
							elem[i].props[j].datatype = (strstr(str1, "uchar")) ? Uchar : Char;
						else if (strstr(str1, "short"))
							elem[i].props[j].datatype = (strstr(str1, "ushort")) ? Ushort : Short;
						else if (strstr(str1, "int"))
							elem[i].props[j].datatype = (strstr(str1, "uint")) ? Uint : Int;
						else if (strstr(str1, "float"))
							elem[i].props[j].datatype = Float;
						else if (strstr(str1, "double"))
							elem[i].props[j].datatype = Double;
						else
						{
							printf("unknown data type\n");
							exit(EXIT_FAILURE);
						}
						strcpy(elem[i].props[j].name, str2);
					}
					free(str1);
					free(str2);
					free(str3);
					fseek(file->ptr, (size * -1), SEEK_CUR);
					fgets(line, 512, file->ptr);
					size = 0;
					properties = 0;
					break;
				}
			}
		}
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
	int	element_total = 0;

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
		{
			sscanf(line, "element vertex %d\n", &v_total);
			++element_total;
		}
		else if (strncmp(line, "element face", 12) == 0)
		{
			sscanf(line, "element face %d\n", &f_total);
			++element_total;
		}
		else if (strncmp(line, "element", 7) == 0)
			++element_total;
		else if (strncmp(line, "end_header", 10) == 0)
			break ;
	}
	free(line);

	Elements *elems = calloc(element_total, sizeof(Elements));	
	get_properties(&file, elems, element_total);
	
	if (file_type == 2)
		check_ascii_face_count(&file, &f_total, v_total);
	*face_count = f_total;
	
	printf("%s: %d vertices, %d faces\n", ply_file, v_total, f_total);

	Face *list;
	if (file_type == 2)
		list = read_ascii_file(file.ptr, elems, element_total, f_total, v_total, edit_info);
	else
		list = read_binary_file(file.ptr, elems, element_total, f_total, v_total, edit_info, file_type);
	for (int i = 0; i < element_total; i++)
		free(elems[i].props);
	free(elems);
	return list;
}
