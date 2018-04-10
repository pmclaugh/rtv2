#include "rt.h"

#define EPS 0.00005
#define MIN_EDGE_WIDTH 1000
#define HEATMAP_RATIO .02f

typedef struct	s_ray
{
	cl_float3	origin;
	cl_float3	direction;
	cl_float3	inv_dir;
	cl_float3	N;
	cl_float3	color;
	float		t;
	_Bool		poly_edge;
	float		eps;
}				t_ray;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int inside_box(t_ray *ray, AABB *box)
{
	if (box->min.x <= ray->origin.x && ray->origin.x <= box->max.x)
		if (box->min.y <= ray->origin.y && ray->origin.y <= box->max.y)
			if (box->min.z <= ray->origin.z && ray->origin.z <= box->max.z)
				return 1;
	return 0;
}

static int intersect_box(t_ray *ray, AABB *box, float *t_out)
{
	float tx0 = (box->min.x - ray->origin.x) * ray->inv_dir.x;
	float tx1 = (box->max.x - ray->origin.x) * ray->inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	float ty0 = (box->min.y - ray->origin.y) * ray->inv_dir.y;
	float ty1 = (box->max.y - ray->origin.y) * ray->inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);


	if ((tmin >= tymax) || (tymin >= tmax))
		return (0);

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (box->min.z - ray->origin.z) * ray->inv_dir.z;
	float tz1 = (box->max.z - ray->origin.z) * ray->inv_dir.z;
	float tzmin = fmin(tz0, tz1);
	float tzmax = fmax(tz0, tz1);

	if ((tmin >= tzmax) || (tzmin >= tmax))
		return (0);

    tmin = fmax(tzmin, tmin);
	tmax = fmin(tzmax, tmax);

	if (tmin <= 0.0 && tmax <= 0.0)
		return (0);
	if (tmin > ray->t)
		return(0);
	if (t_out)
		*t_out = fmax(0.0f, tmin);
	return (1);
}

void	intersect_triangle(t_ray *ray, const cl_float3 v0, const cl_float3 v1, const cl_float3 v2)
{
	float t, u, v;
	cl_float3 e1 = vec_sub(v1, v0);
	cl_float3 e2 = vec_sub(v2, v0);

	cl_float3 h = cross(ray->direction, e2);
	float a = dot(h, e1);

	if (fabs(a) < 0)
		return ;
	float f = 1.0f / a;
	cl_float3 s = vec_sub(ray->origin, v0);
	u = f * dot(s, h);
	if (u < 0.0 || u > 1.0)
		return ;
	cl_float3 q = cross(s, e1);
	v = f * dot(ray->direction, q);
	if (v < 0.0 || u + v > 1.0)
		return ;
	t = f * dot(e2, q);
	if (t > 0 && t < ray->t)
	{
		float	edge_width = (t + MIN_EDGE_WIDTH) * ray->eps;
		ray->poly_edge = (u < edge_width || v < edge_width || 1 - u - v < edge_width) ? 1 : 0;
		ray->t = t;
		ray->N = unit_vec(cross(vec_sub(v1, v0), vec_sub(v2, v0)));
	}
}

