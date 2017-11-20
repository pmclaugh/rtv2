#include "rt.h"

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

void baby_tone_map(cl_float3 *pixels, int count)
{
	for (int i = 0; i < count; i++)
	{
		pixels[i].x = pixels[i].x / (pixels[i].x + 1.0);
		pixels[i].y = pixels[i].y / (pixels[i].y + 1.0);
		pixels[i].z = pixels[i].z / (pixels[i].z + 1.0);
	}
}

int loop_hook(void *param)
{
	t_param *p = (t_param *)param;

	cl_float3 *pixels = gpu_render(p->scene, p->scene->camera);
	baby_tone_map(pixels, xdim * ydim);
	draw_pixels(p->img, p->x, p->y, pixels);
	mlx_put_image_to_window(p->mlx, p->win, p->img, 0, 0);
	free(pixels);
	p->scene->camera.center = vec_add(p->scene->camera.center, (t_float3){0.0, 0.0, 0.1});
	init_camera(&p->scene->camera, xdim, ydim);
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

float vmag(cl_float3 v)
{
	return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float vsum(cl_float3 v)
{
	return v.x + v.y + v.z;
}

float vmax(cl_float3 v)
{
	return fmax(fmax(v.x, v.y), v.z);
}

void reinhard_tone_map(cl_float3 *pixels, int count)
{
	//get greyscale luminance values for all pixels,
	//and pixels become color masks
	float *lums = calloc(count, sizeof(float));
	for (int i = 0; i < count; i++)
	{
		lums[i] = vmag(pixels[i]);
		pixels[i].x /= lums[i];
		pixels[i].y /= lums[i];
		pixels[i].z /= lums[i];
	}

	//compute log average of luminances
	double lavg = 0.0;
	for (int i = 0; i < count; i++)
		lavg += log(lums[i]);
	printf("lavg before exp: %lf\n", lavg / (double)count);
	lavg = exp(lavg / (double)count);

	printf("lavg: %lf\n", lavg);
	//map so log-average value is now mid-gray
	for (int i = 0; i < count; i++)
		lums[i] = lums[i] * 0.5 / lavg;

	//compress high luminances
	for (int i = 0; i < count; i++)
		lums[i] = lums[i] / (lums[i] + 1.0);

	//re-color
	for (int i = 0; i < count; i++)
	{
		pixels[i].x *= lums[i];
		pixels[i].y *= lums[i];
		pixels[i].z *= lums[i];
	}
}

int main(int ac, char **av)
{
	srand(time(NULL));

	t_scene *scene = calloc(1, sizeof(t_scene));

	t_import import;

	// import = load_file(ac, av, GREEN, MAT_DIFFUSE, 0.0);
	// unit_scale(import, (t_float3){0, -3, 0}, 3);
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

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
	new_plane(scene, left_bot_front, left_top_front, right_top_front, GREEN, MAT_DIFFUSE, 0.0); //back wall

	new_sphere(scene, (t_float3){0.0, -2.0, 0.0}, 1.0, WHITE, MAT_DIFFUSE, 0.0);
	new_sphere(scene, (t_float3){2.0, 2.0, 2.0}, 1.0, WHITE, MAT_DIFFUSE, 0.0);
	new_sphere(scene, (t_float3){-2.0, 2.0, 2.0}, 1.0, WHITE, MAT_DIFFUSE, 0.0);

	//***** IMPORTANT ********
	//for now the light needs to be the last object added and there can only be one light.
	new_sphere(scene, (t_float3){0, 3.9, 1.0}, 1.0, WHITE, MAT_NULL, 500.0);

	//make_bvh(scene);


	t_camera cam;
	cam.center = (t_float3){0, 0.0, -14.0};
	cam.normal = (t_float3){0, 0, 1};
	cam.width = 1.0;
	cam.height = 1.0;
	scene->camera = cam;
	init_camera(&scene->camera, xdim, ydim);

	//debug_render(scene, 300, xdim, 300, ydim);
	cl_float3 *pixels = gpu_render(scene, scene->camera);
	printf("left render\n");

	baby_tone_map(pixels, xdim * ydim);
	printf("tone mapped\n");
	

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