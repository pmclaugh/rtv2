#include "rt.h"

static Scene	*combine_scenes(Scene **S, int i)
{
	Scene *all = calloc(1, sizeof(Scene));
	int		mat_count[i];
	int		face_count[i];
	int		m_total = 0;
	int		f_total = 0;

	if (i == 0)
		exit(0);
	for (int j = 0; j < i; j++)
	{
		mat_count[j] = S[j]->mat_count;
		face_count[j] = S[j]->face_count;
		m_total += mat_count[j];
		f_total += face_count[j];
	}
	all->materials = calloc(m_total, sizeof(Material));
	all->mat_count = m_total;
	all->faces = calloc(f_total, sizeof(Face));
	all->face_count = f_total;
	int mat_i = 0;
	int face_i = 0;
	for (int j = 0; j < i; j++)
	{
		printf("inside big loop\n");
		for (int k = 0; k < S[j]->mat_count; k++)
		{//look out for mat_i increment placement
			printf("begin loop\n");
			all->materials[mat_i].friendly_name = calloc(256, sizeof(char));
			all->materials[mat_i].map_Ka_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_Kd_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_bump_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_d_path = calloc(512, sizeof(char));
		//	printf("check\n");
		//	printf("check len=%lu\n", strlen(S[j]->materials[k].map_Ks_path));
			all->materials[mat_i].map_Ks_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_Ka = calloc(1, sizeof(Map));
			all->materials[mat_i].map_Kd = calloc(1, sizeof(Map));
			all->materials[mat_i].map_bump = calloc(1, sizeof(Map));
			all->materials[mat_i].map_d = calloc(1, sizeof(Map));
			all->materials[mat_i].map_Ks = calloc(1, sizeof(Map));
		/*	S[j]->materials[k].map_Ka->height = ;
			S[j]->materials[k].map_Ka->width = ;
			S[j]->materials[k].map_Kd->height = ;
			S[j]->materials[k].map_Kd->width = ;
			S[j]->materials[k].map_bump->height = ;
			S[j]->materials[k].map_bump->width = ;
			S[j]->materials[k].map_d->height = ;
			S[j]->materials[k].map_d->width = ;
			S[j]->materials[k].map_Ks->height = ;
			S[j]->materials[k].map_Ks->width = ;*/
	/*		all->materials[mat_i].map_Ka->pixels = calloc(S[j]->materials[k].map_Ka->height * S[j]->materials[k].map_Ka->width, sizeof(cl_uchar) * 3);
			all->materials[mat_i].map_Kd->pixels = calloc(S[j]->materials[k].map_Kd->height * S[j]->materials[k].map_Kd->width, sizeof(cl_uchar) * 3);
			all->materials[mat_i].map_bump->pixels = calloc(S[j]->materials[k].map_bump->height * S[j]->materials[k].map_bump->width, sizeof(cl_uchar) * 3);
			printf("check3\n");
			all->materials[mat_i].map_d->pixels = calloc(S[j]->materials[k].map_d->height * S[j]->materials[k].map_d->width, sizeof(cl_uchar) * 3);
			printf("check4\n");
			all->materials[mat_i].map_Ks->pixels = calloc(1, sizeof(cl_uchar) * 3);
			printf("check1\n");*/
			memcpy((void*)&all->materials[mat_i++], (void*)&S[j]->materials[k], sizeof(Material));
			printf("check2\n");
		/*	S[j]->materials[k].Ns = 1;
			S[j]->materials[k].Ni = 1;
			S[j]->materials[k].d = 1;
			S[j]->materials[k].Tr = 0;
			S[j]->materials[k].Tf = (cl_float3){0, 0, 0};
			S[j]->materials[k].illum = 0;
			S[j]->materials[k].Ka = (cl_float3){1, 0.4, 0.4}; //ambient mask
			S[j]->materials[k].Kd = (cl_float3){1, .2, .2}; //diffuse mask
			S[j]->materials[k].Ks = (cl_float3){1, 1, 1};
			S[j]->materials[k].Ke = (cl_float3){1, 1, 1};*/
		}
		printf("mid loop\n");
		for (int k = 0; k < S[j]->face_count; k++)
		{
			memcpy((void*)&all->faces[face_i++], (void*)&S[j]->faces[k], sizeof(Face));
		}
		printf("end loop\n");
	}
	return all;
}

Scene	*import_file(void)
{
	FILE *fp = fopen("./config.ini", "r");

	if (fp == NULL)
	{
		printf("failed to load config.ini file\n");
		exit(0);
	}

	char *line = calloc(512, 1);
	char **file_path = calloc(4, sizeof(char*));
	char **dir_path = calloc(4, sizeof(char*));
	char **file = calloc(4, sizeof(char*));
	
	int i = 0;
	for (; i < 4; i++)
	{
		file_path[i] = calloc(512, sizeof(char));
		dir_path[i] = calloc(512, sizeof(char));
	}
	i = 0;
	while (fgets(line, 512, fp))
	{
		if (strncmp(line, "import=", 7) == 0 && i < 4){
			sscanf(line, "import=%s", file_path[i++]);
			printf("filepath=%s|\n", file_path[i-1]);
		}
	}
	free(line);
	fclose(fp);

	Scene **S = calloc(4, sizeof(Scene*));
	for (int j = 0; j < i; j++)
	{
		if ((file[j] = strrchr(file_path[j], '/') + 1) == NULL)
		{
			printf("path to file in config.ini must begin with \"./\"\n");
			exit(0);
		}
		printf("file=%s|\n", file[j]);
		strncpy(dir_path[j], file_path[j], file[j] - file_path[j]);
		printf("|dir=%s|\n|file=%s|\n", dir_path[j], file[j]);

		if (strstr(file[j], ".ply"))
			S[j] = scene_from_ply(dir_path[j], file[j]);
		else if (strstr(file[j], ".obj"))
			S[j] =  scene_from_obj(dir_path[j], file[j]);
		else
		{
			printf("not a valid file, must be file.ply or file.obj\n");
			exit(0);
		}
	}
	printf("before comb\n");
	Scene *all = combine_scenes(S, i);
	printf("after comb\n");
	for (i = 0; i < 4; i++)
		free(file_path[i]);
	free(file);
	free(file_path);
	free(dir_path);
	free(S);
//	exit(0);
	return all;
}
