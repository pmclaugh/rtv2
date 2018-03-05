#include "rt.h"

static Scene	*combine_scenes(Scene **S, int num_files)
{
	Scene *all = calloc(1, sizeof(Scene));
	int		mat_count[num_files];
	int		face_count[num_files];
	int		m_total = 0;
	int		f_total = 0;

	if (num_files == 0)
		exit(0);
	for (int j = 0; j < num_files; j++)
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
	int new_mat_ind = 0;
	int face_i = 0;
	for (int j = 0; j < num_files; j++)
	{
		for (int k = 0; k < S[j]->mat_count; k++)
		{
			all->materials[mat_i].friendly_name = calloc(256, sizeof(char));
			all->materials[mat_i].map_Ka_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_Kd_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_bump_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_d_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_Ks_path = calloc(512, sizeof(char));
			all->materials[mat_i].map_Ka = calloc(1, sizeof(Map));
			all->materials[mat_i].map_Kd = calloc(1, sizeof(Map));
			all->materials[mat_i].map_bump = calloc(1, sizeof(Map));
			all->materials[mat_i].map_d = calloc(1, sizeof(Map));
			all->materials[mat_i].map_Ks = calloc(1, sizeof(Map));
			memcpy((void*)&all->materials[mat_i++], (void*)&S[j]->materials[k], sizeof(Material));
		}
		for (int k = 0; k < S[j]->face_count; k++)
		{
			memcpy((void*)&all->faces[face_i], (void*)&S[j]->faces[k], sizeof(Face));
			all->faces[face_i++].mat_ind += new_mat_ind;
		}
		new_mat_ind += S[j]->mat_count;
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
		if (strncmp(line, "import=", 7) == 0 && i < 4)
			sscanf(line, "import=%s", file_path[i++]);
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
		strncpy(dir_path[j], file_path[j], file[j] - file_path[j]);

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
	Scene *all = combine_scenes(S, i);
	for (i = 0; i < 4; i++)
		free(file_path[i]);
	free(file);
	free(file_path);
	free(dir_path);
	free(S);
	return all;
}
