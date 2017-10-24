#include "rt.h"

#define xdim 1000
#define ydim 1000

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

	// t_float3 offset = (t_float3){0,0,0.25};
	// p->scene->camera.center = vec_add(p->scene->camera.center, offset);
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

#define ROOMSIZE 10

int main(int ac, char **av)
{

	t_scene *scene = calloc(1, sizeof(t_scene));

	t_import import;

	import = load_file(ac, av);
	unit_scale(import, (t_float3){1, 1, 0});
	import.tail->next = scene->objects;
	scene->objects = import.head;

	t_float3 left_bot_back = (t_float3){-1 * ROOMSIZE / 2, -1 * ROOMSIZE / 2, -1 *ROOMSIZE / 2};
	t_float3 left_bot_front = (t_float3){-1 * ROOMSIZE / 2, -1 * ROOMSIZE / 2, ROOMSIZE / 2};
	t_float3 left_top_back = (t_float3){-1 * ROOMSIZE / 2, ROOMSIZE / 2, -1 *ROOMSIZE / 2};
	t_float3 left_top_front = (t_float3){-1 * ROOMSIZE / 2, ROOMSIZE / 2, ROOMSIZE / 2};
	t_float3 right_bot_back = (t_float3){ROOMSIZE / 2, -1 * ROOMSIZE / 2, -1 *ROOMSIZE / 2};
	t_float3 right_bot_front = (t_float3){ROOMSIZE / 2, -1 * ROOMSIZE / 2, ROOMSIZE / 2};
	t_float3 right_top_back = (t_float3){ROOMSIZE / 2, ROOMSIZE / 2, -1 *ROOMSIZE / 2};
	t_float3 right_top_front = (t_float3){ROOMSIZE / 2, ROOMSIZE / 2, ROOMSIZE / 2};

	new_plane(scene, left_bot_back, left_bot_front, left_top_front, WHITE); //left wall
	//new_plane(scene, left_bot_back, right_bot_back, right_top_back, WHITE); // back wall
	new_plane(scene, left_bot_back, left_bot_front, right_bot_front, WHITE); // floor
	new_plane(scene, right_bot_back, right_bot_front, right_top_front, WHITE); //right wall
	new_plane(scene, left_top_front, right_top_front, right_top_back, WHITE); //ceiling
	new_plane(scene, left_bot_front, left_top_front, right_top_front, WHITE); //front wall

	new_sphere(scene, 0, 4, 0, 0.5, WHITE);
	scene->objects->emission = 1.0;

	make_bvh(scene);

	scene->camera = (t_camera){	(t_float3){0, 0, -5},
								(t_float3){0, 0, 1},
								1.0,
								1.0};

	void *mlx = mlx_init();
	void *win = mlx_new_window(mlx, xdim, ydim, "RTV1");
	void *img = mlx_new_image(mlx, xdim, ydim);
	t_float3 *pixels = simple_render(scene, xdim, ydim);
	draw_pixels(img, xdim, ydim, pixels);
	mlx_put_image_to_window(mlx, win, img, 0, 0);

	t_param *param = calloc(1, sizeof(t_param));
	*param = (t_param){mlx, win, img, xdim, ydim, scene};

	mlx_loop_hook(mlx, loop_hook, param);
	
	mlx_key_hook(win, key_hook, param);
	mlx_loop(mlx);

}