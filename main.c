#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
#endif

void	sum_color(cl_double3 *a, cl_float3 *b, int size)
{
	for (int i = 0; i < size; i++)
	{
		a[i].x += b[i].x;
		a[i].y += b[i].y;
		a[i].z += b[i].z;
	}
}

void add_counts(int *total, int *new, int size)
{
	for (int i = 0; i < size; i++)
		total[i] += new[i];
}

void save_file(t_env *env, int frame_no)
{
	char *filename;
	asprintf(&filename, "frame-%d.ppm", frame_no);
	FILE *f = fopen(filename, "w");
	fprintf(f, "P3\n%d %d\n%d\n ",DIM_PT,DIM_PT,255);
	for (int row=0;row<DIM_PT;row++) {
		for (int col=DIM_PT - 1;col>=0;col--) {
			fprintf(f,"%d %d %d ", (int)(env->pt->pixels[col + DIM_PT * row].x * 255.0f), (int)(env->pt->pixels[col + DIM_PT * row].y * 255.0f), (int)(env->pt->pixels[col + DIM_PT * row].z * 255.0f));
		}
		fprintf(f, "\n");
	}
	fclose(f);
}

void	alt_composite(t_mlx_data *data, int resolution, cl_int *count)
{
	double Lw = 0.0;
	int *nanflags = calloc(resolution, sizeof(int));
	for (int i = 0; i < resolution; i++)
	{
		double scale = count[i] > 0 ? 1.0 / (double)(count[i]) : 1;
		data->pixels[i].x = data->total_clr[i].x * scale;
		data->pixels[i].y = data->total_clr[i].y * scale;
		data->pixels[i].z = data->total_clr[i].z * scale;

		double this_lw = log(0.1 + 0.2126 * data->pixels[i].x + 0.7152 * data->pixels[i].y + 0.0722 * data->pixels[i].z);
		// double this_lw = log(0.1 + 0.3333 * data->pixels[i].x + 0.3333 * data->pixels[i].y + 0.3333 * data->pixels[i].z);
		if (this_lw == this_lw)
			Lw += this_lw;
		else
		{
			printf("NaN alert\n");
			nanflags[i] = 1;
		}
	}

	Lw /= (double)resolution;
	Lw = exp(Lw);

	for (int i = 0; i < resolution; i++)
	{
		data->pixels[i].x = data->pixels[i].x * 0.64 / Lw;
		data->pixels[i].y = data->pixels[i].y * 0.64 / Lw;
		data->pixels[i].z = data->pixels[i].z * 0.64 / Lw;

		data->pixels[i].x = data->pixels[i].x / (data->pixels[i].x + 1.0);
		data->pixels[i].y = data->pixels[i].y / (data->pixels[i].y + 1.0);
		data->pixels[i].z = data->pixels[i].z / (data->pixels[i].z + 1.0);

		if (nanflags[i])
		{
			data->pixels[i].x = 0.0;
			data->pixels[i].y = 1.0;
			data->pixels[i].z = 0.0;
		}
	}
	free(nanflags);
}

void		path_tracer(t_env *env)
{
	static int frame_count;
	int first = (env->samples == 0) ? 1 : 0;
	env->samples += 1;
	set_camera(&env->cam, DIM_PT);
	cl_int *count = NULL;
	cl_float3 *pix = gpu_render(env->scene, env->cam, DIM_PT, DIM_PT, 1, env->min_bounces, first, &count);
	sum_color(env->pt->total_clr, pix, DIM_PT * DIM_PT);
	free(pix);
	add_counts(env->pt->count, count, DIM_PT * DIM_PT);
	free(count);
	alt_composite(env->pt, DIM_PT * DIM_PT, env->pt->count);
	draw_pixels(env->pt, DIM_PT, DIM_PT);
	mlx_put_image_to_window(env->mlx, env->pt->win, env->pt->img, 0, 0);
	mlx_key_hook(env->pt->win, exit_hook, env);
	if (env->samples >= env->spp)
	{
		save_file(env, frame_count);
		frame_count++;
		env->samples = 0;
		env->render = 1;
		for (int i = 0; i < DIM_PT * DIM_PT; i++)
		{
			env->pt->total_clr[i].x = 0;
			env->pt->total_clr[i].y = 0;
			env->pt->total_clr[i].z = 0;

			env->pt->pixels[i].x = 0.0f;
			env->pt->pixels[i].y = 0.0f;
			env->pt->pixels[i].z = 0.0f;

			env->pt->count[i] = 0;
		}
		env->cam.pos = vec_add(env->cam.pos, (cl_float3){1.0f, 0.0f, 0.0f});
	}
	printf("sample %d done\n", env->samples);
}

