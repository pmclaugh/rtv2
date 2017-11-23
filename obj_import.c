#include "rt.h"
#include <string.h>

#define VERBOSE 0

Map *load_map(char *rel_path, char *filename)
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

	map->pixels = calloc(map->height * map->width, sizeof(int) * 3);
	unsigned char *raw_pixels = calloc(map->height * map->width, sizeof(unsigned char) * 3);
	fread(raw_pixels, map->height * map->width, sizeof(char) * 3, fp);

	for (int i = 0; i < map->height * map->width; i++)
	{
		//convert to ints and remap from BGR to RGB
		map->pixels[i * 3] = (int)raw_pixels[i * 3 + 2];
		map->pixels[i * 3 + 1] = (int)raw_pixels[i * 3 + 1];
		map->pixels[i * 3 + 2] = (int)raw_pixels[i * 3];
	}

	fclose(fp);
	if (VERBOSE)
		printf("loaded texture %s\n", tga_file);
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
	while (fgets(line, 512, fp))
	{
		if (strncmp(line, "newmtl ", 7) == 0)
		{
			if (mat_ind >= 0)
			{
				S->materials[mat_ind] = m;
				if (VERBOSE)
					printf("loaded material %s\n", m.friendly_name);
			}
			bzero(&m, sizeof(Material));
			mat_ind++;
			m.friendly_name = malloc(256);
			sscanf(line, "newmtl %s\n", m.friendly_name);
			printf("new mtl friendly name %s\n", m.friendly_name);
		}
		else if (strncmp(line, "\tNs", 3) == 0)
			sscanf(line, "\tNs %f", &m.Ns);
		else if (strncmp(line, "\tNi", 3) == 0)
			sscanf(line, "\tNi %f", &m.Ni);
		else if (strncmp(line, "\td", 2) == 0)
			sscanf(line, "\td %f", &m.d);
		else if (strncmp(line, "\tTr", 3) == 0)
			sscanf(line, "\tTr %f", &m.Tr);
		else if (strncmp(line, "\tTf", 3) == 0)
			sscanf(line, "\tTf %f %f %f", &m.Tf.x, &m.Tf.y, &m.Tf.z);
		else if (strncmp(line, "\tillum", 6) == 0)
			sscanf(line, "\tillum %d", &m.illum);

		else if (strncmp(line, "\tKa", 3) == 0)
			sscanf(line, "\tKa %f %f %f", &m.Ka.x, &m.Ka.y, &m.Ka.z);
		else if (strncmp(line, "\tKd", 3) == 0)
			sscanf(line, "\tKd %f %f %f", &m.Kd.x, &m.Kd.y, &m.Kd.z);
		else if (strncmp(line, "\tKs", 3) == 0)
			sscanf(line, "\tKs %f %f %f", &m.Ks.x, &m.Ks.y, &m.Ks.z);
		else if (strncmp(line, "\tKe", 3) == 0)
			sscanf(line, "\tKe %f %f %f", &m.Ke.x, &m.Ke.y, &m.Ke.z);

		else if (strncmp(line, "\tmap_Ka", 7) == 0)
		{
			m.map_Ka_path = calloc(512, 1);
			sscanf(line, "\tmap_Ka %s\n", m.map_Ka_path);
			m.map_Ka = load_map(rel_path, m.map_Ka_path);
		}
		else if (strncmp(line, "\tmap_Kd", 7) == 0)
		{
			m.map_Kd_path = calloc(512, 1);
			sscanf(line, "\tmap_Kd %s\n", m.map_Kd_path);
			m.map_Kd = load_map(rel_path, m.map_Kd_path);
		}
		else if (strncmp(line, "\tmap_bump", 10) == 0)
		{
			m.map_bump_path = calloc(512, 1);
			sscanf(line, "\tmap_bump %s\n", m.map_bump_path);
			m.map_bump = load_map(rel_path, m.map_bump_path);
		}
		else if (strncmp(line, "\tmap_d", 6) == 0)
		{
			m.map_d_path = calloc(512, 1);
			sscanf(line, "\tmap_d %s\n", m.map_d_path);
			m.map_d = load_map(rel_path, m.map_d_path);
		}
	}
	S->materials[mat_ind] = m;

	fclose(fp);

}

