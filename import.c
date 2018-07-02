#include "rt.h"

static float scalar_project(const cl_float3 a, const cl_float3 b)
{
	//scalar projection of a onto b aka mag(vec_project(a,b))
	return dot(a, unit_vec(b));
}

static void		init_key_frames(t_env *env)
{
	t_camera	cam = env->cam;
	for (int i = 0; i < env->num_key_frames; i++)
	{
		cl_float3	next_pos = env->key_frames[i].position;
		cl_float3	next_dir = env->key_frames[i].direction;
		float		inv_frame_count = 1.0f / env->key_frames[i].frame_count;

		set_camera(&cam, DIM_PT);
		env->key_frames[i].translate = vec_scale(vec_sub(next_pos, cam.pos), inv_frame_count);
		cl_float3	next_dir_hor = unit_vec((cl_float3){next_dir.x, 0.0f, next_dir.z});
		cl_float3	camera_dir_hor = unit_vec((cl_float3){cam.dir.x, 0.0f, cam.dir.z});
		env->key_frames[i].rotate_x = acos(dot(next_dir_hor, camera_dir_hor)) * inv_frame_count * ((scalar_project(next_dir, cam.d_x) >= 0) ? 1 : -1);
		camera_dir_hor = vec_rotate_xz(cam.dir, env->key_frames[i].rotate_x * env->key_frames[i].frame_count);
		env->key_frames[i].rotate_y = acos(dot(next_dir, camera_dir_hor)) * inv_frame_count * ((next_dir.y >= cam.dir.y) ? -1 : 1);
		cam.pos = next_pos;
		cam.dir = next_dir;
	}
}

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
			memcpy(&all->materials[mat_i++], &S[j]->materials[k], sizeof(Material));
		}
		for (int k = 0; k < S[j]->face_count; k++)
		{
			memcpy(&all->faces[face_i], &S[j]->faces[k], sizeof(Face));
			all->faces[face_i++].mat_ind += new_mat_ind;
		}
		new_mat_ind += S[j]->mat_count;
		free(S[j]->faces);
		free(S[j]->materials);
	}
	return all;
}

static void	key_frame_data(char **line, FILE *fp, Key_frame *key_frame)
{
	while (fgets(*line, 512, fp))
	{
		if (*line[0] == '#')
			continue ;
		else if (strncmp(*line, "camera.position=", 16) == 0)
			key_frame->position = get_vec(*line);
		else if (strncmp(*line, "camera.normal=", 14) == 0)
			key_frame->direction = unit_vec(get_vec(*line));
		else if (strncmp(*line, "frames=", 7) == 0)
			key_frame->frame_count = (int)strtoul(strchr(*line, '=') + 1, NULL, 10);
		else
			break ;
	}
}

static void file_edits(char **line, FILE *fp, File_edits *edit_info)
{
	edit_info->scale = 1.0f;
	edit_info->ior = 1.0f;
	edit_info->Kd = (cl_float3){0.5, 0.5, 0.5};
	edit_info->Ks = (cl_float3){0.0, 0.0, 0.0};
	edit_info->Ke = (cl_float3){0.0, 0.0, 0.0};
	edit_info->roughness = 0.5;
	edit_info->transparency = 0.0;
	while (fgets(*line, 512, fp))
	{
		if (*line[0] == '#')
			continue ;
		if (strstr(*line, "scale"))
			edit_info->scale = strtof(strchr(*line, '=') + 1, NULL);
		else if (strstr(*line, "translate"))
			edit_info->translate = get_vec(*line);
		else if (strstr(*line, "rotate"))
			edit_info->rotate = get_vec(*line);
		else if (strstr(*line, "map_Kd"))
		{
			edit_info->map_Kd_path = calloc(512, sizeof(char));
			sscanf(strstr(*line, "map_Kd"), "map_Kd= %s\n", edit_info->map_Kd_path);
		}
		else if (strstr(*line, "map_Ks"))
		{
			edit_info->map_Ks_path = calloc(512, sizeof(char));
			sscanf(strstr(*line, "map_Ks"), "map_Ks= %s\n", edit_info->map_Ks_path);
		}
		else if (strstr(*line, "map_Ke"))
		{
			edit_info->map_Ke_path = calloc(512, sizeof(char));
			sscanf(strstr(*line, "map_Ke"), "map_Ke= %s\n", edit_info->map_Ke_path);
		}
		else if (strstr(*line, "Kd"))
			edit_info->Kd = get_vec(*line);
		else if (strstr(*line, "Ks"))
			edit_info->Ks = get_vec(*line);
		else if (strstr(*line, "Ke"))
			edit_info->Ke = get_vec(*line);
		else if (strstr(*line, "refraction"))
			edit_info->ior = strtof(strchr(*line, '=') + 1, NULL);
		else if (strstr(*line, "roughness"))
			edit_info->roughness = strtof(strchr(*line, '=') + 1, NULL);
		else if (strstr(*line, "transparency"))
			edit_info->transparency = strtof(strchr(*line, '=') + 1, NULL);
		else
			break ;
	}
	if (edit_info->scale <= 0.0f)
		edit_info->scale = 1.0f;
	if (edit_info->ior <= 0.0f)
		edit_info->ior = 1.0f;
	if (edit_info->roughness < 0.0f || edit_info->roughness > 1.0f)
		edit_info->roughness = 0.5;
	if (edit_info->transparency < 0.0f || edit_info->transparency > 1.0f)
		edit_info->transparency = 0.1f;
}

