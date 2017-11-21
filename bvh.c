#include "rt.h"

uint64_t splitBy3(const unsigned int a)
{
    uint64_t x = a & 0x1fffff; // we only look at the first 21 bits
    x = (x | x << 32) & 0x1f00000000ffff;  // shift left 32 bits, OR with self, and 00011111000000000000000000000000000000001111111111111111
    x = (x | x << 16) & 0x1f0000ff0000ff;  // shift left 32 bits, OR with self, and 00011111000000000000000011111111000000000000000011111111
    x = (x | x << 8) & 0x100f00f00f00f00f; // shift left 32 bits, OR with self, and 0001000000001111000000001111000000001111000000001111000000000000
    x = (x | x << 4) & 0x10c30c30c30c30c3; // shift left 32 bits, OR with self, and 0001000011000011000011000011000011000011000011000011000100000000
    x = (x | x << 2) & 0x1249249249249249;
    return x;
}
 
uint64_t mortonEncode_magicbits(const unsigned int x, const unsigned int y, const unsigned int z)
{
	//interweave the rightmost 21 bits of 3 unsigned ints to generate a 63-bit morton code
    uint64_t answer = 0;
    answer |= splitBy3(z) | splitBy3(y) << 1 | splitBy3(x) << 2;
    return answer;
}

uint64_t morton64(float x, float y, float z)
{
	if (max3(x,y,z) > 1 || min3(x,y,z) < 0)
		printf("scaling is bad!\n");
	//printf("x %f y %f z %f\n", x, y, z);
	//x, y, z are in [0, 1]. multiply by 2^21 to get 21 bits, confine to [0, 2^21 - 1]
    x = fmin(fmax(x * 2097152.0f, 0.0f), 2097151.0f);
    y = fmin(fmax(y * 2097152.0f, 0.0f), 2097151.0f);
    z = fmin(fmax(z * 2097152.0f, 0.0f), 2097151.0f);
    return (mortonEncode_magicbits((unsigned int)x, (unsigned int)y, (unsigned int)z));
}

uint64_t mortonize(cl_float3 pos, t_box *bounds)
{
	cl_float3 ranges = vec_sub(bounds->max, bounds->min);
    return morton64((pos.x - bounds->min.x) / (ranges.x), 
    				(pos.y - bounds->min.y) / (ranges.y), 
    				(pos.z - bounds->min.z) / (ranges.z));
}

int morton_digit(uint64_t code, int depth)
{
	code = code << (1 + depth * 3);
	code = code >> 61;
	return (int)code;
}

void print_box(const t_box *box)
{
	printf("===========\n");
	printf("Box is %p\n", box);
	if (box)
	{
		printf("min, mid, max\n");
		print_vec(box->min);
		print_vec(box->mid);
		print_vec(box->max);
		printf("children is %p\nchildren_count is %d\nobject is %p\n",
				box->children, box->children_count, box->object);
		if (box->object)
			print_object(box->object);
	}
	printf("===========\n");
}

int intersect_box(t_ray const * const ray, t_box const * const box, float *t)
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
	if (t)
	{
		if (tmin > 0.0)
			*t = tmin;
		else
			*t = tmax;
	}
	return (1);
}

float min3(float a, float b, float c)
{
	return fmin(fmin(a, b), c);
}

float max3(float a, float b, float c)
{
	return fmax(fmax(a, b), c);
}

t_box AABB_from_triangle(t_object *triangle)
{
	t_box box;
	//variable name change
	const cl_float3 v0 = triangle->position;
	const cl_float3 v1 = triangle->normal;
	const cl_float3 v2 = triangle->corner;

	box.min.x = min3(v0.x, v1.x, v2.x) - ERROR;
	box.min.y = min3(v0.y, v1.y, v2.y) - ERROR;
	box.min.z = min3(v0.z, v1.z, v2.z) - ERROR;

	box.max.x = max3(v0.x, v1.x, v2.x) + ERROR;
	box.max.y = max3(v0.y, v1.y, v2.y) + ERROR;
	box.max.z = max3(v0.z, v1.z, v2.z) + ERROR;

	box.mid = (cl_float3){	(box.max.x + box.min.x) / 2,
							(box.max.y + box.min.y) / 2,
							(box.max.z + box.min.z) / 2};

	box.object = triangle;
	box.children_count = 0;
	box.children = NULL;
	return box;
}

