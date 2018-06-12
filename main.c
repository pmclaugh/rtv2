#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
#endif

void	sum_color(cl_float3 *a, cl_float3 *b, int size)
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

void	alt_composite(t_sdl *data, int resolution, cl_int *count)
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
	if (!env->pt)
		init_sdl_pt(env);
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
	draw_pixels(env->pt);
	// save_file(env, env->samples);
	if (env->samples >= env->spp)
	{
		env->samples = 0;
		env->render = 0;
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
	}
	printf("sample %d done\n", env->samples);
}

t_env		*init_env(void)
{
	t_env	*env = malloc(sizeof(t_env));
	env->cam = init_camera();

	env->ia = NULL;
	env->pt = NULL;

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
	env->key.plus = 0;
	env->key.minus = 0;

	env->mode = PT;
	env->view = 1;
	env->spp = 0;
	env->samples = 0;
	env->render = 1;
	env->eps = 0.00002;

	env->running = 1;
	env->current_tick = 0;
	env->previous_tick = 0;

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
	//study_tree(env->scene->bins, 100000);

	//Flatten BVH
	flatten_faces(env->scene);

	//launch core loop
	run_sdl(env);
	
	return 0;
}
