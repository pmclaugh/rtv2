#include "rt.h"

#define xdim 600
#define ydim 600

typedef struct s_param
{
	void *mlx;
	void *win;
	void *img;
	int x;
	int y;
	t_scene *scene;
}				t_param;

#define rot_angle M_PI_2 * 4.0 / 90.0

int loop_hook(void *param)
{
	t_param *p = (t_param *)param;

	t_float3 *pixels = simple_render(p->scene, p->x, p->y);
	draw_pixels(p->img, p->x, p->y, pixels);
	mlx_put_image_to_window(p->mlx, p->win, p->img, 0, 0);
	free(pixels);
	p->scene->camera.center = angle_axis_rot(rot_angle, UNIT_Y, p->scene->camera.center);
	p->scene->camera.normal = angle_axis_rot(rot_angle, UNIT_Y, p->scene->camera.normal);
	return (1);
}

int key_hook(int keycode, void *param)
{
	t_param *p = (t_param *)param;

	if (keycode == 12)
	{
		mlx_destroy_image(p->mlx, p->img);
		mlx_destroy_window(p->mlx, p->win);
		exit(0);
	}
	return (1);
}

#define ROOMSIZE 10.0

int main(int ac, char **av)
{
	srand(time(NULL));

	t_scene *scene = calloc(1, sizeof(t_scene));

	t_import import;

	import = load_file(ac, av, GREEN, MAT_DIFFUSE, 0.0);
	unit_scale(import, (t_float3){0, -3, 0}, 3);
	import.tail->next = scene->objects;
	scene->objects = import.head;

	// import = load_file(ac, av, WHITE, MAT_DIFFUSE, 0.0);
	// unit_scale(import, (t_float3){-3, -3, 0}, 3);
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	t_float3 left_bot_back = (t_float3){-1 * ROOMSIZE / 2, -1 * ROOMSIZE / 2, -1 *ROOMSIZE / 2};
	t_float3 left_bot_front = (t_float3){-1 * ROOMSIZE / 2, -1 * ROOMSIZE / 2, ROOMSIZE / 2};
	t_float3 left_top_back = (t_float3){-1 * ROOMSIZE / 2, ROOMSIZE / 2, -1 *ROOMSIZE / 2};
	t_float3 left_top_front = (t_float3){-1 * ROOMSIZE / 2, ROOMSIZE / 2, ROOMSIZE / 2};
	t_float3 right_bot_back = (t_float3){ROOMSIZE / 2, -1 * ROOMSIZE / 2, -1 *ROOMSIZE / 2};
	t_float3 right_bot_front = (t_float3){ROOMSIZE / 2, -1 * ROOMSIZE / 2, ROOMSIZE / 2};
	t_float3 right_top_back = (t_float3){ROOMSIZE / 2, ROOMSIZE / 2, -1 *ROOMSIZE / 2};
	t_float3 right_top_front = (t_float3){ROOMSIZE / 2, ROOMSIZE / 2, ROOMSIZE / 2};

	new_plane(scene, left_bot_back, left_bot_front, left_top_front, RED, MAT_DIFFUSE, 0.0); //left wall
	//new_plane(scene, left_bot_back, right_bot_back, right_top_back, WHITE, MAT_DIFFUSE, 0.0); // front wall
	new_plane(scene, left_bot_back, left_bot_front, right_bot_front, WHITE, MAT_DIFFUSE, 0.0); // floor
	new_plane(scene, right_bot_back, right_bot_front, right_top_front, BLUE, MAT_DIFFUSE, 0.0); //right wall
	new_plane(scene, left_top_front, right_top_front, right_top_back, WHITE, MAT_DIFFUSE, 0.0); //ceiling
	new_plane(scene, left_bot_front, left_top_front, right_top_front, WHITE, MAT_DIFFUSE, 0.0); //back wall

	new_sphere(scene, (t_float3){-2, -2, 2}, 2.0, WHITE, MAT_SPECULAR, 0.0);

	new_sphere(scene, (t_float3){0, 3, 0}, 1.0, WHITE, MAT_NULL, 800.0);

	make_bvh(scene);


	t_camera cam;
	cam.center = (t_float3){0, 0, -8};
	cam.normal = (t_float3){0, 0, 1};
	cam.width = 1.0;
	cam.height = 1.0;
	scene->camera = cam;
	init_camera(&scene->camera, xdim, ydim);

	//debug_render(scene, 300, xdim, 300, ydim);

	t_float3 *pixels = simple_render(scene, xdim, ydim);
	printf("left render\n");
	void *mlx = mlx_init();
	void *win = mlx_new_window(mlx, xdim, ydim, "RTV1");
	void *img = mlx_new_image(mlx, xdim, ydim);
	draw_pixels(img, xdim, ydim, pixels);
	mlx_put_image_to_window(mlx, win, img, 0, 0);

	t_param *param = calloc(1, sizeof(t_param));
	*param = (t_param){mlx, win, img, xdim, ydim, scene};

	//mlx_loop_hook(mlx, loop_hook, param);
	
	mlx_key_hook(win, key_hook, param);
	mlx_loop(mlx);

}