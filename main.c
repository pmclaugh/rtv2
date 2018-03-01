#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
#endif

//THIS DOESNT WORK RIGHT, JUST FOR TESTING
#define H_FOV M_PI_2 * 60.0 / 90.0
// t_camera	init_camera(int xres, int yres)
// {
// 	t_camera cam;
// 	//cam.pos = (cl_float3){-400.0, 50.0, -220.0}; //reference vase view (1,0,0)
// 	//cam.pos = (cl_float3){-540.0, 150.0, 380.0}; //weird wall-hole (0,0,1)
// 	cam.pos = (cl_float3){-800.0, 450.0, 0.0}; //standard high perspective on curtain
// 	//cam.pos = (cl_float3){-800.0, 600.0, 350.0}; upstairs left
// 	//cam.pos = (cl_float3){800.0, 100.0, 350.0}; //down left
// 	//cam.pos = (cl_float3){900.0, 150.0, -35.0}; //lion
// 	//cam.pos = (cl_float3){-250.0, 100.0, 0.0};
// 	cam.dir = (cl_float3){1.0, 0.0, 0.0};
// 	cam.width = 1.0;
// 	cam.height = 1.0;
// 	//determine a focus point in front of the view-plane
// 	//such that the edge-focus-edge vertex has angle H_FOV

// 	cam.dir = unit_vec(cam.dir);

// 	float d = (cam.width / 2.0) / tan(H_FOV / 2.0);
// 	cam.focus = vec_add(cam.pos, vec_scale(cam.dir, d));

// 	//now i need unit vectors on the plane
// 	t_3x3 camera_matrix = rotation_matrix(UNIT_Z, cam.dir);
// 	cl_float3 camera_x, camera_y;
// 	camera_x = mat_vec_mult(camera_matrix, UNIT_X);
// 	camera_y = mat_vec_mult(camera_matrix, UNIT_Y);

// 	//pixel deltas
// 	cam.d_x = vec_scale(camera_x, cam.width / (float)xres);
// 	cam.d_y = vec_scale(camera_y, cam.height / (float)yres);

// 	//start at bottom corner (the plane's "origin")
// 	cam.origin = vec_sub(cam.pos, vec_scale(camera_x, cam.width / 2.0));
// 	cam.origin = vec_sub(cam.origin, vec_scale(camera_y, cam.height / 2.0));
// 	return cam;
// }

void		set_camera(t_camera *cam)
{
	// printf("angle_x = %f\tangle_y = %f\n", cam->angle_x, cam->angle_y);
	// print_vec(cam->dir);

	cam->focus = vec_add(cam->pos, vec_scale(cam->dir, cam->dist));

	//now i need unit vectors on the plane
	t_3x3 camera_matrix = rotation_matrix(UNIT_Z, cam->dir);
	cl_float3 camera_x, camera_y;
	camera_x = mat_vec_mult(camera_matrix, UNIT_X);
	camera_y = mat_vec_mult(camera_matrix, UNIT_Y);

	//pixel deltas
	cam->d_x = vec_scale(camera_x, cam->width / (float)XDIM);
	cam->d_y = vec_scale(camera_y, cam->height / (float)YDIM);

	//start at bottom corner (the plane's "origin")
	cam->origin = vec_sub(cam->pos, vec_scale(camera_x, cam->width / 2.0));
	cam->origin = vec_sub(cam->origin, vec_scale(camera_y, cam->height / 2.0));
}

t_camera	init_camera(void)
{
	t_camera	cam;
	//cam.pos = (cl_float3){-400.0, 50.0, -220.0}; //reference vase view (1,0,0)
	//cam.pos = (cl_float3){-540.0, 150.0, 380.0}; //weird wall-hole (0,0,1)
	cam.pos = (cl_float3){-800.0, 450.0, 0.0}; //standard high perspective on curtain
	//cam.pos = (cl_float3){-800.0, 600.0, 350.0}; upstairs left
	//cam.pos = (cl_float3){800.0, 100.0, 350.0}; //down left
	//cam.pos = (cl_float3){900.0, 150.0, -35.0}; //lion
	//cam.pos = (cl_float3){-250.0, 100.0, 0.0};
	cam.dir = unit_vec((cl_float3){1.0, 0.0, 0.0});
	cam.width = 1.0;
	cam.height = 1.0;
	cam.angle_x = 0;
	cam.angle_y = 0;
	//determine a focus point in front of the view-plane
	//such that the edge-focus-edge vertex has angle H_FOV

	cam.dist = (cam.width / 2.0) / tan(H_FOV / 2.0);
	set_camera(&cam);
	return cam;
}

t_env		*init_env(Scene *S)
{
	t_env	*env = malloc(sizeof(t_env));
	env->cam = init_camera();
	env->pixels = malloc(sizeof(cl_double3) * (XDIM * YDIM));
	env->scene = S;
	env->mlx = mlx_init();
	env->win = mlx_new_window(env->mlx, XDIM, YDIM, "PATH_TRACER");
	env->img = mlx_new_image(env->mlx, XDIM, YDIM);
	env->key.w = 0;
	env->key.a = 0;
	env->key.s = 0;
	env->key.d = 0;
	env->key.uarr = 0;
	env->key.darr = 0;
	env->key.larr = 0;
	env->key.rarr = 0;
	env->key.space = 0;
	env->key.ctrl = 0;
	mlx_hook(env->win, 2, 0, key_press, env);
	mlx_hook(env->win, 3, 0, key_release, env);
	// mlx_hook(env->win, 6, 0, mouse_pos, env);
	mlx_hook(env->win, 17, 0, exit_hook, env);
	mlx_loop_hook(env->mlx, forever_loop, env);
	mlx_loop(env->mlx);
	return env;
}

int 		main(int ac, char **av)
{
	srand(time(NULL));

	//Load scene
	Scene *sponza = scene_from_obj("objects/sponza/", "sponza.obj");

	//LL is best for this bvh. don't want to rearrange import for now, will do later
	Face *face_list = NULL;
	for (int i = 0; i < sponza->face_count; i++)
	{
		Face *f = calloc(1, sizeof(Face));
		memcpy(f, &sponza->faces[i], sizeof(Face));
		f->next = face_list;
		face_list = f;
	}
	free(sponza->faces);

	//Build BVH
	int box_count, ref_count;
	AABB *tree = sbvh(face_list, &box_count, &ref_count);
	printf("finished with %d boxes\n", box_count);
	study_tree(tree, 100000);

	//Flatten BVH
	sponza->bins = tree;
	sponza->bin_count = box_count;
	sponza->face_count = ref_count;
	printf("about to flatten\n");
	flatten_faces(sponza);
	printf("starting render\n");
	
	//Render
	t_env		*env = init_env(sponza);
	return 0;
}
