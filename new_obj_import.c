#include "rt.h"
#include <string.h>


typedef struct s_face
{
	cl_float3 verts[4];
	cl_float3 norms[4];
	cl_float3 tex[4];
	int smoothing;
	int dim;
}				Face;

typedef struct s_map
{
	//not sure what this is going to look like
	//but placeholder for texture/lighting maps
}				Map;

typedef struct s_material
{
	char *friendly_name;
	float Ns; //specular exponent
	float Ni; //index of refraction
	float d; //opacity
	float Tr; //transparency (1 - d)
	cl_float3 Tf; //transmission filter (ie color)
	int illum; //flag for illumination model, raster only
	cl_float3 Ka; //ambient mask
	cl_float3 Kd; //diffuse mask
	cl_float3 Ks; //specular mask
	cl_float3 Ke; //emission mask

	char *map_Ka_path;
	Map *map_Ka;

	char *map_Kd_path;
	Map *map_Kd;

	char *map_bump_path;
	Map *map_bump;

	char *map_d_path;
	Map *map_d;
}				Material;

typedef struct s_new_object
{
	char *friendly_name;
	Face *faces;
	int face_count;
	Material *mat;
	char *matstring;
}				Object;

typedef struct s_new_scene
{
	Material *materials;
	int mat_count;
	Object *objects;
	int obj_count;
}				Scene;

void load_mats(Scene *S, char *filename)
{
	//printf("trying to open %s\n", filename);
	FILE *fp = fopen("objects/sponza/sponza.mtl", "r");

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
				S->materials[mat_ind] = m;
			bzero(&m, sizeof(Material));
			mat_ind++;
			m.friendly_name = strdup(&line[7]);
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
		}
		else if (strncmp(line, "\tmap_Kd", 7) == 0)
		{
			m.map_Kd_path = calloc(512, 1);
			sscanf(line, "\tmap_Kd %s\n", m.map_Kd_path);
		}
		else if (strncmp(line, "\tmap_bump", 10) == 0)
		{
			m.map_bump_path = calloc(512, 1);
			sscanf(line, "\tmap_bump %s\n", m.map_bump_path);
		}
		else if (strncmp(line, "\tmap_d", 6) == 0)
		{
			m.map_d_path = calloc(512, 1);
			sscanf(line, "\tmap_d %s\n", m.map_d_path);
		}
	}

	fclose(fp);

}

Scene *scene_from_obj(char *filename)
{
	//meta function to load whole scene from file (ie sponza.obj + sponza.mtl)

	FILE *fp = fopen(filename, "r");

	char c;
	char *line = calloc(512, 1);

	char *matpath = calloc(512, 1);

	Scene *S = calloc(1, sizeof(Scene));

	//First loop: counting objects, v, vn, vt
	int v_count = 0;
	int vn_count = 0;
	int vt_count = 0;
	while(fgets(line, 512, fp))
	{
		if (strncmp(line, "# object", 8) == 0)
			S->obj_count++;
		else if (strncmp(line, "mtllib ", 7) == 0)
			sscanf(line, "mtllib %s\n", matpath);
		else if (strncmp(line, "v ", 2) == 0)
			v_count++;
		else if (strncmp(line, "vn", 2) == 0)
			vn_count++;
		else if (strncmp(line, "vt", 2) == 0)
			vt_count++;
	}

	//load mats
	load_mats(S, matpath);
	printf("basics counted\n");

	//back to top of file, alloc objects, count faces, load v, vt, vn
	fseek(fp, 0, SEEK_SET);

	cl_float3 *V = calloc(v_count, sizeof(cl_float3));
	cl_float3 *VN = calloc(vn_count, sizeof(cl_float3));
	cl_float3 *VT = calloc(vt_count, sizeof(cl_float3));

	v_count = 0;
	vn_count = 0;
	vt_count = 0;

	S->objects = calloc(S->obj_count, sizeof(Object));
	int obj_ind = -1;
	while(fgets(line, 512, fp))
	{
		cl_float3 v;
		if (strncmp(line, "# object", 8) == 0)
		{
			obj_ind++;
			S->objects[obj_ind].friendly_name = strdup(&line[8]);
		}
		else if (strncmp(line, "v ", 2) == 0)
		{
			sscanf(line, "%f %f %f", &v.x, &v.y, &v.z);
			V[v_count++] = v;
		}
		else if (strncmp(line, "vn", 2) == 0)
		{
			sscanf(line, "%f %f %f", &v.x, &v.y, &v.z);
			VN[vn_count++] = v;
		}
		else if (strncmp(line, "vt", 2) == 0)
		{
			sscanf(line, "%f %f %f", &v.x, &v.y, &v.z);
			VT[vt_count++] = v;
		}
		else if (strncmp(line, "f ", 2) == 0)
			S->objects[obj_ind].face_count++;
		else if (strncmp(line, "usemtl ", 7) == 0)
			S->objects[obj_ind].matstring = strdup(&line[7]);
	}

	printf("objects described\n");

	//last pass; make faces
	fseek(fp, 0, SEEK_SET);
	obj_ind = -1;
	int f_ind = 0;
	while(fgets(line, 512, fp))
	{
		if (strncmp(line, "# object", 8) == 0)
		{
			obj_ind++;
			S->objects[obj_ind].faces = calloc(S->objects[obj_ind].face_count, sizeof(Face));
			f_ind = 0;
		}
		else if (strncmp(line, "f ", 2) == 0)
		{
			Face f;
			int va, vna, vta, vb, vnb, vtb, vc, vnc, vtc, vd, vnd, vtd;
			int count = sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d %d/%d/%d", &va, &vta, &vna, &vb, &vtb, &vnb, &vc, &vtc, &vnc, &vd, &vtd, &vnd);
			if (count == 12)
				f.dim = 4;
			else if (count == 9)
				f.dim = 3;
			else
				printf("problem\n");
			f.verts[0] = V[va - 1]; 
			f.verts[1] = V[vb - 1]; 
			f.verts[2] = V[vc - 1];
			if (f.dim == 4)
				f.verts[3] = V[vd - 1];

			f.norms[0] = VN[vna - 1]; 
			f.norms[1] = VN[vnb - 1]; 
			f.norms[2] = VN[vnc - 1]; 
			if (f.dim == 4)
				f.norms[3] = VN[vnd - 1];

			f.tex[0] = VN[vta - 1];
			f.tex[1] = VN[vtb - 1];
			f.tex[2] = VN[vtc - 1];
			if (f.dim == 4)
				f.tex[3] = VN[vtd - 1];

			S->objects[obj_ind].faces[f_ind] = f;
			f_ind++;
		}
	}
	printf("objects loaded\n");
	fclose(fp);
	return S;
}

int main (void)
{
	scene_from_obj("objects/sponza/sponza.obj");
}