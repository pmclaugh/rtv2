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

void		path_tracer(t_env *env)
{
	if (!env->pt)
		init_sdl_pt(env);
	int first = (env->samples == 0) ? 1 : 0;
	env->samples += 1;
	set_camera(&env->cam, DIM_PT);
	// cl_float3 *pix = gpu_render(env->scene, env->cam, DIM_PT, DIM_PT, 1, first, env);
	cl_float3	*pix = bidirectional(env);
	sum_color(env->pt->total_clr, pix, DIM_PT * DIM_PT);
	free(pix);
	alt_composite(env->pt, DIM_PT * DIM_PT, env->samples);
	draw_pixels(env->pt);
	if (env->samples >= env->spp)
	{
		env->samples = 0;
		env->render = 0;
		for (int i = 0; i < DIM_PT * DIM_PT; i++)
		{
			env->pt->total_clr[i].x = 0;
			env->pt->total_clr[i].y = 0;
			env->pt->total_clr[i].z = 0;
		}
	}
}

t_env		*init_env(void)
{
	t_env	*env = malloc(sizeof(t_env));
	env->cam = init_camera();
	//load camera settings from config file and import scene
	env->mode = PT;
	env->view = 1;
	env->show_rays = 0;
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
	env->spp = 0;
	env->depth = 6;
	env->render = 1;
	env->eps = 0.00005;
	env->ray_density = 64;
	env->bounce_vis = 1;
	env->running = 1;
	env->current_tick = 0;
	env->previous_tick = 0;

	env->ray_display = calloc(sizeof(int), (DIM_PT * DIM_PT));
	env->mouse_x = 0;
	env->mouse_y = 0;

	env->ia = NULL;
	env->pt = NULL;
	load_config(env);
	return env;
}

int 		main(int ac, char **av)
{
	srand(time(NULL));

	//Initialize environment with scene and intial configurations
	t_env	*env = init_env();

	//LL is best for this bvh. don't want to rearrange import for now, will do later
	Face *face_list = NULL;
	for (int i = 0; i < env->scene->face_count; i++)
	{
		Face *f = calloc(1, sizeof(Face));
		memcpy(f, &env->scene->faces[i], sizeof(Face));
		f->next = face_list;
		face_list = f;
	}
	free(env->scene->faces);

	//Build BVH
	int box_count, ref_count;
	AABB *tree = sbvh(face_list, &box_count, &ref_count);
	printf("finished with %d boxes\n", box_count);
	study_tree(tree, 100000);

	//Flatten BVH
	env->scene->bins = tree;
	env->scene->bin_count = box_count;
	env->scene->face_count = ref_count;
	flatten_faces(env->scene);

	// for (int i = 0; i < env->scene->face_count; i++)
	// {
	// 	Face *tmp = face_list->next;
	// 	free(face_list);
	// 	face_list = tmp;
	// }

	run_sdl(env);

	return 0;
}
