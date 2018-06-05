#include "rt.h"

#define VERBOSE 0

Map *load_bmp_map(char *rel_path, char *filename)
{
	char *bmp_file = malloc(strlen(rel_path) + strlen(filename) + 1);
	strcpy(bmp_file, rel_path);
	strcat(bmp_file, filename);
	BMP *bmp = BMP_ReadFile(bmp_file);
	
	BMP_CHECK_ERROR( stdout, NULL );

	Map *map = calloc(1, sizeof(Map));
	map->width = (int)BMP_GetWidth(bmp);
	map->height = (int)BMP_GetHeight(bmp);

	map->pixels = calloc(map->height * map->width, sizeof(cl_uchar) * 3);
	int i = 0;
	unsigned char r, g, b;
	for (int y = 0; y < map->height; ++y)
	{
		for (int x = 0; x < map->width; ++x)
		{
			BMP_GetPixelRGB( bmp, x, y, &r, &g, &b );
			map->pixels[i * 3] = (cl_uchar)r;
			map->pixels[i * 3 + 1] = (cl_uchar)g;
			map->pixels[i * 3 + 2] = (cl_uchar)b;
			i++;
		}
	}
	BMP_CHECK_ERROR( stdout, NULL );
	if (VERBOSE)
	{
		printf("loaded texture %s\n", bmp_file);
		printf("%d %d\n", map->height, map->width);
	}
	free(bmp_file);
	BMP_Free(bmp);
	return map;
}

Map *load_jpg_map(char *rel_path, char *filename)
{
	char *jpg_file = malloc(strlen(rel_path) + strlen(filename) + 1);
	strcpy(jpg_file, rel_path);
	strcat(jpg_file, filename);
	FILE *fp = fopen(jpg_file, "rb");
	
	if (fp == NULL)
	{
		if (VERBOSE)
			printf("cannot find file %s\n", jpg_file);
		free(jpg_file);
		return NULL;
	}
	
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);

	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);
	
	int row_stride;
	row_stride = cinfo.output_width * cinfo.output_components;
	
	Map *map = calloc(1, sizeof(Map));
	map->width = (int)cinfo.output_width;
	map->height = (int)cinfo.output_height;

	map->pixels = calloc(map->height * map->width * 3, sizeof(cl_uchar));
	unsigned char *raw_pixels;

	while (cinfo.output_scanline < cinfo.output_height)
	{
		raw_pixels = map->pixels + row_stride * cinfo.output_scanline;
		jpeg_read_scanlines(&cinfo, &raw_pixels, 1);
	}
	if (VERBOSE)
	{
		printf("loaded texture %s\n", jpg_file);
		printf("%d %d\n", map->height, map->width);
	}
	free(jpg_file);
	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(fp);
	return map;
}

Map *load_tga_map(char *rel_path, char *filename)
{
	char *tga_file = malloc(strlen(rel_path) + strlen(filename) + 1);
	strcpy(tga_file, rel_path);
	strcat(tga_file, filename);
	FILE *fp = fopen(tga_file, "r");
	
	if (fp == NULL)
	{
		if (VERBOSE)
			printf("cannot find file %s\n", tga_file);
		free(tga_file);
		return NULL;
	}

	char *header = malloc(18);
	fread(header, 18, 1, fp);

	Map *map = calloc(1, sizeof(Map));
	//12-13 width
	map->width = (unsigned char)header[12] + (unsigned char)header[13] * 256;
	//14-15 height
	map->height = (unsigned char)header[14] + (unsigned char)header[15] * 256;

	free(header);
	map->pixels = calloc(map->height * map->width, sizeof(cl_uchar) * 3);
	unsigned char *raw_pixels = calloc(map->height * map->width, sizeof(unsigned char) * 3);
	fread(raw_pixels, map->height * map->width, sizeof(unsigned char) * 3, fp);

	for (int i = 0; i < map->height * map->width; i++)
	{
		//remap from BGR to RGB
		map->pixels[i * 3] = raw_pixels[i * 3 + 2];
		map->pixels[i * 3 + 1] = raw_pixels[i * 3 + 1];
		map->pixels[i * 3 + 2] = raw_pixels[i * 3];
	}

	fclose(fp);
	free(raw_pixels);
	if (VERBOSE)
	{
		printf("loaded texture %s\n", tga_file);
		printf("%d %d\n", map->height, map->width);
	}
	free(tga_file);
	return map;
}