static void		count_files(FILE *fp, int *num_files, int *num_key_frames)
{
	*num_files = 0;
	*num_key_frames = 0;
	char *line = calloc(512, 1);
	while (fgets(line, 512, fp))
	{
		if (line[0] == '#')
			continue ;
		else if (strncmp(line, "import=", 7) == 0)
			*num_files += 1;
		else if (strncmp(line, "key_frame", 9) == 0)
			*num_key_frames += 1;
	}
	free(line);
	fseek(fp, 0L, SEEK_SET);
}

void	load_config(t_env *env)
{
	FILE *fp = fopen("./config.ini", "r");

	if (fp == NULL)
	{
		printf("failed to load config.ini file\n");
		exit(0);
	}

	int	num_files = 0;
	count_files(fp, &num_files, &env->num_key_frames);

	char *line = calloc(512, 1);
	char **file_path = calloc(num_files, sizeof(char*));
	char **dir_path = calloc(num_files, sizeof(char*));
	char **file = calloc(num_files, sizeof(char*));
	File_edits *edit_info = calloc(num_files, sizeof(File_edits));
	env->key_frames = malloc(sizeof(Key_frame) * env->num_key_frames);

	
	int i = 0;
	for (; i < num_files; i++)
	{
		file_path[i] = calloc(512, sizeof(char));
		dir_path[i] = calloc(512, sizeof(char));
	}
	i = 0;
	int j = 0;
	while (fgets(line, 512, fp))
	{
		if (line[0] == '#')
			continue ;
		
		while (strncmp(line, "import=", 7) == 0 && i < num_files)
		{
			sscanf(line, "import= %s\n", file_path[i]);
			file_edits(&line, fp, &edit_info[i++]);
		}
		while (strncmp(line, "key_frame", 9) == 0 && j < env->num_key_frames)
		{
			key_frame_data(&line, fp, &env->key_frames[j++]);
		}
		if (strncmp(line, "mode=", 5) == 0)
		{
			if (strstr(line, "ia"))
			{
				#ifdef __APPLE__
					env->mode = IA;
					env->render = 0;
				#endif
			}
			else if (strstr(line, "mv"))
			{
				env->mode = MV;
				env->render = 0;
			}
			else if (strstr(line, "pv"))
			{
				env->mode = PV;
				env->render = 0;
			}
		}
		else if (strncmp(line, "camera.position=", 16) == 0)
			env->cam.pos = get_vec(line);
		else if (strncmp(line, "camera.normal=", 14) == 0)
			env->cam.dir = unit_vec(get_vec(line));
		else if (strncmp(line, "samples=", 8) == 0)
			env->spp = (int)strtoul(strchr(line, '=') + 1, NULL, 10);
		else if (strncmp(line, "minimum.depth=", 14) == 0)
			env->min_bounces = (int)strtoul(strchr(line, '=') + 1, NULL, 10);
		else if (strncmp(line, "focal.length=", 13) == 0)
			env->cam.focal_length = strtof(strchr(line, '=') + 1, NULL);
		else if (strncmp(line, "focal.aperture=", 15) == 0)
			env->cam.aperture = strtof(strchr(line, '=') + 1, NULL);
	}
	if (env->cam.focal_length < 1.0f)
		env->cam.focal_length = 1.0f;
	if (env->min_bounces <= 0)
		env->min_bounces = 1;
	if (env->spp <= 0)
		env->spp = 1;
	env->cam.angle_x = atan2(env->cam.dir.z, env->cam.dir.x);
	printf("cam.pos= %.0f %.0f %.0f\n", env->cam.pos.x, env->cam.pos.y, env->cam.pos.z);
	printf("cam.dir= %.3f %.3f %.3f\n", env->cam.dir.x, env->cam.dir.y, env->cam.dir.z);
	printf("spp= %u\n", env->spp);
	free(line);
	fclose(fp);

	Scene **S = calloc(num_files, sizeof(Scene*));
	for (int j = 0; j < num_files; j++)
	{
		if ((file[j] = strrchr(file_path[j], '/') + 1) == NULL)
		{
			printf("path to file in config.ini must begin with \"./\"\n");
			exit(0);
		}
		strncpy(dir_path[j], file_path[j], file[j] - file_path[j]);

		if (strstr(file[j], ".ply"))
			S[j] = scene_from_ply(dir_path[j], file[j], edit_info[j]);
		else if (strstr(file[j], ".obj"))
			S[j] =  scene_from_obj(dir_path[j], file[j], edit_info[j]);
		else
		{
			printf("not a valid file, must be file.ply or file.obj\n");
			exit(0);
		}
	}
	Scene *all = combine_scenes(S, num_files);
	env->scene = all;
	
	if (env->mode == MV || env->mode == PV)
		init_key_frames(env);
	
	for (i = 0; i < num_files; i++)
	{
		free(file_path[i]);
		free(dir_path[i]);
		free(S[i]);
	}
	free(file);
	free(file_path);
	free(dir_path);
	free(S);
	free(edit_info);
}
