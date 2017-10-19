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
	t_ray L = (t_ray){offset, unit_vec(vec_sub(scene->light, point)), (t_float3){1,1,1}};
	if (hit(&L, scene, 0) && dist(L.origin, point) < dist(point, scene->light))
		return (t_float3){0, 0, 0};
	else
		return L.direction;
}

int hit(t_ray *ray, const t_scene *scene, int do_shade)
{
	//printf("hit start\n");
	// the goal of hit() is to check against all objects in the scene for intersection with Ray.
	// returns 1/0 for hit/miss, stores resulting bounced ray in out.

	t_float3 closest;
	float closest_dist = FLT_MAX;
	t_object *closest_object = NULL;
	hit_nearest(ray, scene->bvh, &closest_object, &closest_dist);
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

	float illumination = ambient + diffuse + specular;

	//jank normalization
	illumination /= (ka + kd + ks);

	*ray = (t_ray){closest, bounce(ray->direction, N), vec_scale(closest_object->color, illumination)};
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
			rays[y * xres + x] = (t_ray){focus, unit_vec(vec_sub(focus, from)), (t_float3){1,1,1}};
			from = vec_add(from, delta_x);
		}
		origin = vec_add(origin, delta_y);
	}

	return (rays);
}

t_float3 trace(t_ray *ray, const t_scene *scene)
{
	//this will be recursive and more complicated but for now isn't
	if (hit(ray, scene, 1))
		return ray->color;
	else
		return BLACK;
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

	if (ac != 1)
		load_file(scene, ac, av);

	
	new_plane(scene, (t_float3){0, 0, 100}, (t_float3){0, 0, -1}, (t_float3){100, 100, 0}, WHITE);
	new_plane(scene, (t_float3){-100, 0, 0}, (t_float3){1, 0, 0}, (t_float3){0, 100, 100}, WHITE);
	new_plane(scene, (t_float3){100, 0, 0}, (t_float3){-1, 0, 0}, (t_float3){0, 100, 100}, WHITE);
	new_plane(scene, (t_float3){0, 100, 0}, (t_float3){0, -1, 0}, (t_float3){100, 0, 100}, WHITE);
	new_plane(scene, (t_float3){0, -100, 0}, (t_float3){0, 1, 0}, (t_float3){100, 0, 100}, WHITE);

	// new_sphere(scene, -2, -4, 0, 1, GREEN);
	// new_sphere(scene, 3, -3, 2, 2, RED);

	// new_triangle(scene, (t_float3){-1, 0, 3}, (t_float3){1, 0, 3}, (t_float3){0, 2, 3}, BLUE);

	make_bvh(scene);

	scene->camera = (t_plane){	(t_float3){0, 0, -300},
								(t_float3){0, 0, 1},
								1.0,
								1.0};

	scene->light = (t_float3){70, 90, 0};

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

/*
optimization insights:
need to improve bvh creation so it doesn't go so deep
optimize intersect_box
vector functions seem to be pretty fast.
intersect plane is really bad too, i might just want to make planes into 2 triangles lol
*/