void load_mats(Scene *S, char *rel_path, char *filename)
{
	char *mtl_file = malloc(strlen(rel_path) + strlen(filename) + 1);
	strcpy(mtl_file, rel_path);
	strcat(mtl_file, filename);
	FILE *fp = fopen(mtl_file, "r");
	free(mtl_file);

	char *line = calloc(512, 1);
	int mtl_count = 0;
	//count how many materials
	while (fgets(line, 512, fp))
		if (strncmp(line, "newmtl", 6) == 0)
			mtl_count++;
	S->materials = calloc(mtl_count, sizeof(Material));
	S->mat_count = mtl_count;
	fseek(fp, 0, SEEK_SET);
	int mat_ind = -1;
	Material m;
	printf("\n");
	char *str;
	while (fgets(line, 512, fp))
	{
		str = strtrim(line);
		if (strncmp(str, "newmtl ", 7) == 0)
		{
			if (mat_ind >= 0)
			{
				S->materials[mat_ind] = m;
				if (VERBOSE)
					printf("loaded material %s\n", m.friendly_name);
			}
			mat_ind++;
			bzero(&m, sizeof(Material));
			m.friendly_name = malloc(256);
			m.map_Ka_path = NULL;
			m.map_Ka = NULL;
			m.map_Kd_path = NULL;
			m.map_Kd = NULL;
			m.map_bump_path = NULL;
			m.map_bump = NULL;
			m.map_d_path = NULL;
			m.map_d = NULL;
			m.map_Ks_path = NULL;
			m.map_Ks = NULL;
			m.map_Ke_path = NULL;
			m.map_Ke = NULL;
			sscanf(str, "newmtl %s\n", m.friendly_name);
			if (VERBOSE)
				printf("new mtl friendly name %s\n", m.friendly_name);
		}
		else if (strncmp(str, "Ns", 2) == 0)
			sscanf(str, "Ns %f", &m.Ns.x);
		else if (strncmp(str, "Ni", 2) == 0)
			sscanf(str, "Ni %f", &m.Ni);
		else if (strncmp(str, "d ", 2) == 0)
			sscanf(str, "d %f", &m.d);
		else if (strncmp(str, "Tr", 2) == 0)
			sscanf(str, "Tr %f", &m.Tr);
		else if (strncmp(str, "Tf", 2) == 0)
			sscanf(str, "Tf %f %f %f", &m.Tf.x, &m.Tf.y, &m.Tf.z);
		else if (strncmp(str, "illum", 5) == 0)
			sscanf(str, "illum %d", &m.illum);
		else if (strncmp(str, "Ka", 2) == 0)
			sscanf(str, "Ka %f %f %f", &m.Ka.x, &m.Ka.y, &m.Ka.z);
		else if (strncmp(str, "Kd", 2) == 0)
			sscanf(str, "Kd %f %f %f", &m.Kd.x, &m.Kd.y, &m.Kd.z);
		else if (strncmp(str, "Ks", 2) == 0)
			sscanf(str, "Ks %f %f %f", &m.Ks.x, &m.Ks.y, &m.Ks.z);
		else if (strncmp(str, "Ke", 2) == 0)
			sscanf(str, "Ke %f %f %f", &m.Ke.x, &m.Ke.y, &m.Ke.z);

		else if (strncmp(str, "map_Ka", 6) == 0)
		{
			//printf("loading a Ka\n");
			m.map_Ka_path = calloc(512, 1);
			sscanf(str, "map_Ka %s\n", m.map_Ka_path);
			if (strstr(m.map_Ka_path, ".bmp") || strstr(m.map_Ka_path, ".BMP"))
				m.map_Ka = load_bmp_map(rel_path, m.map_Ka_path);
			else if (strstr(m.map_Ka_path, ".tga") || strstr(m.map_Ka_path, ".TGA"))
				m.map_Ka = load_tga_map(rel_path, m.map_Ka_path);
			else if (strstr(m.map_Ka_path, ".jpg") || strstr(m.map_Ka_path, ".jpeg") || strstr(m.map_Ka_path, ".JPG") || strstr(m.map_Ka_path, ".JPEG"))
				m.map_Ka = load_jpg_map(rel_path, m.map_Ka_path);
		}
		else if (strncmp(str, "map_Kd", 6) == 0)
		{
			//printf("loading a Kd\n");
			m.map_Kd_path = calloc(512, 1);
			sscanf(str, "map_Kd %s\n", m.map_Kd_path);
			if (strstr(m.map_Kd_path, ".bmp") || strstr(m.map_Kd_path, ".BMP"))
				m.map_Kd = load_bmp_map(rel_path, m.map_Kd_path);
			else if (strstr(m.map_Kd_path, ".tga") || strstr(m.map_Kd_path, ".TGA"))
				m.map_Kd = load_tga_map(rel_path, m.map_Kd_path);
			else if (strstr(m.map_Kd_path, ".jpg") || strstr(m.map_Kd_path, ".jpeg") || strstr(m.map_Kd_path, ".JPG") || strstr(m.map_Kd_path, ".JPEG"))
				m.map_Kd = load_jpg_map(rel_path, m.map_Kd_path);
		}
		else if (strncmp(str, "map_bump", 8) == 0)
		{
			//printf("loading a bump\n");
			m.map_bump_path = calloc(512, 1);
			sscanf(str, "map_bump %s\n", m.map_bump_path);
			if (strstr(m.map_bump_path, ".bmp") || strstr(m.map_bump_path, ".BMP"))
				m.map_bump = load_bmp_map(rel_path, m.map_bump_path);
			else if (strstr(m.map_bump_path, ".tga") || strstr(m.map_bump_path, ".TGA"))
				m.map_bump = load_tga_map(rel_path, m.map_bump_path);
			else if (strstr(m.map_bump_path, ".jpg") || strstr(m.map_bump_path, ".jpeg") || strstr(m.map_bump_path, ".JPG") || strstr(m.map_bump_path, ".JPEG"))
				m.map_bump = load_jpg_map(rel_path, m.map_bump_path);
		}
		else if (strncmp(str, "map_d", 5) == 0)
		{
			//printf("loading a map_d\n");
			m.map_d_path = calloc(512, 1);
			sscanf(str, "map_d %s\n", m.map_d_path);
			if (strstr(m.map_d_path, ".bmp") || strstr(m.map_d_path, ".BMP"))
				m.map_d = load_bmp_map(rel_path, m.map_d_path);
			else if (strstr(m.map_d_path, ".tga") || strstr(m.map_d_path, ".TGA"))
				m.map_d = load_tga_map(rel_path, m.map_d_path);
			else if (strstr(m.map_d_path, ".jpg") || strstr(m.map_d_path, ".jpeg") || strstr(m.map_d_path, ".JPG") || strstr(m.map_d_path, ".JPEG"))
				m.map_d = load_jpg_map(rel_path, m.map_d_path);
		}
		else if (strncmp(str, "map_Ks", 6) == 0)
		{
			//printf("loading a Ks\n");
			m.map_Ks_path = calloc(512, 1);
			sscanf(str, "map_Ks %s\n", m.map_Ks_path);
			if (strstr(m.map_Ks_path, ".bmp") || strstr(m.map_Ks_path, ".BMP"))
				m.map_Ks = load_bmp_map(rel_path, m.map_Ks_path);
			else if (strstr(m.map_Ks_path, ".tga") || strstr(m.map_Ks_path, ".TGA"))
				m.map_Ks = load_tga_map(rel_path, m.map_Ks_path);
			else if (strstr(m.map_Ks_path, ".jpg") || strstr(m.map_Ks_path, ".jpeg") || strstr(m.map_Ks_path, ".JPG") || strstr(m.map_Ks_path, ".JPEG"))
				m.map_Ks = load_jpg_map(rel_path, m.map_Ks_path);
		}
		free(str);
	}
	S->materials[mat_ind] = m;

	fclose(fp);
	free(line);
}

