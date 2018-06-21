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

static float scalar_project(cl_float3 a, cl_float3 b)
{
	//scalar projection of a onto b aka mag(vec_project(a,b))
	return dot(a, unit_vec(b));
}

cl_float3	cam_perspective(t_env *env, const cl_float3 a)
{
	cl_float3	out;

	out.x = scalar_project(a, env->cam.hor_ref);
	out.y = scalar_project(a, env->cam.vert_ref);
	out.z = scalar_project(a, env->cam.dir);

	return out;
}

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

int		plot_line(t_env *env, int x1, int y1, int x2, int y2)
{
	int		dx, dy, sx, sy, err, err2, i;

	dx = abs(x2 - x1);
	sx = x1 < x2 ? 1 : -1;
	dy = abs(y2 - y1);
	sy = y1 < y2 ? 1 : -1;
	err = (dx > dy ? dx : -dy) / 2;
	i = 0;
	while (x1 != x2)
	{
		// if (x1 >= 0 && x1 < DIM_IA && y1 >= 0 && y1 < DIM_IA)
		// 	env->ia->pixels[(DIM_IA - x1) + (y1 * DIM_IA)] = clr; //is the image being flipped horizontally?
		err2 = err;
		if (err2 > -dx)
		{
			err -= dy;
			x1 += sx;
		}
		if (err2 < dy)
		{
			err += dx;
			y1 += sy;
		}
		i++;
	}
	while (y1 != y2)
	{
		// if (x1 >= 0 && x1 < DIM_IA && y1 >= 0 && y1 < DIM_IA)
			// env->ia->pixels[(DIM_IA - x1) + (y1 * DIM_IA)] = clr;
		y1 += sy;
		i++;
	}
	return i;
}

float	plane_intersect(cl_float3 line_dir, cl_float3 line_origin, cl_float3 plane_N, cl_float3 plane_origin, int *index)
{
	float	cost = dot(vec_scale(line_dir, -1), plane_N);
	if (cost == 0)
		return -1;
	else
	{
		*index = (cost > 0) ? 2 : 1; //which side of plane was intersected? inside or outside of view frustrum?
		return dot(vec_sub(line_origin, plane_origin), plane_N) / cost;
	}
}

void	clip_line(t_env *env, cl_float3 *p1_in, cl_float3 *p2_in, _Bool *p1_clip, _Bool *p2_clip)
{
	cl_float3	p1 = *p1_in, p2 = *p2_in, intersection;
	cl_float3	line = vec_sub(p2, p1);
	cl_float3	line_dir = unit_vec(line);
	float		line_mag = vec_mag(line);

	float		t;
	int			index = 0;
	t = plane_intersect(line_dir, p1, env->cam.view_frustrum_top, env->cam.focus, &index);
	if (t > 0 && t < line_mag)
	{
		intersection = vec_add(p1, vec_scale(line_dir, t));
		float	p_x = scalar_project(unit_vec(vec_sub(intersection, env->cam.focus)), env->cam.hor_ref);
		if (p_x > -sin(H_FOV / 2) && p_x < sin(H_FOV / 2))
		{
			if (index == 1)
			{
				*p1_in = intersection;
				*p1_clip = 1;
			}
			else if (index == 2)
			{
				*p2_in = intersection;
				*p2_clip = 1;
			}
		}
	}
	t = plane_intersect(line_dir, p1, env->cam.view_frustrum_bottom, env->cam.focus, &index);
	if (t > 0 && t < line_mag)
	{
		intersection = vec_add(p1, vec_scale(line_dir, t));
		float	p_x = scalar_project(unit_vec(vec_sub(intersection, env->cam.focus)), env->cam.hor_ref);
		if (p_x > -sin(H_FOV / 2) && p_x < sin(H_FOV / 2))
		{
			if (index == 1)
			{
				*p1_in = intersection;
				*p1_clip = 1;
			}
			else if (index == 2)
			{
				*p2_in = intersection;
				*p2_clip = 1;
			}
		}
	}
	t = plane_intersect(line_dir, p1, env->cam.view_frustrum_left, env->cam.focus, &index);
	if (t > 0 && t < line_mag)
	{
		intersection = vec_add(p1, vec_scale(line_dir, t));
		float	p_y = scalar_project(unit_vec(vec_sub(intersection, env->cam.focus)), env->cam.vert_ref);
		if (p_y > -sin(H_FOV / 2) && p_y < sin(H_FOV / 2))
		{
			if (index == 1)
			{
				*p1_in = intersection;
				*p1_clip = 1;
			}
			else if (index == 2)
			{
				*p2_in = intersection;
				*p2_clip = 1;
			}
		}
	}
	t = plane_intersect(line_dir, p1, env->cam.view_frustrum_right, env->cam.focus, &index);
	if (t > 0 && t < line_mag)
	{
		intersection = vec_add(p1, vec_scale(line_dir, t));
		float	p_y = scalar_project(unit_vec(vec_sub(intersection, env->cam.focus)), env->cam.vert_ref);
		if (p_y > -sin(H_FOV / 2) && p_y < sin(H_FOV / 2))
		{
			if (index == 1)
			{
				*p1_in = intersection;
				*p1_clip = 1;
			}
			else if (index == 2)
			{
				*p2_in = intersection;
				*p2_clip = 1;
			}
		}
	}
	// if (index != 0)
	// 	return ;
	// t = plane_intersect(line_dir, p1, env->cam.dir, vec_add(env->cam.focus, vec_scale(env->cam.dir, env->cam.dist)), &index);
	// if (t > 0 && t < line_mag)
	// {
	// 	intersection = vec_add(p1, vec_scale(line_dir, t));
	// 	if (index == 1)
	// 		*p1_in = intersection;
	// 	else if (index == 2)
	// 		*p2_in = intersection;
		// printf("--------------------------------\n");
		// print_vec(env->cam.focus);
		// print_vec(intersection);
	// }
}

