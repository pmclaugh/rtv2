// typedef struct meta_bvh_struct
// {
// 	cl_float3 min;
// 	cl_float3 max;
// 	struct meta_bvh_struct *left;
// 	struct meta_bvh_struct *right;
// 	tree_box *boxes;
// 	int count;
// 	struct meta_bvh_struct *next;
// }				meta_box;

#include "rt.h"

#define LEAF_THRESHOLD 1

static void set_bounds(tree_box *boxes, int count, cl_float3 *min, cl_float3 *max)
{
	float max_x = -1.0 * FLT_MAX;
	float max_y = -1.0 * FLT_MAX;
	float max_z = -1.0 * FLT_MAX;

	float min_x = FLT_MAX;
	float min_y = FLT_MAX;
	float min_z = FLT_MAX; //never forget

	for (int i = 0; i < count; i++)
	{
		max_x = fmax(boxes[i].max.x, max_x);
		max_y = fmax(boxes[i].max.y, max_y);
		max_z = fmax(boxes[i].max.z, max_z);

		min_x = fmin(boxes[i].min.x, min_x);
		min_y = fmin(boxes[i].min.y, min_y);
		min_z = fmin(boxes[i].min.z, min_z);
	}

	*min = (cl_float3){min_x - ERROR, min_y - ERROR, min_z - ERROR};
	*max = (cl_float3){max_x + ERROR, max_y + ERROR, max_z + ERROR};
}

static cl_float3 center(tree_box *B)
{
	cl_float3 c = vec_add(B->min, B->max);
	c = vec_scale(c, 0.5f);
	return c;
}

static int x_cmp(const void *a, const void *b)
{
	tree_box *ta = (tree_box *)a;
	tree_box *tb = (tree_box *)b;
	cl_float3 ca = center(ta);
	cl_float3 cb = center(tb);

	if (ca.x > cb.x)
		return (1);
	else if (ca.x < cb.x)
		return (-1);
	else
		return (0);
}

static int y_cmp(const void *a, const void *b)
{
	tree_box *ta = (tree_box *)a;
	tree_box *tb = (tree_box *)b;
	cl_float3 ca = center(ta);
	cl_float3 cb = center(tb);

	if (ca.y > cb.y)
		return (1);
	else if (ca.y < cb.y)
		return (-1);
	else
		return (0);
}

static int z_cmp(const void *a, const void *b)
{
	tree_box *ta = (tree_box *)a;
	tree_box *tb = (tree_box *)b;
	cl_float3 ca = center(ta);
	cl_float3 cb = center(tb);

	if (ca.z > cb.z)
		return (1);
	else if (ca.z < cb.z)
		return (-1);
	else
		return (0);
}

static float sah(meta_box *B, int pivot)
{
	//returns the surface area heuristic for proposed split of B->faces at index [pivot]
	cl_float3 lmin, lmax, rmin, rmax;
	set_bounds(B->boxes, pivot, &lmin, &lmax);
	set_bounds(&B->boxes[pivot], B->count - pivot, &rmin, &rmax);

	cl_float3 lspan = vec_sub(lmax, lmin);
	cl_float3 rspan = vec_sub(rmax, rmin);
	float left_SA = 2 * lspan.x * lspan.y + 2 * lspan.y * lspan.z + 2 * lspan.x * lspan.z;
	float right_SA = 2 * rspan.x * rspan.y + 2 * rspan.y * rspan.z + 2 * rspan.x * rspan.z;

	cl_float3 Bspan = vec_sub(B->max, B->min);
	float B_SA = 2 * Bspan.x * Bspan.y + 2 * Bspan.y * Bspan.z + 2 * Bspan.x * Bspan.z;

	return left_SA * (float)pivot / B_SA + right_SA * (B->count - pivot) / B_SA;
}

static int find_pivot(meta_box *B)
{
	//find best pivot in B->faces
	float best_SAH = FLT_MAX;
	int best_ind = 0;

	for (int i = 0; i < B->count; i++)
	{
		float SAH = sah(B, i);
		if (SAH < best_SAH)
		{
			best_SAH = SAH;
			best_ind = i;
		}
	}
	return best_ind;
}

static void push(meta_box **BVH, meta_box *node)
{
	if (*BVH)
		node->next = *BVH;
	*BVH = node;
}

static meta_box *pop(meta_box **BVH)
{
	meta_box *popped = NULL;
	if (*BVH)
	{
		popped = *BVH;
		*BVH = (*BVH)->next;
	}
	return popped;
}

static void split(meta_box *B)
{
	qsort(B->boxes, B->count, sizeof(tree_box), x_cmp);
	int px = find_pivot(B);
	float SAHx = sah(B, px);

	qsort(B->boxes, B->count, sizeof(tree_box), y_cmp);
	int py = find_pivot(B);
	float SAHy = sah(B, py);

	qsort(B->boxes, B->count, sizeof(tree_box), z_cmp);
	int pz = find_pivot(B);
	float SAHz = sah(B, pz);
 
	int p;
	if (SAHx <= SAHy && SAHx <= SAHz)
	{
		qsort(B->boxes, B->count, sizeof(tree_box), x_cmp);
		p = px;
		 // printf("splitting along x axis, %d in left %d in right, SAH %f\n", p, B->count - p, SAHx);
	}
	else if (SAHy <= SAHx && SAHy <= SAHz)
	{
		qsort(B->boxes, B->count, sizeof(tree_box), y_cmp);
		p = py;
		 // printf("splitting along y axis, %d in left %d in right, SAH %f\n", p, B->count - p, SAHy);
	}
	else
	{
		p = pz;
		 // printf("splitting along z axis, %d in left %d in right, SAH %f\n", p, B->count - p, SAHz);
	}

	meta_box *L = calloc(1, sizeof(meta_box));
	L->boxes = B->boxes;
	L->count = p;
	if (L->count)
	{
		set_bounds(L->boxes, L->count, &L->min, &L->max);
		B->left = L;
	}
	else
		free(L);

	meta_box *R = calloc(1, sizeof(meta_box));
	R->boxes = &B->boxes[p];
	R->count = B->count - p;
	if (R->count)
	{
		set_bounds(R->boxes, R->count, &R->min, &R->max);
		B->right = R;
	}
	else
		free(R);
}

meta_box *meta_bvh(tree_box *objects, int count, int *box_count)
{
	//objects array needs to be flattened before this
	meta_box *root = calloc(1, sizeof(meta_box));
	root->boxes = objects;
	root->count = count;
	set_bounds(objects, count, &root->min, &root->max);

	meta_box *Q = root;
	meta_box *B = NULL;

	int bcount = 1;

	while ((B = pop(&Q)))
	{
		split(B);
		if (B->left)
			bcount++;
		if (B->right)
			bcount++;
		if (B->left && B->left->count > LEAF_THRESHOLD)
			push(&Q, B->left);
		if (B->right && B->right->count > LEAF_THRESHOLD)
			push(&Q, B->right);
	}

	printf("%d boxes\n", bcount);
	*box_count = bcount;

	return root;
}