void check_triangles(t_ray *ray, AABB *box)
{
	for (AABB *member = box->members; member; member = member->next)
		intersect_triangle(ray, member->f->verts[0], member->f->verts[1], member->f->verts[2]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void stack_push(AABB **stack, AABB *box)
{
	box->next = *stack;
	*stack = box;
}

static AABB *stack_pop(AABB **stack)
{
	AABB *popped = NULL;
	if (*stack)
	{
		popped = *stack;
		*stack = popped->next;
	}
	return popped;
}

void	trace_bvh_heatmap(AABB *tree, t_ray *ray)
{
	tree->next = NULL;
	AABB *stack = tree;

	ray->color = (cl_float3){0, 0, 1};
	while(stack)
	{
		AABB *box = stack_pop(&stack);
		if (intersect_box(ray, box, NULL))
		{
			if (ray->color.z > 0.0)
			{
				ray->color.y += HEATMAP_RATIO;
				ray->color.z -= HEATMAP_RATIO;
			}
			else if (ray->color.x < 1.0f)
				ray->color.x += HEATMAP_RATIO;
			else if (ray->color.y > 0.0f)
				ray->color.y -= HEATMAP_RATIO;
			if (box->left) //boxes have either 0 or 2 children
			{
				stack_push(&stack, box->left);
				stack_push(&stack, box->right);
			}
		}
	}
}

void	trace_bvh(AABB *tree, t_ray *ray)
{
	tree->next = NULL;
	AABB *stack = tree;

	ray->color = (cl_float3){1, 1, 1};
	while(stack)
	{
		AABB *box = stack_pop(&stack);
		if (intersect_box(ray, box, NULL))
		{
			ray->color.x *= .99;
			ray->color.y *= .99;
			ray->color.z *= .99;
			if (box->left) //boxes have either 0 or 2 children
			{
				stack_push(&stack, box->left);
				stack_push(&stack, box->right);
			}
		}
	}
}

void	trace_scene(AABB *tree, t_ray *ray, int view)
{
	tree->next = NULL;
	AABB *stack = tree;

	while(stack)
	{
		AABB *box = stack_pop(&stack);
		if (intersect_box(ray, box, NULL))
		{
			if (box->left) //boxes have either 0 or 2 children
			{
				float tleft, tright;
				int lret = intersect_box(ray, box->left, &tleft);
				int rret = intersect_box(ray, box->right, &tright);
				 if (lret && tleft >= tright)
                    push(&stack, box->left);
                if (rret)
                    push(&stack, box->right);
                if (lret && tleft < tright)
                    push(&stack, box->left);
			}
			else
				check_triangles(ray, box);
		}
	}
	if (ray->poly_edge && view == 2)
		ray->color = (cl_float3){.25, .25, .25};
	else
	{
		// cl_float3	light = unit_vec((cl_float3){.5, 1, -.25});
		float		cost = dot(ray->N, ray->direction);
		if (cost < 0)
			cost *= -1.0f;
		cost = ((cost - .5) / 2) + .5; //make greyscale contrast less extreme
		ray->color = (cl_float3){cost, cost, cost};
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// void	plot_line(t_env *env, int x1, int y1, int x2, int y2, cl_float3 clr)
// {
// 	// printf("------------------\n");
// 	// printf("x1 = %d\ty1 = %d\nx2 = %d\ty2 = %d\n", x1, y1, x2, y2);
// 	int		dx, dy, sx, sy, err, err2;

// 	dx = abs(x2 - x1);
// 	sx = x1 < x2 ? 1 : -1;
// 	dy = abs(y2 - y1);
// 	sy = y1 < y2 ? 1 : -1;
// 	err = (dx > dy ? dx : -dy) / 2;
// 	while (x1 != x2)
// 	{
// 		if (x1 >= 0 && x1 < DIM_IA && y1 >= 0 && y1 < DIM_IA)
// 			env->ia->pixels[(DIM_IA - x1) + (y1 * DIM_IA)] = clr; //is the image being flipped horizontally?
// 		err2 = err;
// 		if (err2 > -dx)
// 		{
// 			err -= dy;
// 			x1 += sx;
// 		}
// 		if (err2 < dy)
// 		{
// 			err += dx;
// 			y1 += sy;
// 		}
// 	}
// 	while (y1 != y2)
// 	{
// 		if (x1 >= 0 && x1 < DIM_IA && y1 >= 0 && y1 < DIM_IA)
// 			env->ia->pixels[(DIM_IA - x1) + (y1 * DIM_IA)] = clr;
// 		y1 += sy;
// 	}
// }

void	plot_line(t_env *env, int x1, int y1, int x2, int y2, cl_float3 clr)
{
	SDL_Renderer	*renderer = SDL_CreateRenderer(env->ia->win, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetRenderDrawColor(renderer, clr.x, clr.y, clr.z, SDL_ALPHA_OPAQUE);
	SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
	SDL_RenderPresent(renderer);
	// SDL_DestroyRenderer(renderer);
}

void	draw_ray(t_env *env, cl_float3 p1, cl_float3 p2, cl_float3 clr)
{
	float		pix_x1, pix_y1, pix_x2, pix_y2, ratio;
	cl_float3	dir1, dir2;
	float 		dist = (env->cam.width / 2) / tan(H_FOV / 2);
	t_3x3 		rot_hor = rotation_matrix(env->cam.hor_ref, UNIT_X);
	t_3x3 		rot_vert = rotation_matrix(env->cam.vert_ref, UNIT_Y);

	dir1 = unit_vec(vec_sub(p1, env->cam.pos));
	dir1 = mat_vec_mult(rot_hor, mat_vec_mult(rot_vert, dir1));
	ratio = (dir1.z > 0) ? dist / dir1.z : 100; //protection against inf when dir1.z is 0
	pix_x1 = ((dir1.x * ratio) * DIM_IA) + (DIM_IA / 2);
	pix_y1 = ((dir1.y * -ratio) * DIM_IA) + (DIM_IA / 2);

	dir2 = unit_vec(vec_sub(p2, env->cam.pos));
	dir2 = mat_vec_mult(rot_hor, mat_vec_mult(rot_vert, dir2));
	ratio = (dir2.z > 0) ? dist / dir2.z : 100;
	pix_x2 = ((dir2.x * ratio) * DIM_IA) + (DIM_IA / 2);
	pix_y2 = ((dir2.y * -ratio) * DIM_IA) + (DIM_IA / 2);

	plot_line(env, pix_x1, pix_y1, pix_x2, pix_y2, clr);
}

void	ray_visualizer(t_env *env)
{
	cl_float3	clr = (cl_float3){1, 0, 0};
	double		clr_delta = env->depth / 6;
	for (int i = 0; i < env->bounce_vis; i++)
	{
		for (int y = 0; y < DIM_PT; y += env->ray_density)
			for (int x = 0; x < DIM_PT; x += env->ray_density)
				draw_ray(env, env->rays[i][x + (y * DIM_PT)].origin, env->rays[i + 1][x + (y * DIM_PT)].origin, clr);
		if (clr.y < 1.0)
			clr.y += clr_delta;
		else if (clr.x > 0.0)
			clr.x -= clr_delta;
		else if (clr.z < 1.0)
			clr.z += clr_delta;
		else
			clr.y -= clr_delta;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

t_ray	generate_ray(t_env *env, float x, float y)
{
	t_ray ray;
	ray.origin = env->cam.focus;
	cl_float3 through = vec_add(env->cam.origin, vec_add(vec_scale(env->cam.d_x, x), vec_scale(env->cam.d_y, y)));
	ray.direction = unit_vec(vec_sub(env->cam.focus, through));
	ray.inv_dir.x = 1.0f / ray.direction.x;
	ray.inv_dir.y = 1.0f / ray.direction.y;
	ray.inv_dir.z = 1.0f / ray.direction.z;
	ray.N = ray.direction;
	ray.t = FLT_MAX;
	ray.poly_edge = 0;
	ray.eps = env->eps;
	return ray;
}

void	interactive(t_env *env)
{
	clock_t	frame_start = clock();
	set_camera(&env->cam, (float)DIM_IA);
	for (int y = 0; y < DIM_IA; y += 2)
	{
		for (int x = 0; x < DIM_IA; x += 2)
		{
			//find world co-ordinates from camera and pixel plane (generate ray)
			t_ray ray = generate_ray(env, x, y);
			if (env->view == 1 || env->view == 2)
				trace_scene(env->scene->bins, &ray, env->view);
			else if (env->view == 3)
				trace_bvh(env->scene->bins, &ray);
			else if (env->view == 4)
				trace_bvh_heatmap(env->scene->bins, &ray);
			//upscaling the resolution by 2x
			env->ia->pixels[x + (y * DIM_IA)] = (cl_float3){ray.color.x, ray.color.y, ray.color.z};
			env->ia->pixels[(x + 1) + (y * DIM_IA)] = (cl_float3){ray.color.x, ray.color.y, ray.color.z};
			env->ia->pixels[x + ((y + 1) * DIM_IA)] = (cl_float3){ray.color.x, ray.color.y, ray.color.z};
			env->ia->pixels[(x + 1) + ((y + 1) * DIM_IA)] = (cl_float3){ray.color.x, ray.color.y, ray.color.z};
		}
	}
	if (env->show_rays)
		ray_visualizer(env);
	draw_pixels(env->ia);
	// draw_pixels(env->ia, DIM_IA, DIM_IA);
	// mlx_put_image_to_window(env->mlx, env->ia->win, env->ia->img, 0, 0);
	// if (env->show_fps)
	// {
	// 	float frames = 1.0f / (((float)clock() - (float)frame_start) / (float)CLOCKS_PER_SEC);
	// 	char *fps = NULL;
	// 	asprintf(&fps, "%lf", frames);
	// 	mlx_string_put(env->mlx, env->ia->win, 0, 0, 0x00ff00, fps);
	// }
}