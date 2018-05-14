#include "rt.h"

#define NORMAL_SHIFT 0.0003f
#define SUN_BRIGHTNESS 1000.0f
#define stop_prob 0.3f

typedef struct	s_ray
{
	cl_float3	origin;
	cl_float3	direction;
	cl_float3	inv_dir;

	cl_float3	color;
	cl_float3	mask;

	//surface data
	Face		*f;
	cl_float3	N;
	cl_float3	sample_N;
	float		u;
	float		v;
	float		t;
	cl_float3	bump;
	cl_float3	diff;
	cl_float3	spec;
	cl_float3	trans;

	int			bounce;
	int 		subsample_count;
	_Bool		status;
	struct s_ray	*next;
}				t_ray;

void	init_ray(t_ray *ray)
{
	ray->color = BLACK;
	ray->mask = WHITE;

	ray->f = NULL;
	ray->N = ray->direction; //???
	ray->sample_N = ORIGIN;
	ray->u = 0;
	ray->v = 0;
	ray->t = FLT_MAX;
	ray->bump = BLACK;
	ray->diff = BLACK;
	ray->spec = BLACK;
	ray->trans = BLACK;

	ray->bounce = 0;
	ray->status = 1;
	ray->subsample_count = 1; //avoid nans this way

	ray->next = NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int inside_box(t_ray *ray, AABB *box)
{
	if (box->min.x <= ray->origin.x && ray->origin.x <= box->max.x)
		if (box->min.y <= ray->origin.y && ray->origin.y <= box->max.y)
			if (box->min.z <= ray->origin.z && ray->origin.z <= box->max.z)
				return 1;
	return 0;
}

static int intersect_boxes(t_ray *ray, AABB *box, float *t_out)
{
	float tx0 = (box->min.x - ray->origin.x) * ray->inv_dir.x;
	float tx1 = (box->max.x - ray->origin.x) * ray->inv_dir.x;
	float tmax = fmax(tx0, tx1);
	float tmin = fmin(tx0, tx1);

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

void	intersect_tri(t_ray *ray, Face *face)
{
	const cl_float3 v0 = face->verts[0], v1 = face->verts[1], v2 = face->verts[2];
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
		ray->t = t;
		ray->N = unit_vec(cross(vec_sub(v1, v0), vec_sub(v2, v0)));
		ray->f = face;
		ray->u = u;
		ray->v = v;
	}
}

void check_tri(t_ray *ray, AABB *box)
{
	for (AABB *member = box->members; member; member = member->next)
		intersect_tri(ray, member->f);
}

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

void	cam_traverse(t_env *env, t_ray *ray)
{
	if (!ray->status)
		return ;
	AABB *stack = env->scene->bins;
	stack->next = NULL;

	while(stack)
	{
		AABB *box = stack_pop(&stack);
		if (intersect_boxes(ray, box, NULL))
		{
			if (box->left) //boxes have either 0 or 2 children
			{
				float tleft, tright;
				int lret = intersect_boxes(ray, box->left, &tleft);
				int rret = intersect_boxes(ray, box->right, &tright);
				if (lret && tleft >= tright)
                    push(&stack, box->left);
                if (rret)
                    push(&stack, box->right);
                if (lret && tleft < tright)
                    push(&stack, box->left);
			}
			else
			{
				check_tri(ray, box);
			}
		}
	}
}

cl_float3 cos_weight_dir(cl_float3 N)
{
	float	r1 = (float)rand() / (float)RAND_MAX;
	float	r2 = (float)rand() / (float)RAND_MAX;

	//local orthonormal system
	cl_float3 axis = fabs(N.x) > fabs(N.y) ? (cl_float3){0.0f, 1.0f, 0.0f} : (cl_float3){1.0f, 0.0f, 0.0f};
	cl_float3 hem_x = cross(axis, N);
	cl_float3 hem_y = cross(N, hem_x);

	//generate random direction on the unit hemisphere (cosine-weighted)
	float r = sqrt(r1);
	float theta = 2.0f * M_PI * r2;

	//combine for new direction
	cl_float3	tmp1 = vec_scale(vec_scale(hem_x, r), cos(theta));
	cl_float3	tmp2 = vec_scale(vec_scale(hem_y, r), sin(theta));
	cl_float3	tmp3 = vec_scale(N, sqrt(fmax(0.0f, 1.0f - r1)));
	cl_float3	o = unit_vec(vec_add(vec_add(tmp1, tmp2), tmp3));
	return o;
}

cl_float3 uniform_dir(cl_float3 N)
{
	float	r1 = (float)rand() / (float)RAND_MAX;
	float	r2 = (float)rand() / (float)RAND_MAX;

	//local orthonormal system
	cl_float3 axis = fabs(N.x) > fabs(N.y) ? (cl_float3){0.0f, 1.0f, 0.0f} : (cl_float3){1.0f, 0.0f, 0.0f};
	cl_float3 hem_x = cross(axis, N);
	cl_float3 hem_y = cross(N, hem_x);

	//generate random direction on the unit hemisphere (cosine-weighted)
	float r = sqrt(1.0f - r1 * r1);
	float theta = 2.0f * M_PI * r2;

	//combine for new direction
	cl_float3	tmp1 = vec_scale(vec_scale(hem_x, r), cos(theta));
	cl_float3	tmp2 = vec_scale(vec_scale(hem_y, r), sin(theta));
	cl_float3	tmp3 = vec_scale(N, r1);
	cl_float3	o = unit_vec(vec_add(vec_add(tmp1, tmp2), tmp3));
	return o;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static cl_float3 fetch_tex(Map *map, const cl_float3 txcrd, const cl_float3 flat)
{
	if (!map || map->width == 0 || map->height == 0)
		return flat;
	int x = floor((float)map->width * txcrd.x);
	int y = floor((float)map->height * txcrd.y);

	cl_uchar R = map->pixels[(y * map->width + x) * 3];
	cl_uchar G = map->pixels[(y * map->width + x) * 3 + 1];
	cl_uchar B = map->pixels[(y * map->width + x) * 3 + 2];
	cl_float3 out = vec_scale((cl_float3){(float)R, (float)G, (float)B}, 1.0f / 255.0f);
	return out;
}

void	fetch(t_env *env, t_ray *ray)
{
	if (!ray->f)
	{
		// ray->color = vec_scale(ray->mask, SUN_BRIGHTNESS);
		// fabs(pow(dot(ray->direction, UNIT_Y), 1.0f)
		ray->color = BLACK;
		ray->status = 0;
		return ;
	}

	cl_float3	tmp1, tmp2, tmp3;

	cl_float3	v0 = ray->f->verts[0];
	cl_float3	v1 = ray->f->verts[1];
	cl_float3	v2 = ray->f->verts[2];
	cl_float3	geom_N = unit_vec(cross(vec_sub(v1, v0), vec_sub(v2, v0)));

	tmp1 = vec_scale(ray->f->norms[0], (1.0f - ray->u - ray->v));
	tmp2 = vec_scale(ray->f->norms[1], ray->u);
	tmp3 = vec_scale(ray->f->norms[2], ray->v);
	ray->sample_N = unit_vec(vec_add(vec_add(tmp1, tmp2), tmp3));

	ray->sample_N = dot(ray->sample_N, geom_N) > 0.0f ? ray->sample_N : vec_scale(ray->sample_N, -1.0f);

	tmp1 = vec_scale(ray->f->tex[0], (1.0f - ray->u - ray->v));
	tmp2 = vec_scale(ray->f->tex[1], ray->u);
	tmp3 = vec_scale(ray->f->tex[2], ray->v);
	cl_float3	txcrd = vec_add(vec_add(tmp1, tmp2), tmp3);
	txcrd.x -= floor(txcrd.x);
	txcrd.y -= floor(txcrd.y);
	if (txcrd.x < 0.0f)
		txcrd.x = 1.0f + txcrd.x;
	if (txcrd.y < 0.0f)
		txcrd.y = 1.0f + txcrd.y;

	Material mat = env->scene->materials[ray->f->mat_ind];
	if (mat.Ke.x > 0)
	{
		// ray->color = vec_scale(ray->mask, SUN_BRIGHTNESS);
		ray->status = 0;
	}
	else
	{
		// printf("%f\t%f\n", txcrd.x, txcrd.y);
		ray->diff = fetch_tex(mat.map_Kd, txcrd, mat.Kd);
		ray->spec = fetch_tex(mat.map_Ks, txcrd, mat.Ks);
		ray->trans = fetch_tex(mat.map_d, txcrd, mat.Kd);
		ray->bump = fetch_tex(mat.map_bump, txcrd, mat.Kd);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

_Bool	check_visibility(t_env *env, t_ray *ray, t_ray *light_path_vertex)
{
	cl_float3	dir = vec_sub(light_path_vertex->origin, ray->origin);
	float		t = vec_mag(dir);

	t_ray		edge;
	edge.origin = ray->origin;
	edge.direction = unit_vec(dir);
	edge.inv_dir = vec_inverse(edge.direction);
	init_ray(&edge);
	edge.t = t;

	AABB *stack = env->scene->bins;
	stack->next = NULL;
	while (stack)
	{
		AABB *box = stack_pop(&stack);
		if (intersect_boxes(&edge, box, NULL))
		{
			if (box->left) //boxes have either 0 or 2 children
			{
				float tleft, tright;
				int lret = intersect_boxes(ray, box->left, &tleft);
				int rret = intersect_boxes(ray, box->right, &tright);
				if (lret && tleft >= tright)
                    push(&stack, box->left);
                if (rret)
                    push(&stack, box->right);
                if (lret && tleft < tright)
                    push(&stack, box->left);
			}
			else
			{
				check_tri(ray, box);
				if (edge.t < t)
					return 0;
			}
		}
	}
	return 1;
}

void	multi_sample_estimator(t_ray *ray, t_ray *light_path_vertex)
{
	cl_float3 cam_to_light = vec_sub(light_path_vertex->origin, ray->origin);
	cam_to_light = unit_vec(cam_to_light);

	float geom_term = fmax(dot(cam_to_light, ray->N), 0.0f) * fmax(dot(vec_scale(cam_to_light, -1.0f), light_path_vertex->N), 0.0f);

	ray->color = vec_add(ray->color, vec_scale(vec_mult(ray->mask, light_path_vertex->mask), geom_term));
}

void	sample_light_path(t_env *env, t_ray *ray, t_ray *light_path)
{
	t_ray	*light_path_vertex = light_path;
	while (light_path_vertex)
	{
		//ray->subsample_count++;
		if (check_visibility(env, ray, light_path_vertex))
			multi_sample_estimator(ray, light_path_vertex);
		light_path_vertex = light_path_vertex->next;
	}
}

void	bounce(t_env *env, t_ray *ray, t_ray *light_path)
{	
	ray->bounce++;

	if (ray->bounce >= env->depth && ((float)rand() / (float)RAND_MAX) > stop_prob)
	{
		ray->status = 0;
		return ;
	}
	if (!ray->status)
		return ;

	cl_float3	o = cos_weight_dir(ray->N);
	float weight = 1.0f; //dot(ray->N, o);

	float o_sign = dot(ray->N, o) > 0.0f ? 1.0f : -1.0f;
	ray->mask = vec_mult(ray->mask, vec_scale(ray->diff, weight));
	ray->origin = vec_add(vec_add(ray->origin, vec_scale(ray->direction, ray->t)), vec_scale(ray->N, o_sign * NORMAL_SHIFT));
	ray->direction = o;
	ray->inv_dir = vec_inverse(o);
	if (dot(ray->mask, ray->mask) == 0.0f)
		ray->status = 0;
	sample_light_path(env, ray, light_path);

	ray->f = NULL;
	ray->t = FLT_MAX;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void	light_bounce(t_env *env, t_ray *ray_in)
{	
	if (ray_in->bounce >= env->depth && ((float)rand() / (float)RAND_MAX) > stop_prob)
	{
		ray_in->status = 0;
		ray_in->next = NULL;
		return ;
	}
	if (!ray_in->status)
		return ;

	t_ray	*ray_out = malloc(sizeof(t_ray));
	init_ray(ray_out);

	ray_out->bounce = ray_in->bounce + 1;

	cl_float3	o = uniform_dir(ray_in->N);
	float weight = fmax(0.0f, dot(ray_in->N, ray_in->direction));

	float o_sign = dot(ray_in->N, o) > 0.0f ? 1.0f : -1.0f;
	ray_out->mask = vec_mult(ray_in->mask, vec_scale(ray_in->diff, weight));
	ray_out->origin = vec_add(vec_add(ray_in->origin, vec_scale(ray_in->direction, ray_in->t)), vec_scale(ray_in->N, o_sign * NORMAL_SHIFT));
	ray_out->direction = o;
	ray_out->inv_dir = vec_inverse(o);
	if (dot(ray_out->mask, ray_out->mask) == 0.0f)
		ray_out->status = 0;

	ray_out->f = NULL;
	ray_out->t = FLT_MAX;
	ray_in->next = ray_out;
}

t_ray	*light_path(t_env *env)
{
	Scene	*scene = env->scene;

	float	total_light_area = scene->light_area[scene->light_count];
	float	light_area_interval = ((float)rand() / (float)RAND_MAX) * total_light_area;

	int light_i = 0;
	while (light_i < scene->light_count)
	{
		if (light_area_interval <= scene->light_area[light_i])
			break ;
		light_i++;
	}

	Face	light_source = scene->lights[light_i];
	cl_float3	v0 = light_source.verts[0];
	cl_float3	v1 = light_source.verts[1];
	cl_float3	v2 = light_source.verts[2];
	cl_float3	n = unit_vec(light_source.norms[0]);

	float	r1, r2, r3;
	r1 = (float)rand() / (float)RAND_MAX;
	r2 = (float)rand() / (float)RAND_MAX;

	cl_float3	p = vec_add(vec_add(vec_scale(v0, (1.0f - sqrt(r1))), vec_scale(v1, (sqrt(r1) * (1.0f - r2)))), vec_scale(v2, (r2 * sqrt(r1))));
	// print_vec(p);

	cl_float3	dir = cos_weight_dir(n);
	dir = (cl_float3){0.0, -1.0, 0.0};

	t_ray	*light_path = malloc(sizeof(t_ray));
	t_ray	*light_path_vertex = light_path;
	light_path_vertex->origin = p;
	light_path_vertex->direction = dir;
	init_ray(light_path_vertex);
	//light_path_vertex->N = n;
	cl_float3 Ke = scene->materials[light_source.mat_ind].Ke;
	light_path_vertex->mask = vec_mult(Ke, vec_scale(WHITE, SUN_BRIGHTNESS));
	while (light_path_vertex && light_path_vertex->status)
	{
		cam_traverse(env, light_path_vertex);
		fetch(env, light_path_vertex);
		light_bounce(env, light_path_vertex);
		light_path_vertex = light_path_vertex->next;
	}
	return light_path;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

t_ray	generate_rays(t_env *env, float x, float y)
{
	t_ray	ray;

	ray.origin = env->cam.focus;
	x += (float)rand() / (float)RAND_MAX;
	y += (float)rand() / (float)RAND_MAX;
	cl_float3 through = vec_add(env->cam.origin, vec_add(vec_scale(env->cam.d_x, x), vec_scale(env->cam.d_y, y)));
	ray.direction = unit_vec(vec_sub(env->cam.focus, through));
	ray.inv_dir.x = 1.0f / ray.direction.x;
	ray.inv_dir.y = 1.0f / ray.direction.y;
	ray.inv_dir.z = 1.0f / ray.direction.z;
	init_ray(&ray);
	return ray;
}

cl_float3	*bidirectional(t_env *env)
{
	set_camera(&env->cam, (float)DIM_PT);
	cl_float3	*out = calloc(sizeof(cl_float3), (DIM_PT * DIM_PT));

	for (int y = 0; y < DIM_PT; y++)
	{
		for (int x = 0; x < DIM_PT; x++)
		{
			t_ray ray = generate_rays(env, x, y);
			t_ray *light_ray = light_path(env);
			while (ray.status)
			{
				cam_traverse(env, &ray);
				fetch(env, &ray);
				bounce(env, &ray, light_ray);
			}
			out[x + (y * DIM_PT)] = vec_scale(ray.color, 1.0f / (float)ray.subsample_count);
		}
	}
	return out;
}