t_box AABB_from_sphere(t_object *sphere)
{
	t_box box;
	box.mid = sphere->position;
	//remember sphere's radius is stored in "normal" bc reasons
	box.max = (cl_float3){	sphere->position.x + sphere->normal.x,
							sphere->position.y + sphere->normal.x,
							sphere->position.z + sphere->normal.x};
	box.min = (cl_float3){	sphere->position.x - sphere->normal.x,
							sphere->position.y - sphere->normal.x,
							sphere->position.z - sphere->normal.x};
	box.object = sphere;
	box.children_count = 0;
	box.children = NULL;
	return box;					
}

t_box AABB_from_plane(t_object *plane)
{
	printf("AABB_from_plane\n");
	t_box box;
	box.max = plane->corner;
	box.mid = plane->position;
	box.min = vec_add(plane->position, vec_sub(plane->position, plane->corner));

	box.object = plane;
	box.children_count = 0;
	box.children = NULL;
	print_box(&box);
	return box;
}

t_box AABB_from_obj(t_object *object)
{
	if (object->shape == TRIANGLE)
		return AABB_from_triangle(object);
	else if (object->shape == SPHERE)
		return AABB_from_sphere(object);
	else
		return AABB_from_plane(object);
}

int box_cmp(const void *a, const void *b)
{
	t_box *box_a = (t_box *)a;
	t_box *box_b = (t_box *)b;

	return (box_a->morton - box_b->morton);
}

t_box BB_from_boxes(t_box *boxes, int count)
{
	float max_x = -1.0 * FLT_MAX;
	float max_y = -1.0 * FLT_MAX;
	float max_z = -1.0 * FLT_MAX;

	float min_x = FLT_MAX;
	float min_y = FLT_MAX;
	float min_z = FLT_MAX;

	for (int i = 0; i < count; i++)
	{
		min_x = fmin(boxes[i].min.x, min_x);
		max_x = fmax(boxes[i].max.x, max_x);

		min_y = fmin(boxes[i].min.y, min_y);
		max_y = fmax(boxes[i].max.y, max_y);

		min_z = fmin(boxes[i].min.z, min_z);
		max_z = fmax(boxes[i].max.z, max_z);
	}

	t_box box;

	box.min = (cl_float3){min_x, min_y, min_z};
	box.max = (cl_float3){max_x, max_y, max_z};
	box.mid = (cl_float3){	(box.max.x + box.min.x) / 2,
							(box.max.y + box.min.y) / 2,
							(box.max.z + box.min.z) / 2};
	box.children_count = 0;
	box.object = NULL;
	return box;
}

