#include "rt.h"

#define NORMAL_SHIFT 0.0003f
#define SUN_BRIGHTNESS 60000.0f

typedef struct	s_ray
{
	cl_float3	origin;
	cl_float3	direction;
	cl_float3	inv_dir;

	cl_float3	color;
	cl_float3	mask;

	cl_float3	bump;
	cl_float3	diff;
	cl_float3	spec;
	cl_float3	trans;

	Face		*f;
	float		u;
	float		v;
	cl_float3	sample_N;

	cl_float3	N;
	float		t;
	int			bounce;
	_Bool		status;
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static cl_float3 fetch_tex(Map *map, const cl_float3 txcrd)
{
	if (!map || map->width == 0 || map->height == 0)
		return BLACK;
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
		ray->color = vec_scale(ray->mask, SUN_BRIGHTNESS);
		// fabs(pow(dot(ray->direction, UNIT_Y), 1.0f)
		ray->status = 0;
		return ;
	}

	cl_float3	tmp1, tmp2, tmp3;

	// cl_float3	v0 = ray->f->verts[0];
	// cl_float3	v1 = ray->f->verts[1];
	// cl_float3	v2 = ray->f->verts[2];
	// cl_float3	geom_N = unit_vec(cross(vec_sub(v1, v0), vec_sub(v2, v0)));

	tmp1 = vec_scale(ray->f->norms[0], (1.0f - ray->u - ray->v));
	tmp2 = vec_scale(ray->f->norms[1], ray->u);
	tmp3 = vec_scale(ray->f->norms[2], ray->v);
	ray->sample_N = unit_vec(vec_add(vec_add(tmp1, tmp2), tmp3));

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
	// printf("%f\t%f\n", txcrd.x, txcrd.y);
	ray->diff = fetch_tex(mat.map_Kd, txcrd);
	ray->spec = fetch_tex(mat.map_Ks, txcrd);
	ray->trans = fetch_tex(mat.map_d, txcrd);
	ray->bump = fetch_tex(mat.map_bump, txcrd);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void	bounce(t_env *env, t_ray *ray)
{	
	ray->bounce++;

	if (!ray->status || ray->bounce >= env->depth)
	{
		ray->status = 0;
		return ;
	}

	float	r1 = (float)rand() / (float)RAND_MAX;
	float	r2 = (float)rand() / (float)RAND_MAX;

	// float3 weight = WHITE;
	//Cosine-weighted pure diffuse reflection
	//local orthonormal system
	cl_float3 axis = fabs(ray->N.x) > fabs(ray->N.y) ? (cl_float3){0.0f, 1.0f, 0.0f} : (cl_float3){1.0f, 0.0f, 0.0f};
	cl_float3 hem_x = cross(axis, ray->N);
	cl_float3 hem_y = cross(ray->N, hem_x);

	//generate random direction on the unit hemisphere (cosine-weighted)
	float r = sqrt(r1);
	float theta = 2.0f * M_PI * r2;

	//combine for new direction
	cl_float3	tmp1 = vec_scale(vec_scale(hem_x, r), cos(theta));
	cl_float3	tmp2 = vec_scale(vec_scale(hem_y, r), sin(theta));
	cl_float3	tmp3 = vec_scale(ray->N, sqrt(fmax(0.0f, 1.0f - r1)));
	cl_float3	o = unit_vec(vec_add(vec_add(tmp1, tmp2), tmp3));
	float weight = acos(dot(ray->N, o));

	float o_sign = dot(ray->N, o) > 0.0f ? 1.0f : -1.0f;
	ray->mask = vec_mult(ray->mask, vec_scale(ray->diff, weight));
	ray->origin = vec_add(vec_add(ray->origin, vec_scale(ray->direction, ray->t)), vec_scale(ray->N, o_sign * NORMAL_SHIFT));
	ray->direction = o;
	ray->inv_dir = vec_inverse(o);
	if (dot(ray->mask, ray->mask) == 0.0f)
		ray->status = 0;

	ray->bounce++;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

t_ray	generate_rays(t_env *env, float x, float y)
{
	t_ray ray;
	ray.origin = env->cam.focus;
	x += rand() / RAND_MAX;
	y += rand() / RAND_MAX;
	// printf("%f\t%f\n", x, y);
	cl_float3 through = vec_add(env->cam.origin, vec_add(vec_scale(env->cam.d_x, x), vec_scale(env->cam.d_y, y)));
	ray.direction = unit_vec(vec_sub(env->cam.focus, through));
	ray.inv_dir.x = 1.0f / ray.direction.x;
	ray.inv_dir.y = 1.0f / ray.direction.y;
	ray.inv_dir.z = 1.0f / ray.direction.z;
	ray.N = ray.direction;
	ray.t = FLT_MAX;
	
	ray.mask = WHITE;
	ray.color = BLACK;

	ray.bump = BLACK;
	ray.diff = BLACK;
	ray.spec = BLACK;
	ray.trans = BLACK;

	ray.bounce = 0;
	ray.status = 1;

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
			while (ray.status)
			{
				ray.t = FLT_MAX;
				ray.f = NULL;
				cam_traverse(env, &ray);
				fetch(env, &ray);
				bounce(env, &ray);
			}
			out[x + (y * DIM_PT)] = ray.color;
		}
	}
	return out;
}