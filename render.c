#include "rt.h"

//completely arbitrary values
#define ka 0.5
#define kd 2
#define ks 1

#define Ia 1

//using a point light for simplicity for now
t_float3 clear_shot(const t_float3 point, const t_float3 N, const t_scene *scene)
{
	//is there a clear shot from this point to the light? (note need to move point slightly off surface)
	t_float3 offset = vec_add(point, vec_scale(N, ERROR));
	t_ray L;
	L.origin = offset;
	L.direction = unit_vec(vec_sub(scene->light, point));
	L.color = WHITE;
	L.inv_dir = vec_inv(L.direction);
	if (hit(&L, scene, 0) && dist(L.origin, point) < dist(point, scene->light))
		return (t_float3){0, 0, 0};
	else
		return L.direction;
}
int g_tests = 0;
int g_singles = 0;
int g_nonsingle = 0;
int g_max_tests = 0;

int hit(t_ray *ray, const t_scene *scene, int do_shade)
{
	//printf("hit start\n");
	// the goal of hit() is to check against all objects in the scene for intersection with Ray.
	// returns 1/0 for hit/miss, stores resulting bounced ray in out.

	t_float3 closest;
	float closest_dist = FLT_MAX;
	t_object *closest_object = NULL;
	int tests = 0;
	hit_nearest(ray, scene->bvh, &closest_object, &closest_dist, &tests);
	g_tests += tests;
	if (tests == 1)
		g_singles++;
	else
		g_nonsingle += tests;
	if (tests > g_max_tests)
		g_max_tests = tests;
	//hit_nearest_debug(ray, scene, &closest_object, &closest_dist);
	//printf("hit decided\n");
	if (closest_object == NULL)
		return 0; //nothing was hit
	//printf("hit something\n");
	closest = vec_add(ray->origin, vec_scale(ray->direction, closest_dist));
	if (!do_shade)
	{
		ray->origin = closest;
		return (1);
	}
	ray->origin = closest;
	//normal at collision point
	t_float3 N = norm_object(closest_object, ray);
	//vector to light source (0,0,0 if no line)
	t_float3 L = clear_shot(closest, N, scene);
	//vector to camera
	t_float3 V = vec_rev(ray->direction);
	//reflected light ray
	t_float3 R = bounce(vec_rev(L), N);

	float ambient = Ia * ka;
	float diffuse = kd * dot(L, N);
	float specular = pow(ks * dot(V, R), 10);

	if (diffuse < 0)
		printf("diffuse wtf\n");

	float illumination = ambient + diffuse + specular;

	//jank normalization
	illumination /= (ka + kd + ks);
	illumination = fmin(1.0, illumination);

	t_float3 dir = bounce(ray->direction, N);
	*ray = (t_ray){closest, dir, vec_scale(closest_object->color, illumination), vec_inv(dir)};
	return (1);
}

#define H_FOV M_PI_2 * 60.0 / 90.0 	//60 degrees. eventually this will be dynamic with aspect
									// V_FOV is implicit with aspect of view-plane

t_ray *rays_from_camera(t_plane camera, int xres, int yres)
{
	//xres and yres will likely end up in some struct.

	t_ray *rays = calloc(xres * yres, sizeof(t_ray));

	//determine a focus point in front of the view-plane
	//such that the edge-focus-edge vertex has angle H_FOV
	float d = (camera.width / 2.0) / tan(H_FOV / 2.0);
	t_float3 focus = vec_add(camera.center, vec_scale(camera.normal, d));

	//now i need unit vectors on the plane
	t_3x3 camera_matrix = rotation_matrix(UNIT_Z, camera.normal);
	t_float3 camera_x, camera_y;
	camera_x = mat_vec_mult(camera_matrix, UNIT_X);
	camera_y = mat_vec_mult(camera_matrix, UNIT_Y);

	//pixel deltas
	t_float3 delta_x, delta_y;
	delta_x = vec_scale(camera_x, camera.width / (float)xres);
	delta_y = vec_scale(camera_y, camera.height / (float)yres);

	//start at bottom corner (the plane's "origin")
	t_float3 origin = camera.center;
	origin = vec_sub(origin, vec_scale(camera_x, camera.width / 2.0));
	origin = vec_sub(origin, vec_scale(camera_y, camera.height / 2.0));

	for (int y = 0; y < yres; y++)
	{
		t_float3 from = origin;
		for (int x = 0; x < xres; x++)
		{
			rays[y * xres + x] = (t_ray){focus, unit_vec(vec_sub(focus, from)), WHITE, vec_inv(unit_vec(vec_sub(focus, from)))};
			from = vec_add(from, delta_x);
		}
		origin = vec_add(origin, delta_y);
	}

	return (rays);
}