int g_boxcount = 0;
int g_maxdepth = 0;
void tree_down(t_box *boxes, int box_count, t_box *parent, t_box *work_array, int depth)
{
	if (depth > g_maxdepth)
		g_maxdepth = depth;
	if (depth > 30)
	{
		printf("deep cancel\n");
		parent->children_count = 0;
		return;
	}

	int counts[8];
	int indexes[8];
	bzero(counts, 8 * sizeof(int));
	bzero(indexes, 8 * sizeof(int));
	for (int i = 0; i < box_count; i++)
		counts[morton_digit(boxes[i].morton, depth)]++;
	for (int i = 1; i < 8; i++)
		indexes[i] = indexes[i - 1] + counts[i - 1];
	for (int i = 0; i < box_count; i++)
		work_array[indexes[morton_digit(boxes[i].morton, depth)]++] = boxes[i];
	for (int i = 0; i < box_count; i++)
		boxes[i] = work_array[i];
	int start = 0;
	t_box *children = malloc(8 * sizeof(t_box));
	bzero(children, sizeof(t_box) * 8);
	int child_ind = 0;
	for (int i = 0; i < 8; i++)
	{
		if (counts[i] == 0)
			continue ;
		else if (counts[i] == 1)
		{
			children[child_ind] = boxes[start];

		}
		else if (counts[i] == 2 && vec_equ(boxes[0].mid, boxes[1].mid))
		{
			//printf("dupe fudge\n");
			children[child_ind++] = boxes[start];
			children[child_ind] = boxes[start + 1];
		}
		else
		{
			children[child_ind] = BB_from_boxes(&boxes[start], counts[i]);
			tree_down(&boxes[start], counts[i], &children[child_ind], work_array, depth + 1);
		}
		g_boxcount++;
		child_ind++;
		start += counts[i];
	}
	parent->children_count = child_ind;
	parent->children = children;
}

void sort_check(t_box *boxes, int count)
{
	//NB this isn't actually the best test since the objects don't get fully sorted.
	for (int i = 0; i < count - 1; i++)
		if (boxes[i].morton > boxes[i + 1].morton)
		{
			printf("FAIL at %d, %d\n", i, i+1);
			printf("%llu %llu\n", boxes[i].morton , boxes[i + 1].morton);
		}
}

void analyze_bvh(t_box *root, int depth)
{
	printf("===========\n");
	printf("box at depth %d\n", depth);
	print_vec(root->min);
	print_vec(root->max);
	printf("area of box is %f\n", vec_mag(vec_sub(root->max, root->min)));
	if (root->children_count)
		printf("%d children\n", root->children_count);
	else
		printf("leaf node, %p\n", root->object);
	for (int i = 0; i < root->children_count; i++)
		analyze_bvh(&root->children[i], depth + 1);
}

void make_bvh(t_scene *scene)
{
	int count = 0;
	t_object *obj = scene->objects;
	while(obj && ++count)
		obj = obj->next;
	t_box *boxes = malloc(count * sizeof(t_box));
	obj = scene->objects;
	for (int i = 0; i < count; i++, obj = obj->next)
		boxes[i] = AABB_from_obj(obj);
	t_box *root = malloc(sizeof(t_box));
	*root = BB_from_boxes(boxes, count);
	for (int i = 0; i < count; i++)
		boxes[i].morton = mortonize(boxes[i].mid, root);
	t_box *work_array = malloc(count * sizeof(t_box));
	tree_down(boxes, count, root, work_array, 0);

	scene->bvh = root;
	printf("max depth %d boxcount %d\n", g_maxdepth, g_boxcount);
	//print_box(root);
	free(boxes);
	free(work_array);
}

void hit_nearest(const t_ray *ray, t_box const * const box, t_object **hit, float *d)
{
	float this_d = 0.0;
	if (box->object)
	{
		if (intersect_object(ray, box->object, &this_d))
			if (this_d < *d)
			{
				*d = this_d;
				*hit = box->object;
			}
	}
	else if (intersect_box(ray, box, NULL))
	{
		for (int i = 0; i < box->children_count; i++)
			hit_nearest(ray, &box->children[i], hit, d);
	}
}

void hit_nearest_faster(const t_ray * const ray, t_box const * const box, t_object **hit, float *d)
{
	float this_d = 0.0;
	if (box->object)
	{
		if (intersect_object(ray, box->object, &this_d))
			if (this_d < *d)
			{
				*d = this_d;
				*hit = box->object;
			}
	}
	else if (intersect_box(ray, box, &this_d))
	{
		if (!*hit || this_d < *d)
			for (int i = 0; i < box->children_count; i++)
				hit_nearest(ray, &box->children[i], hit, d);
	}
}