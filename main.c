#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
# define quit_key 12
#endif

typedef struct s_param
{
	void *mlx;
	void *win;
	void *img;
	int x;
	int y;

	Scene *scene;
	t_camera cam;
	cl_float3 *pixels;
	int samplecount;
}				t_param;

#define rot_angle M_PI_2 * 4.0 / 90.0

cl_float3 *baby_tone_dupe(cl_float3 *pixels, int count, int samplecount)
{
	cl_float3 *toned = calloc(count, sizeof(cl_float3));
	for (int i = 0; i < count; i++)
	{
		toned[i].x = pixels[i].x / ((float)samplecount);
		toned[i].y = pixels[i].y / ((float)samplecount);
		toned[i].z = pixels[i].z / ((float)samplecount);
		toned[i].x = toned[i].x / (toned[i].x + 1.0);
		toned[i].y = toned[i].y / (toned[i].y + 1.0);
		toned[i].z = toned[i].z / (toned[i].z + 1.0);
	}
	return toned;
}

#define H_FOV M_PI_2 * 60.0 / 90.0 	//60 degrees. eventually this will be dynamic with aspect
									// V_FOV is implicit with aspect of view-plane

void init_camera(t_camera *camera, int xres, int yres)
{
	//printf("init camera %d %d\n", xres, yres);
	//determine a focus point in front of the view-plane
	//such that the edge-focus-edge vertex has angle H_FOV
	float d = (camera->width / 2.0) / tan(H_FOV / 2.0);
	camera->focus = vec_add(camera->center, vec_scale(camera->normal, d));

	//now i need unit vectors on the plane
	t_3x3 camera_matrix = rotation_matrix(UNIT_Z, camera->normal);
	cl_float3 camera_x, camera_y;
	camera_x = mat_vec_mult(camera_matrix, UNIT_X);
	camera_y = mat_vec_mult(camera_matrix, UNIT_Y);

	//pixel deltas
	camera->d_x = vec_scale(camera_x, camera->width / (float)xres);
	camera->d_y = vec_scale(camera_y, camera->height / (float)yres);

	//start at bottom corner (the plane's "origin")
	camera->origin = vec_sub(camera->center, vec_scale(camera_x, camera->width / 2.0));
	camera->origin = vec_sub(camera->origin, vec_scale(camera_y, camera->height / 2.0));
}

int key_hook(int keycode, void *param)
{
	t_param *p = (t_param *)param;
	if (keycode == quit_key)
	{
		mlx_destroy_image(p->mlx, p->img);
		mlx_destroy_window(p->mlx, p->win);
		exit(0);
	}
	return (1);
}

int main(int ac, char **av)
{
	srand(time(NULL));

	//bvh construction now happens inside scene_from_obj
	Scene *scene = scene_from_obj("objects/sponza/", "sponza.obj");

	printf("loaded scene. it has %d faces and %d materials\n", scene->face_count, scene->mat_count);

	int xdim, ydim;
	if (ac < 2)
	{
		xdim = 512;
		ydim = 512;
	}
	else
	{
		xdim = atoi(av[1]);
		ydim = atoi(av[2]);
	}

	t_camera cam;

	if (ac <= 3)
	{
		//default view
		cam.center = (cl_float3){800.0, 250.0, 0.0};
		cam.normal = (cl_float3){-1.0, 0.0, 0.0};
	}
	//pointed at hanging vase
	if (strcmp(av[3], "vase") == 0)
	{
		cam.center = (cl_float3){-620.0, 130.0, 50.0};
		cam.normal = (cl_float3){0.0, 0.0, 1.0};
	}

	//central view
	if (strcmp(av[3], "center") == 0)
	{
		cam.center = (cl_float3){-100.0, 330.0, 0.0};
		cam.normal = (cl_float3){-1.0, 0.0, 0.0};
	}

	//central view with drape
	if (strcmp(av[3], "drape") == 0)
	{
		cam.center = (cl_float3){800.0, 600.0, 0.0};
		cam.normal = (cl_float3){-1.0, 0.0, 0.0};
	}

	//2nd floor hall right
	if (strcmp(av[3], "hall") == 0)
	{
		cam.center = (cl_float3){700.0, 500.0, 350.0};
		cam.normal = (cl_float3){-1.0, 0.0, 0.0};
	}

	//2nd floor hall right facing center
	if (strcmp(av[3], "hall2") == 0)
	{
		cam.center = (cl_float3){500.0, 500.0, 380.0};
		cam.normal = (cl_float3){0.0, 0.0, -1.0};
	}

	//rounded column and some drape
	if (strcmp(av[3], "mix") == 0)
	{
		cam.center = (cl_float3){200.0, 650.0, -200.0};
		cam.normal = (cl_float3){-1.0, 0.0, 0.0};
	}
	//rounded column
	if (strcmp(av[3], "column") == 0)
	{
		cam.center = (cl_float3){40.0, 600.0, -280.0};
		cam.normal = (cl_float3){-1.0, 0.0, 0.0};
	}

	//lion
	if (strcmp(av[3], "lion") == 0)
	{
		cam.center = (cl_float3){-1050.0, 160.0, -40.0};
		cam.normal = (cl_float3){-1.0, 0.0, 0.0};
	}

	cam.normal = unit_vec(cam.normal);
	cam.width = 1.0;
	cam.height = 1.0;
	init_camera(&cam, xdim, ydim);

	cl_float3 *pixels = gpu_render(scene, cam, xdim, ydim);

	//baby_tone_map(pixels, xdim * ydim);
	
	void *mlx = mlx_init();
	void *win = mlx_new_window(mlx, xdim, ydim, "RTV1");
	void *img = mlx_new_image(mlx, xdim, ydim);
	draw_pixels(img, xdim, ydim, pixels);
	mlx_put_image_to_window(mlx, win, img, 0, 0);

	t_param *param = calloc(1, sizeof(t_param));
	*param = (t_param){mlx, win, img, xdim, ydim, scene, cam, NULL, 0};
	
	mlx_key_hook(win, key_hook, param);
	//mlx_loop_hook(mlx, loop_hook, param);
	mlx_loop(mlx);
}