Scene *scene_from_obj(char *rel_path, char *filename, File_edits edit_info)
{
	//meta function to load whole scene from file (ie sponza.obj + sponza.mtl)

	char *obj_file = malloc(strlen(rel_path) + strlen(filename) + 1);
	strcpy(obj_file, rel_path);
	strcat(obj_file, filename);
	FILE *fp = fopen(obj_file, "r");
	if (fp == NULL)
	{
		printf("tried and failed to open file %s\n", obj_file);
		exit(1);
	}
	free(obj_file);

	char *line = calloc(3072, 1);
	char *matpath = calloc(512, 1);

	Scene *S = calloc(1, sizeof(Scene));

	//First loop: counting v, vn, vt, grabbing materials file path, count objects
	int v_count = 0;
	int vn_count = 0;
	int vt_count = 0;
	int face_count = 0;
	int obj_count = 0;
	cl_float3 v;
	cl_float3 min = (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX};
	cl_float3 max = (cl_float3){FLT_MIN, FLT_MIN, FLT_MIN};
	cl_float3 center;
	while(fgets(line, 3072, fp))
	{
		if (strncmp(line, "mtllib", 6) == 0)
			sscanf(line, "mtllib %s\n", matpath);
		else if (strncmp(line, "v ", 2) == 0)
		{
			v_count++;
			sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);
			if (v.x < min.x)
				min.x = v.x;
			if (v.y < min.y)
				min.y = v.y;
			if (v.z < min.z)
				min.z = v.z;
			if (v.x > max.x)
				max.x = v.x;
			if (v.y > max.y)
				max.y = v.y;
			if (v.z > max.z)
				max.z = v.z;
		}
		else if (strncmp(line, "vn", 2) == 0)
			vn_count++;
		else if (strncmp(line, "vt", 2) == 0)
			vt_count++;
		else if (strncmp(line, "f ", 2) == 0)
		{
			int count = count_face_elements(line);
			face_count += (count - 2);
		}
		else if (strncmp(line, "g ", 2) == 0)
			obj_count++;
	}
	center = vec_add(vec_scale(vec_sub(max, min), 0.5), min);
	if (VERBOSE)
		printf("%d triangles\n", face_count);

	//load mats
	if (*matpath != '\0')
		load_mats(S, rel_path, matpath);
	free(matpath);

	//back to top of file, alloc objects, count faces, load v, vt, vn
	fseek(fp, 0, SEEK_SET);

	cl_float3 *V = calloc(v_count, sizeof(cl_float3));
	cl_float3 *VN = calloc(vn_count, sizeof(cl_float3));
	cl_float3 *VT = calloc(vt_count, sizeof(cl_float3));
	Face *faces = calloc(face_count, sizeof(Face));

	v_count = 0;
	vn_count = 0;
	vt_count = 0;
	face_count = 0;

	if (VERBOSE)
		printf("%d objects\n", obj_count);
	int *obj_indices = calloc(obj_count, sizeof(int));
	obj_count = 0;

	int mat_ind = -1;
	int smoothing = 0;
	while(fgets(line, 3072, fp))
	{
		v.x = v.y = v.z = 0;
		if (strncmp(line, "v ", 2) == 0)
		{
			sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);
			v = vec_sub(v, center);
			v = vec_scale(v, edit_info.scale);
			vec_rot(edit_info.rotate, &v);
			v = vec_add(v, edit_info.translate);
			V[v_count++] = v;
		}
		else if (strncmp(line, "vn", 2) == 0)
		{
			sscanf(line, "vn %f %f %f", &v.x, &v.y, &v.z);
			VN[vn_count++] = v;
		}
		else if (strncmp(line, "vt", 2) == 0)
		{
			sscanf(line, "vt %f %f %f", &v.x, &v.y, &v.z);
			VT[vt_count++] = v;
		}
		else if (strncmp(line, "usemtl ", 7) == 0)
		{
			char *matstring = malloc(256);
			sscanf(line, "usemtl %s\n", matstring);
			//match to material in array
			for (int i = 0; i < S->mat_count; i++)
			{
				if (strcmp(matstring, S->materials[i].friendly_name) == 0)
				{
					mat_ind = i;
					break;
				}
				if (i == S->mat_count - 1)
					printf("failed to match material\n");
			}
			free(matstring);
		}
		else if (strncmp(line, "g ", 2) == 0)
		{
			obj_indices[obj_count] = face_count;
			obj_count++;
		}
		else if (strncmp(line, "f ", 2) == 0)
		{
			Face f;
			int va, vna, vta, vb, vnb, vtb, vc, vnc, vtc;
			int count = get_face_elements(line, &va, &vta, &vna, &vb, &vtb, &vnb, &vc, &vtc, &vnc);
			f.shape = 3;
			f.center = ORIGIN;
			if (va < 0)
				f.verts[0] = V[v_count + va];
			else
				f.verts[0] = V[va - 1];
			if (vb < 0)
				f.verts[1] = V[v_count + vb];
			else
				f.verts[1] = V[vb - 1]; 
			if (vc < 0)
				f.verts[2] = V[v_count + vc];
			else
				f.verts[2] = V[vc - 1];
		
			f.center = vec_add(f.center, f.verts[0]);
			f.center = vec_add(f.center, f.verts[1]);
			f.center = vec_add(f.center, f.verts[2]);
			f.center = vec_scale(f.center, 1.0f / (float)f.shape);

			if (vna == 0 || vnb == 0 || vnc == 0)
			{
				f.norms[0] = unit_vec(cross(vec_sub(f.verts[1], f.verts[0]), vec_sub(f.verts[2], f.verts[0])));
				f.norms[1] = f.norms[0];
				f.norms[2] = f.norms[0];
			}
			else
			{
				if (vna < 0)
					f.norms[0] = VN[vn_count + vna];
				else
					f.norms[0] = VN[vna - 1];
				if (vnb < 0)
					f.norms[1] = VN[vn_count + vnb];
				else
					f.norms[1] = VN[vnb - 1];
				if (vnc < 0)
					f.norms[2] = VN[vn_count + vnc];
				else
					f.norms[2] = VN[vnc - 1];
			}

			if (vta == 0 || vtb == 0 || vtc == 0)
			{
				f.tex[0] = (cl_float3){0, 0, 0};
				f.tex[1] = (cl_float3){0, 0, 0};
				f.tex[2] = (cl_float3){0, 0, 0};
			}
			else
			{
				if (vta < 0)
					f.tex[0] = VT[vt_count + vta];
				else
					f.tex[0] = VT[vta - 1];
				if (vtb < 0)
					f.tex[1] = VT[vt_count + vtb];
				else
					f.tex[1] = VT[vtb - 1];
				if (vtc < 0)
					f.tex[2] = VT[vt_count + vtc];
				else
					f.tex[2] = VT[vtc - 1];
			}

			f.N = unit_vec(cross(vec_sub(f.verts[1], f.verts[0]), vec_sub(f.verts[2], f.verts[0])));
			if (dot(f.N, f.norms[0]) < 0)
				f.N = vec_rev(f.N);
			f.smoothing = smoothing;
			f.mat_type = GPU_MAT_DIFFUSE;
			f.mat_ind = mat_ind;
			f.next = NULL;
			faces[face_count++] = f;

			if (count > 3)
			{
				for (int triangle = 0; triangle < count - 3; triangle++)
				{
					Face new_f;
					break_down_poly(line, triangle, &vb, &vtb, &vnb, &vc, &vtc, &vnc);
					new_f.shape = 3;
					new_f.center = ORIGIN;
					if (va < 0)
						new_f.verts[0] = V[v_count + va];
					else
						new_f.verts[0] = V[va - 1];
					if (vb < 0)
						new_f.verts[1] = V[v_count + vb];
					else
						new_f.verts[1] = V[vb - 1]; 
					if (vc < 0)
						new_f.verts[2] = V[v_count + vc];
					else
						new_f.verts[2] = V[vc - 1];
					
					new_f.center = vec_add(new_f.center, new_f.verts[0]);
					new_f.center = vec_add(new_f.center, new_f.verts[1]);
					new_f.center = vec_add(new_f.center, new_f.verts[2]);
					new_f.center = vec_scale(new_f.center, 1.0f / (float)new_f.shape);
			
					if (vna == 0 || vnb == 0 || vnc == 0)
					{
						new_f.norms[0] = unit_vec(cross(vec_sub(new_f.verts[1], new_f.verts[0]), vec_sub(new_f.verts[2], new_f.verts[0])));
						new_f.norms[1] = new_f.norms[0];
						new_f.norms[2] = new_f.norms[0];
					}
					else
					{
						if (vna < 0)
							new_f.norms[0] = VN[vn_count + vna];
						else
							new_f.norms[0] = VN[vna - 1];
						if (vnb < 0)
							new_f.norms[1] = VN[vn_count + vnb];
						else
							new_f.norms[1] = VN[vnb - 1];
						if (vnc < 0)
							new_f.norms[2] = VN[vn_count + vnc];
						else
							new_f.norms[2] = VN[vnc - 1];
					}

					if (vta == 0 || vtb == 0 || vtc == 0)
					{
						new_f.tex[0] = (cl_float3){0, 0, 0};
						new_f.tex[1] = (cl_float3){0, 0, 0};
						new_f.tex[2] = (cl_float3){0, 0, 0};
					}
					else
					{
						if (vta < 0)
							new_f.tex[0] = VT[vt_count + vta];
						else
							new_f.tex[0] = VT[vta - 1];
						if (vtb < 0)
							new_f.tex[1] = VT[vt_count + vtb];
						else
							new_f.tex[1] = VT[vtb - 1];
						if (vtc < 0)
							new_f.tex[2] = VT[vt_count + vtc];
						else
							new_f.tex[2] = VT[vtc - 1];
					}

					new_f.N = unit_vec(cross(vec_sub(new_f.verts[1], new_f.verts[0]), vec_sub(new_f.verts[2], new_f.verts[0])));
					if (dot(new_f.N, new_f.norms[0]) < 0)
						new_f.N = vec_rev(new_f.N);
					new_f.smoothing = smoothing;
					new_f.mat_type = GPU_MAT_DIFFUSE;
					new_f.mat_ind = mat_ind;
					new_f.next = NULL;
					faces[face_count++] = new_f;
				}
			}
		}
	}

	S->faces = faces;
	S->face_count = face_count;

	fclose(fp);
	free(obj_indices);
	free(line);
	free(V);
	free(VN);
	free(VT);

	return S;
}