Scene *scene_from_obj(char *rel_path, char *filename)
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

	char *line = calloc(512, 1);

	char *matpath = calloc(512, 1);

	Scene *S = calloc(1, sizeof(Scene));

	//First loop: counting v, vn, vt, grabbing materials file path
	int v_count = 0;
	int vn_count = 0;
	int vt_count = 0;
	int face_count = 0;
	while(fgets(line, 512, fp))
	{
		if (strncmp(line, "mtllib ", 7) == 0)
			sscanf(line, "mtllib %s\n", matpath);
		else if (strncmp(line, "v ", 2) == 0)
			v_count++;
		else if (strncmp(line, "vn", 2) == 0)
			vn_count++;
		else if (strncmp(line, "vt", 2) == 0)
			vt_count++;
		else if (strncmp(line, "f ", 2) == 0)
			face_count++;
	}

	//load mats
	load_mats(S, rel_path, matpath);
	printf("basics counted\n");

	//back to top of file, alloc objects, count faces, load v, vt, vn
	fseek(fp, 0, SEEK_SET);

	cl_float3 *V = calloc(v_count, sizeof(cl_float3));
	cl_float3 *VN = calloc(vn_count, sizeof(cl_float3));
	cl_float3 *VT = calloc(vt_count, sizeof(cl_float3));
	Face *faces = calloc(face_count + 1, sizeof(Face));

	v_count = 0;
	vn_count = 0;
	vt_count = 0;
	face_count = 0;

	int mat_ind = -1;
	int smoothing = 0;
	while(fgets(line, 512, fp))
	{
		cl_float3 v;
		if (strncmp(line, "v ", 2) == 0)
		{
			sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);
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
			}
		}
		else if (strncmp(line, "s ", 2) == 0)
			sscanf(line, "s %d\n", &smoothing);
		else if (strncmp(line, "f ", 2) == 0)
		{
			Face f;
			int va, vna, vta, vb, vnb, vtb, vc, vnc, vtc, vd, vnd, vtd;
			int count = sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", &va, &vta, &vna, &vb, &vtb, &vnb, &vc, &vtc, &vnc, &vd, &vtd, &vnd);
			if (count == 12)
				f.shape = 4;
			else if (count == 9)
				f.shape = 3;
			else
				printf("problem\n");
			f.verts[0] = V[va - 1]; 
			f.verts[1] = V[vb - 1]; 
			f.verts[2] = V[vc - 1];
			if (f.shape == 4)
				f.verts[3] = V[vd - 1];

			f.norms[0] = VN[vna - 1]; 
			f.norms[1] = VN[vnb - 1]; 
			f.norms[2] = VN[vnc - 1]; 
			if (f.shape == 4)
				f.norms[3] = VN[vnd - 1];

			f.tex[0] = VN[vta - 1];
			f.tex[1] = VN[vtb - 1];
			f.tex[2] = VN[vtc - 1];
			if (f.shape == 4)
				f.tex[3] = VN[vtd - 1];

			f.N = unit_vec(cross(vec_sub(f.verts[1], f.verts[0]), vec_sub(f.verts[2], f.verts[0])));
			if (dot(f.N, f.norms[0]) < 0)
				f.N = vec_rev(f.N);
			f.smoothing = smoothing;
			f.mat_type = GPU_MAT_DIFFUSE;
			f.mat_ind = mat_ind;
			faces[face_count++] = f;
		}
	}

	S->faces = faces;
	S->face_count = face_count;

	printf("faces loaded\n");

	fclose(fp);

	free(V);
	free(VN);
	free(VT);

	return S;
}



/*
	this has gone faster than expected and is very promising. still a lot of work to do before seeing it though.

	things to think about:
		I should build a very simple test case to try, on both CPU and GPU, that will help me verify basic assumptions about texturing.

		this scene necessitates a BVH. I should redesign it for gpu use. maybe reconsider how i'm storing all this stuff.
			i went to the trouble of making objects collections of faces but I think on the GPU it's going to just be a giant array? not sure.



	UGGGHHH i guess i'm redoing this one more time. Just noticed different sets of faces in the same object can use different materials.
		It's time to just make it one big array fuck distinguishing between objects. each face apparently has its own material value.
*/