void	draw_ray(t_env *env, cl_float3 p1, cl_float3 p2, cl_float3 clr, float *t_array)
{
	/* Convert points from global space to camera space */
	//////////////////////////////////////////////////////////////////////////////////
	cl_float3	cam_to_p1, cam_to_p2, dir_p1, dir_p2, tmp;
	float		dir_p1_z, dir_p2_z, p1_dist;
	_Bool		p1_clip = 0, p2_clip = 0;

	cam_to_p1 = vec_sub(p1, env->cam.focus);
	cam_to_p2 = vec_sub(p2, env->cam.focus);
	dir_p1 = unit_vec(cam_to_p1);
	dir_p2 = unit_vec(cam_to_p2);
	dir_p1_z = scalar_project(dir_p1, env->cam.dir);
	dir_p2_z = scalar_project(dir_p2, env->cam.dir);
	if (dir_p1_z <= 0 && dir_p2_z <= 0)
		return ;
	if (dir_p1_z < cos(H_FOV / 2) || dir_p2_z < cos(H_FOV / 2))
		clip_line(env, &p1, &p2, &p1_clip, &p2_clip);
	
	cam_to_p1 = vec_sub(p1, env->cam.focus);
	cam_to_p2 = vec_sub(p2, env->cam.focus);
	
	//draw point that is in view first (swap p1 and p2 if only p2 is in view)
	if (p1_clip && !p2_clip)
	{
		dir_p1 = unit_vec(cam_to_p2);
		dir_p2 = unit_vec(cam_to_p1);
		tmp = p1;
		p1 = p2;
		p2 = tmp;
	}
	else
	{
		dir_p1 = unit_vec(cam_to_p1);
		dir_p2 = unit_vec(cam_to_p2);
	}

	cl_float3	cam_dir_p1 = cam_perspective(env, dir_p1);
	cl_float3	cam_dir_p2 = cam_perspective(env, dir_p2);
	//////////////////////////////////////////////////////////////////////////////////
	/* Compute raster coordinates */
	//////////////////////////////////////////////////////////////////////////////////
	int		x1, y1, x2, y2;
	float	ratio, t;

	if (cam_dir_p1.z <= 0)
		return ;
	ratio = env->cam.dist / cam_dir_p1.z;
	x1 = ((cam_dir_p1.x * ratio) * DIM_IA) + (DIM_IA / 2);
	y1 = ((cam_dir_p1.y * -ratio) * DIM_IA) + (DIM_IA / 2);

	if (cam_dir_p2.z <= 0)
		return ;
	ratio = env->cam.dist / cam_dir_p2.z;
	x2 = ((cam_dir_p2.x * ratio) * DIM_IA) + (DIM_IA / 2);
	y2 = ((cam_dir_p2.y * -ratio) * DIM_IA) + (DIM_IA / 2);
	//////////////////////////////////////////////////////////////////////////////////
	/* Pre-plot line for ray-step */
	//////////////////////////////////////////////////////////////////////////////////
	// int steps = 0;
	// _Bool behind = 0;
	// steps = plot_line(env, x1, y1, x2, y2);
	cl_float3	line = vec_sub(p2, p1);
	// float		line_mag = vec_mag(line);
	// cl_float3	ray_step = vec_scale(line, 1 / (float)steps);
	cl_float3	ray_dir = unit_vec(line);
	//////////////////////////////////////////////////////////////////////////////////
	/* Bresingham's line drawing algorithm */
	//////////////////////////////////////////////////////////////////////////////////
	cl_float3	point = p1, d_x, d_y;
	int			dx, dy, sx, sy, err, err2, tmp_x = x1, tmp_y = y1;
	float 		a, b, A, B;
	
	ratio = scalar_project(cam_to_p1, env->cam.dir) / env->cam.dist;

	dx = abs(x2 - x1);
	sx = x1 < x2 ? 1 : -1;
	dy = abs(y2 - y1);
	sy = y1 < y2 ? 1 : -1;
	err = (dx > dy ? dx : -dy) / 2;
	if (err > 100000)
		return ;
	while (x1 != x2)
	{
		//compute distance to anchor pixel
		d_x = vec_scale(env->cam.d_x, x1 - tmp_x);
		d_y = vec_scale(env->cam.d_y, tmp_y - y1);
		//calculate corresponding horizontal component A
		A = vec_mag(vec_add(d_x, d_y)) * ratio;
		cl_float3	pixel_pos = vec_add(env->cam.origin, vec_add(vec_scale(env->cam.d_x, DIM_IA - (x1 + 1)), vec_scale(env->cam.d_y, y1)));
		cl_float3	point_dir = unit_vec(vec_sub(env->cam.focus, pixel_pos));
		a = acos(dot(point_dir, ray_dir));
		if (x1 >= (DIM_IA / 2) - 1)
			b = acos(dot(env->cam.hor_ref, point_dir));
		else
			b = acos(dot(vec_scale(env->cam.hor_ref, -1), point_dir));
		B = A * sin(b) / sin(a);
		point = vec_add(p1, vec_scale(ray_dir, B));
		// point = vec_add(point, ray_step);

		cl_float3 cam_to_point = vec_sub(point, env->cam.focus);
		if (x1 >= 0 && x1 < DIM_IA && y1 >= 0 && y1 < DIM_IA)
		{
			t = vec_mag(cam_to_point);
			if (t <= t_array[(DIM_IA - x1) + (y1 * DIM_IA)])
				env->ia->pixels[(DIM_IA - x1) + (y1 * DIM_IA)] = clr;
		}
		err2 = err;
		if (err2 > -dx)
		{
			err -= dy;
			x1 += sx;
		}
		if (err2 < dy)
		{
			err += dx;
			y1 += sy;
		}
	}
	while (y1 != y2)
	{
		d_x = vec_scale(env->cam.d_x, x1 - tmp_x);
		d_y = vec_scale(env->cam.d_y, tmp_y - y1);
		A = vec_mag(vec_add(d_x, d_y)) * ratio;
		cl_float3	pixel_pos = vec_add(env->cam.origin, vec_add(vec_scale(env->cam.d_x, DIM_IA - (x1 + 1)), vec_scale(env->cam.d_y, y1)));
		cl_float3	point_dir = unit_vec(vec_sub(env->cam.focus, pixel_pos));
		a = acos(dot(point_dir, ray_dir));
		if (x1 >= (DIM_IA / 2) - 1)
			b = acos(dot(env->cam.hor_ref, point_dir));
		else
			b = acos(dot(vec_scale(env->cam.hor_ref, -1), point_dir));
		B = A * sin(b) / sin(a);
		point = vec_add(p1, vec_scale(ray_dir, B));
		// point = vec_add(point, ray_step);

		cl_float3 cam_to_point = vec_sub(point, env->cam.focus);
		if (x1 >= 0 && x1 < DIM_IA && y1 >= 0 && y1 < DIM_IA)
		{
			t = vec_mag(vec_sub(point, env->cam.focus));
			if (t <= t_array[(DIM_IA - x1) + (y1 * DIM_IA)])
				env->ia->pixels[(DIM_IA - x1) + (y1 * DIM_IA)] = clr;
		}
		y1 += sy;
	}
	// print_vec(vec_sub(pix_pos, env->cam.origin));
	// print_vec(cam_perspective(env, unit_vec(vec_sub(env->cam.focus, env->cam.origin))));
	// printf("------------------------------------\n");
	// // printf("line_mag = %f\n", line_mag);
	// printf("B = %f * (sin(%f) / sin(%f))\n", A, b * 180.0f / M_PI, a * 180.0f / M_PI);
	// printf("B = %f\n", B);
	// print_vec(p2);
	// print_vec(point);
}