void		init_mlx_data(t_env *env)
{
	env->mlx = mlx_init();

	env->pt = malloc(sizeof(t_mlx_data));
	env->pt->bpp = 0;
	env->pt->size_line = 0;
	env->pt->endian = 0;
	env->pt->win = mlx_new_window(env->mlx, DIM_PT, DIM_PT, "CLIVE - Path Tracer");
	env->pt->img = mlx_new_image(env->mlx, DIM_PT, DIM_PT);
	env->pt->imgbuff = mlx_get_data_addr(env->pt->img, &env->pt->bpp, &env->pt->size_line, &env->pt->endian);
	env->pt->bpp /= 8;
	env->pt->pixels = calloc(DIM_PT * DIM_PT, sizeof(cl_double3));
	env->pt->total_clr = calloc(DIM_PT * DIM_PT, sizeof(cl_double3));
	env->pt->count = calloc(DIM_PT * DIM_PT, sizeof(cl_int));
	
	if (env->mode == IA)
	{
		env->ia = malloc(sizeof(t_mlx_data));
		env->ia->bpp = 0;
		env->ia->size_line = 0;
		env->ia->endian = 0;
		env->ia->win = mlx_new_window(env->mlx, DIM_IA, DIM_IA, "CLIVE - Interactive Mode");
		env->ia->img = mlx_new_image(env->mlx, DIM_IA, DIM_IA);
		env->ia->imgbuff = mlx_get_data_addr(env->ia->img, &env->ia->bpp, &env->ia->size_line, &env->ia->endian);
		env->ia->bpp /= 8;
		env->ia->pixels = calloc(DIM_IA * DIM_IA, sizeof(cl_double3));
		env->ia->total_clr = NULL;
		mlx_hook(env->ia->win, 2, 0, key_press, env);
		mlx_hook(env->ia->win, 3, 0, key_release, env);
		mlx_hook(env->ia->win, 17, 0, exit_hook, env);
	}

	mlx_loop_hook(env->mlx, forever_loop, env);
	mlx_loop(env->mlx);
}

t_env		*init_env(void)
{
	t_env	*env = malloc(sizeof(t_env));
	env->cam = init_camera();
	//load camera settings from config file and import scene
	env->mode = PT;
	env->view = 1;
	env->show_fps = 0;
	env->key.w = 0;
	env->key.a = 0;
	env->key.s = 0;
	env->key.d = 0;
	env->key.uarr = 0;
	env->key.darr = 0;
	env->key.larr = 0;
	env->key.rarr = 0;
	env->key.space = 0;
	env->key.shift = 0;
	env->samples = 0;
	env->render = 1;
	env->eps = 0.00005;
	load_config(env);
	return env;
}

int 		main(int ac, char **av)
{
	srand(time(NULL));

	//Initialize environment with scene and intial configurations, load files
	t_env	*env = init_env();

	//Build BVH
	sbvh(env);

	//Performance metrics on BVH
	study_tree(env->scene->bins, 100000);

	//Flatten BVH
	flatten_faces(env->scene);

	//launch core loop
	init_mlx_data(env);
	
	return 0;
}
