#include "rt.h"

typedef struct s_traversal
{
	cl_float3 origin;
	cl_float3 direction;
	cl_float3 inv_dir;
	float t;

	int box_comps;
	int boxes_hit;
	int inside_comps;
	int insides_hit;
	int tri_comps;
	int tris_hit;
}			Traversal;

typedef struct s_triangle
{
	cl_float3 v0;
	cl_float3 v1;
	cl_float3 v2;
}				Triangle;

static int inside_box(Traversal *ray, AABB *box)
{
	ray->inside_comps++;
	if (box->min.x <= ray->origin.x && ray->origin.x <= box->max.x)
		if (box->min.y <= ray->origin.y && ray->origin.y <= box->max.y)
			if (box->min.z <= ray->origin.z && ray->origin.z <= box->max.z)
			{
				ray->insides_hit++;
				return 1;
			}
	return 0;
}

static int intersect_box(Traversal *ray, AABB *box)
{
	ray->box_comps++;

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
	ray->boxes_hit++;
	return (1);
}

static int intersect_triangle(Traversal *ray, const cl_float3 v0, const cl_float3 v1, const cl_float3 v2)
{
	ray->tri_comps++;


	float t, u, v;
	cl_float3 e1 = vec_sub(v1, v0);
	cl_float3 e2 = vec_sub(v2, v0);

	cl_float3 h = cross(ray->direction, e2);
	float a = dot(h, e1);

	if (fabs(a) < 0)
		return 0;
	float f = 1.0f / a;
	cl_float3 s = vec_sub(ray->origin, v0);
	u = f * dot(s, h);
	if (u < 0.0 || u > 1.0)
		return 0;
	cl_float3 q = cross(s, e1);
	v = f * dot(ray->direction, q);
	if (v < 0.0 || u + v > 1.0)
		return 0;
	t = f * dot(e2, q);
	if (t < ray->t)
		ray->t = t;
	ray->tris_hit++;
	return 1;
}

void check_tris(Traversal *ray, AABB *box)
{
	for (AABB *member = box->members; member; member = member->next)
		intersect_triangle(ray, member->f->verts[0], member->f->verts[1], member->f->verts[2]);
}

static void push(AABB **stack, AABB *box)
{
	box->next = *stack;
	*stack = box;
}

static AABB *pop(AABB **stack)
{
	AABB *popped = NULL;
	if (*stack)
	{
		popped = *stack;
		*stack = popped->next;
	}
	return popped;
}

void traverse(AABB *tree, Traversal *ray)
{
	tree->next = NULL;
	AABB *stack = tree;

	float t = FLT_MAX;

	while(stack)
	{
		AABB *box = pop(&stack);
		if (intersect_box(ray, box))
		{
			if (box->left) //boxes have either 0 or 2 children
			{
				push(&stack, box->left);
				push(&stack, box->right);
			}
			else
				check_tris(ray, box);
		}
	}
}

float unit_rand()
{
	return (float)rand() / (float)RAND_MAX;
}

Traversal *random_ray(AABB *tree)
{
	Traversal *ray = calloc(1, sizeof(Traversal));
	float rx = unit_rand();
	float ry = unit_rand();
	float rz = unit_rand();

	ray->origin = (cl_float3){	tree->min.x + rx * (tree->max.x - tree->min.x), 
								tree->min.y + ry * (tree->max.y - tree->min.y),
								tree->min.z + rz * (tree->max.z - tree->min.z)};

	float theta = 2 * M_PI * unit_rand();
	float phi = acos(2 * unit_rand() - 1);

	ray->direction = (cl_float3){	sin(phi) * cos(theta), sin(phi) * sin(theta), cos(phi)};
	ray->inv_dir = (cl_float3){1.0f / ray->direction.x, 1.0f / ray->direction.y, 1.0f / ray->direction.z};

	return ray;
}

void tally(Traversal *sum, Traversal *ray)
{
	sum->box_comps += ray->box_comps;
	sum->boxes_hit += ray->boxes_hit;
	sum->inside_comps += ray->inside_comps;
	sum->insides_hit += ray->insides_hit;
	sum->tri_comps += ray->tri_comps;
	sum->tris_hit += ray->tris_hit;
	free(ray);
}

float area(AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	return span.x * span.y * span.z;
}

int depth(AABB *box)
{
	int d = 0;
	while (box)
	{
		box = box->parent;
		d++;
	}
	return d;
}

void study_tree(AABB *tree, int ray_count)
{
	printf("\n\n\nentering the lab\n");

	printf("\nmeasuring tree:\n");

	float root_area = area(tree);

	int depth_total = 0;
	int max_depth = 0;
	float area_total = 0;
	float leaf_area_subtotal = 0;
	int node_count = 0;
	int leaf_count = 0;

	AABB *queue_head = tree;
	AABB *queue_tail = tree;

	while (queue_head)
	{
		area_total += area(queue_head);
		node_count++;
		if (queue_head->left)
		{
			queue_head->left->next = queue_head->right;
			queue_tail->next = queue_head->left;
			queue_tail = queue_head->right;
			queue_tail->next = NULL;
		}
		else
		{
			leaf_count++;
			int d = depth(queue_head);
			if (d > max_depth)
				max_depth = d;
			depth_total += d;
			leaf_area_subtotal += area(queue_head);
		}
		queue_head = queue_head->next;
	}
	printf("%d leaf nodes, %d nodes total\n", leaf_count, node_count);
	printf("%.2f average depth, %d max\n", (float)depth_total / (float)leaf_count, max_depth);
	printf("total area of all boxes %.2f\n", area_total);
	printf("leaves are %.2f%% of root box area\n", 100.0f * leaf_area_subtotal / root_area);


	printf("\ntesting the tree:\n");
	Traversal *sum = calloc(1, sizeof(Traversal));

	int i;
	//#pragma omp parallel for private(i)
	for (i = 0; i < ray_count; i++)
	{
		Traversal *ray = random_ray(tree);
		traverse(tree, ray);
		//#pragma omp critical
		tally(sum, ray);
	}

	printf("%d rays complete\n", ray_count);
	printf("%.2f box comparisons per ray avg\n", (float)sum->box_comps / (float)ray_count);
	printf("%.2f triangle comparisons per ray avg\n", (float)sum->tri_comps / (float)ray_count);
}