void	ray_visualizer(t_env *env, float *t_array)
{
	// for (int i = 0; i < env->bounce_vis; i++)
	// {
	// 	for (int y = 0; y < DIM_PT; y += env->ray_density)
	// 		for (int x = 0; x < DIM_PT; x += env->ray_density)
	// 			env->ray_display[x + (y * DIM_PT)] = 1;
	// 	if (clr.y < 1.0)
	// 		clr.y += clr_delta;
	// 	else if (clr.x > 0.0)
	// 		clr.x -= clr_delta;
	// 	else if (clr.z < 1.0)
	// 		clr.z += clr_delta;
	// 	else
	// 		clr.y -= clr_delta;
	// }
	cl_float3	clr;
	double		clr_delta = env->depth / 6;
	
	for (int i = 0; i < (DIM_PT * DIM_PT); i++)
	{
		clr = (cl_float3){1, 0, 0};
		// draw_ray(env, (cl_float3){1, -650, -750}, (cl_float3){1, -650, 0}, clr, t_array);
		for (int bounce = 0; bounce < env->ray_display[i]; bounce++)
		{
			draw_ray(env, env->rays[bounce][i].origin, env->rays[bounce + 1][i].origin, clr, t_array);
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
/*
void	draw_text(t_sdl *sdl)
{
	SDL_Renderer	*renderer = SDL_GetRenderer(sdl->win);
	// SDL_SetRenderDrawColor(renderer, 0, 0, 0, 1);

	// TTF_Font	*Sans = TTF_OpenFont("Sans.ttf", 24);
	// SDL_Color clr = {255, 0, 0};

	// SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, "CLIVE - Path Tracer", clr);

	// SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

	SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);
	// printf("%d\t%d\n", info.max_texture_width, info.max_texture_height);
	printf("%s\n", info.name);

	// SDL_Rect Message_rect;
	// Message_rect.x = 0;
	// Message_rect.y = 0;
	// Message_rect.w = 100;
	// Message_rect.h = 100;

	// SDL_RenderClear(renderer);
	// SDL_RenderCopy(renderer, Message, NULL, NULL);
	// SDL_RenderPresent(renderer);
	// SDL_Delay(5000);
}
*/
void	interactive(t_env *env)
{
	clock_t	frame_start = clock();
	set_camera(&env->cam, (float)DIM_IA);
	float	*t_array = calloc(sizeof(float), DIM_IA * DIM_IA);
	
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
			env->ia->pixels[x + (y * DIM_IA)] = ray.color;
			env->ia->pixels[(x + 1) + (y * DIM_IA)] = ray.color;
			env->ia->pixels[x + ((y + 1) * DIM_IA)] = ray.color;
			env->ia->pixels[(x + 1) + ((y + 1) * DIM_IA)] = ray.color;
			t_array[x + (y * DIM_IA)] = ray.t;
			t_array[(x + 1) + (y * DIM_IA)] = ray.t;
			t_array[x + ((y + 1) * DIM_IA)] = ray.t;
			t_array[(x + 1) + ((y + 1) * DIM_IA)] = ray.t;
		}
	}
	if (env->show_rays)
		ray_visualizer(env, t_array);
	draw_pixels(env->ia);
	// draw_text(env->ia);
	// SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing file", "File is missing. Please reinstall the program.", env->ia->win);
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
