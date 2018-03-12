#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
#endif

//THIS DOESNT WORK RIGHT, JUST FOR TESTING
#define H_FOV M_PI_2 * 60.0 / 90.0

void		set_camera(t_camera *cam, float win_dim)
{
	cam->focus = vec_add(cam->pos, vec_scale(cam->dir, cam->dist));

	cl_float3 camera_x, camera_y;
	//horizontal reference
	if (fabs(cam->dir.x) < .00001)
		camera_x = (cam->dir.z > 0) ? UNIT_X : vec_scale(UNIT_X, -1);
	else
		camera_x = unit_vec(cross((cl_float3){cam->dir.x, 0, cam->dir.z}, (cl_float3){0, -1, 0}));
	//vertical reference
	if (fabs(cam->dir.y) < .00001)
		camera_y = UNIT_Y;
	else
		camera_y = unit_vec(cross(cam->dir, camera_x));
	// if (camera_y.y < 0) //when vertical plane reference points down
	// 	camera_y.y *= -1;
	
	//pixel deltas
	cam->d_x = vec_scale(camera_x, cam->width / win_dim);
	cam->d_y = vec_scale(camera_y, cam->height / win_dim);

	//start at bottom corner (the plane's "origin")
	cam->origin = vec_sub(cam->pos, vec_scale(camera_x, cam->width / 2.0));
	cam->origin = vec_sub(cam->origin, vec_scale(camera_y, cam->height / 2.0));
}

t_camera	init_camera(void)
{
	t_camera	cam;
	//cam.pos = (cl_float3){-400.0, 50.0, -220.0}; //reference vase view (1,0,0)
	//cam.pos = (cl_float3){-540.0, 150.0, 380.0}; //weird wall-hole (0,0,1)
	cam.pos = (cl_float3){800.0, 450.0, 0.0}; //standard high perspective on curtain
	//cam.pos = (cl_float3){-800.0, 600.0, 350.0}; upstairs left
	//cam.pos = (cl_float3){800.0, 100.0, 350.0}; //down left
	//cam.pos = (cl_float3){900.0, 150.0, -35.0}; //lion
	//cam.pos = (cl_float3){-250.0, 100.0, 0.0};
	cam.dir = unit_vec((cl_float3){-1.0, 0.0, 0.0});
	cam.width = 1.0;
	cam.height = 1.0;
	cam.angle_x = atan2(cam.dir.z, cam.dir.x);
	cam.angle_y = 0;
	//determine a focus point in front of the view-plane
	//such that the edge-focus-edge vertex has angle H_FOV

	cam.dist = (cam.width / 2.0) / tan(H_FOV / 2.0);
	return cam;
}

t_env		*init_env(void)
{
	t_env	*env = malloc(sizeof(t_env));
	env->cam = init_camera();
	//load camera settings from config file and import scene
	load_config(env);
	set_camera(&env->cam, (float)DIM_IA);
	env->ia = malloc(sizeof(t_mlx_data));
	env->ia->bpp = 0;
	env->ia->size_line = 0;
	env->ia->endian = 0;
	env->pt = malloc(sizeof(t_mlx_data));
	env->pt->bpp = 0;
	env->pt->size_line = 0;
	env->pt->endian = 0;
	env->mode = 1;
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
	return env;
}

void		path_tracer(t_env *env)
{
	if (!env->pt->win || !env->pt->img || !env->pt->imgbuff)
	{
		env->pt->win = mlx_new_window(env->mlx, DIM_PT, DIM_PT, "CLIVE - Path Tracer");
		env->pt->img = mlx_new_image(env->mlx, DIM_PT, DIM_PT);
		env->pt->imgbuff = mlx_get_data_addr(env->pt->img, &env->pt->bpp, &env->pt->size_line, &env->pt->endian);
		env->pt->bpp /= 8;
	}
	set_camera(&env->cam, DIM_PT);
	printf("1\n");
	env->pt->pixels = gpu_render(env->scene, env->cam, DIM_PT, DIM_PT, env->spp);
	printf("2\n");
	draw_pixels(env->pt, DIM_PT, DIM_PT);
	printf("3\n");
	mlx_put_image_to_window(env->mlx, env->pt->win, env->pt->img, 0, 0);
	mlx_key_hook(env->pt->win, exit_hook, env);
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

  	//Enter interactive loop
  	env->mlx = mlx_init();
	env->ia->win = mlx_new_window(env->mlx, DIM_IA, DIM_IA, "CLIVE - Interactive Mode");
	env->ia->img = mlx_new_image(env->mlx, DIM_IA, DIM_IA);
	env->ia->imgbuff = mlx_get_data_addr(env->ia->img, &env->ia->bpp, &env->ia->size_line, &env->ia->endian);
	env->ia->bpp /= 8;
	env->ia->pixels = malloc(sizeof(cl_double3) * ((DIM_IA) * (DIM_IA)));

	mlx_hook(env->ia->win, 2, 0, key_press, env);
	mlx_hook(env->ia->win, 3, 0, key_release, env);
	// mlx_hook(env->win, 6, 0, mouse_pos, env);
	mlx_hook(env->ia->win, 17, 0, exit_hook, env);
	mlx_loop_hook(env->mlx, forever_loop, env);
	mlx_loop(env->mlx);
	
	return 0;
}