t_float3 trace(t_ray *ray, const t_scene *scene)
{
	//this will be recursive and more complicated but for now isn't

	int test_track = g_tests;
	
	if (hit(ray, scene, 1))
	{
		if (g_tests - test_track > 2000)
			return RED;
		else
			return ray->color;
	}
	else
	{
		if (g_tests - test_track > 2000)
			return RED;
		else
			return BLACK;
	}
}

t_float3 *simple_render(const t_scene *scene, const int xres, const int yres)
{
	clock_t start, end;
	start = clock();
	t_float3 *pixels = calloc(xres * yres, sizeof(t_float3));
	t_ray *rays = rays_from_camera(scene->camera, xres, yres);
	for (int y = 0; y < yres; y++)
		for (int x = 0; x < xres; x++)
		{
			//printf("tracing\n");
			pixels[y * xres + x] = trace(&(rays[y * xres + x]), scene);
			//printf("traced\n");
		}
	end = clock();
	double cpu_time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("trace time %lf sec, %lf FPS\n", cpu_time, 1.0 / cpu_time);
	free(rays);
	return pixels;
}

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
	// p->scene->camera.center = angle_axis_rot(rot_angle, UNIT_Y, p->scene->camera.center);
	// p->scene->camera.normal = angle_axis_rot(rot_angle, UNIT_Y, p->scene->camera.normal);
	p->scene->light = angle_axis_rot(rot_angle, UNIT_Y, p->scene->light);
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

int main(int ac, char **av)
{

	t_scene *scene = calloc(1, sizeof(t_scene));

	t_import import;

	// import = load_file(ac, av);
	// unit_scale(import, (t_float3){0, 0, 0});
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	// import = load_file(ac, av);
	// unit_scale(import, (t_float3){1, 0, 0});
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	// import = load_file(ac, av);
	// unit_scale(import, (t_float3){1, 1, 0});
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	// import = load_file(ac, av);
	// unit_scale(import, (t_float3){1, 1, 1});
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	// import = load_file(ac, av);
	// unit_scale(import, (t_float3){0, 1, 0});
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	// import = load_file(ac, av);
	// unit_scale(import, (t_float3){0, 1, 1});
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	// import = load_file(ac, av);
	// unit_scale(import, (t_float3){0, 0, 1});
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	// import = load_file(ac, av);
	// unit_scale(import, (t_float3){1, 0, 1});
	// import.tail->next = scene->objects;
	// scene->objects = import.head;

	
	// new_plane(scene, (t_float3){0, 0, 100}, (t_float3){0, 0, -1}, (t_float3){100, 100, 0}, WHITE);
	// new_plane(scene, (t_float3){-100, 0, 0}, (t_float3){1, 0, 0}, (t_float3){0, 100, 100}, WHITE);
	// new_plane(scene, (t_float3){100, 0, 0}, (t_float3){-1, 0, 0}, (t_float3){0, 100, 100}, WHITE);
	// new_plane(scene, (t_float3){0, 100, 0}, (t_float3){0, -1, 0}, (t_float3){100, 0, 100}, WHITE);
	// new_plane(scene, (t_float3){0, -100, 0}, (t_float3){0, 1, 0}, (t_float3){100, 0, 100}, WHITE);

	for (int i = 1; i < 10; i++)
		for (int j = 1; j < 10; j++)
			for (int k = 1; k < 10; k++)
				new_sphere(scene, i * 2, j * 2, k * 2, 1, BLUE);
	// new_sphere(scene, -2, -4, 0, 1, GREEN);
	// new_sphere(scene, 0, 0, 0, 0.5, RED);

	// new_triangle(scene, (t_float3){-1, 0, 3}, (t_float3){1, 0, 3}, (t_float3){0, 2, 3}, BLUE);

	make_bvh(scene);

	scene->camera = (t_plane){	(t_float3){10, 10, -30},
								(t_float3){0, 0, 1},
								1.0,
								1.0};

	scene->light = (t_float3){0, 80, -30};

	void *mlx = mlx_init();
	void *win = mlx_new_window(mlx, xdim, ydim, "RTV1");
	void *img = mlx_new_image(mlx, xdim, ydim);
	g_tests = 0;
	t_float3 *pixels = simple_render(scene, xdim, ydim);
	printf("average %f tests per ray\n", (float)g_tests / 1000000.0);
	printf("%d single test rays (ie complete misses)\n", g_singles);
	printf("average of nonsingles is %f\n", (float)g_nonsingle / (float)(1000000 - g_singles));
	printf("heaviest ray was %d tests\n", g_max_tests);
	draw_pixels(img, xdim, ydim, pixels);
	mlx_put_image_to_window(mlx, win, img, 0, 0);

	t_param *param = calloc(1, sizeof(t_param));
	*param = (t_param){mlx, win, img, xdim, ydim, scene};

	//mlx_loop_hook(mlx, loop_hook, param);
	
	mlx_key_hook(win, key_hook, param);
	mlx_loop(mlx);

}