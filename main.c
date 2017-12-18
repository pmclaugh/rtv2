#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
# define quit_key 12
#endif

#define xdim 512
#define ydim 512

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

//THIS DOESNT WORK RIGHT, JUST FOR TESTING
#define H_FOV M_PI_2 * 60.0 / 90.0
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
	// Face *bunny = ply_import("objects/ply/bunny.ply");
	// int count;
	// bunny = object_flatten(bunny, &count);
	// int box_count;
	// tree_box *bun_bvh =  super_bvh(bunny, count, &box_count);
	
	// printf("CL_INVALID_PROGRAM_EXECUTABLE %d\n", CL_INVALID_PROGRAM_EXECUTABLE);
	// printf("CL_INVALID_COMMAND_QUEUE %d\n", CL_INVALID_COMMAND_QUEUE);
	// printf("CL_INVALID_KERNEL %d\n", CL_INVALID_KERNEL);
	// printf("CL_INVALID_CONTEXT %d\n", CL_INVALID_CONTEXT);
	// printf("CL_INVALID_KERNEL_ARGS %d\n", CL_INVALID_KERNEL_ARGS);
	// printf("CL_INVALID_WORK_DIMENSION %d\n", CL_INVALID_WORK_DIMENSION);
	// printf("CL_INVALID_GLOBAL_WORK_SIZE %d\n", CL_INVALID_GLOBAL_WORK_SIZE);

	Scene *sponza = scene_from_obj("objects/sponza/", "sponza.obj");
	int bin_count;
	tree_box *sponza_bvh = super_bvh(sponza->faces, sponza->face_count, &bin_count);
	sponza->bins = sponza_bvh;
	sponza->bin_count = bin_count;

	flatten_bvh(sponza_bvh, bin_count);

	t_camera cam;
	cam.center = (cl_float3){-200.0, 200.0, 0.0};
	cam.normal = (cl_float3){-1.0, 0.0, 0.0};
	cam.width = 1.0;
	cam.height = 1.0;
	init_camera(&cam, 512, 512);
	//printf("%lu\n", sizeof(gpu_bin));
	cl_double3 *pixels = gpu_render(sponza, cam, 512, 512);

	void *mlx = mlx_init();
	void *win = mlx_new_window(mlx, xdim, ydim, "RTV1");
	void *img = mlx_new_image(mlx, xdim, ydim);
	draw_pixels(img, xdim, ydim, pixels);
	mlx_put_image_to_window(mlx, win, img, 0, 0);

	t_param *param = calloc(1, sizeof(t_param));
	*param = (t_param){mlx, win, img, xdim, ydim, sponza, cam, NULL, 0};
	
	mlx_key_hook(win, key_hook, param);
	mlx_loop(mlx);
}
