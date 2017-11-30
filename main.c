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

#define H_FOV M_PI_2 * 60.0 / 90.0 	//60 degrees. eventually this will be dynamic with aspect
									// V_FOV is implicit with aspect of view-plane

void init_camera(t_camera *camera, int xres, int yres)
{
	//this does not work correctly!
	//weird skewed viewpoint for non-axial normal


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

	Face *bunny = ply_import("objects/ply/bunny.ply");
	Scene *S = calloc(1, sizeof(Scene));
	S->faces = bunny;
	flatten(S);
	printf("flattened scene array has %d faces\n", S->face_count);
	build_sbvh(S);
}