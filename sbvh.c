#include "rt.h"

#define BIN_COUNT 8

void push(tree_box **BVH, tree_box *node)
{
	if (*BVH)
		node->next = *BVH;
	*BVH = node;
}

tree_box *pop(tree_box **BVH)
{
	tree_box *popped = NULL;
	if (*BVH)
	{
		popped = *BVH;
		*BVH = (*BVH)->next;
	}
	return popped;
}

void set_bounds(Face *faces, int count, cl_float3 *min, cl_float3 *max)
{
	float max_x = -1.0 * FLT_MAX;
	float max_y = -1.0 * FLT_MAX;
	float max_z = -1.0 * FLT_MAX;

	float min_x = FLT_MAX;
	float min_y = FLT_MAX;
	float min_z = FLT_MAX; //never forget

	for (int i = 0; i < count; i++)
	{
		for (int j = 0; j < faces[i].shape; j++)
		{
			max_x = fmax(faces[i].verts[j].x, max_x);
			max_y = fmax(faces[i].verts[j].y, max_y);
			max_z = fmax(faces[i].verts[j].z, max_z);

			min_x = fmin(faces[i].verts[j].x, min_x);
			min_y = fmin(faces[i].verts[j].y, min_y);
			min_z = fmin(faces[i].verts[j].z, min_z);
		}
	}

	*min = (cl_float3){min_x, min_y, min_z};
	*max = (cl_float3){max_x, max_y, max_z};
}

void set_bounds_box(tree_box *B)
{
	set_bounds(B->faces, B->count, &B->min, &B->max);
}

float sah(tree_box *B, int pivot)
{
	//returns the surface area heuristic for proposed split of B->faces at index [pivot]
	cl_float3 lmin, lmax, rmin, rmax;
	set_bounds(B->faces, pivot, &lmin, &lmax);
	set_bounds(&B->faces[pivot], B->count - pivot, &rmin, &rmax);

	cl_float3 lspan = vec_sub(lmax, lmin);
	cl_float3 rspan = vec_sub(rmax, rmin);
	float left_SA = 2 * lspan.x * lspan.y + 2 * lspan.y * lspan.z + 2 * lspan.x * lspan.z;
	float right_SA = 2 * rspan.x * rspan.y + 2 * rspan.y * rspan.z + 2 * rspan.x * rspan.z;

	cl_float3 Bspan = vec_sub(B->max, B->min);
	float B_SA = 2 * Bspan.x * Bspan.y + 2 * Bspan.y * Bspan.z + 2 * Bspan.x * Bspan.z;

	return left_SA * (float)pivot / B_SA + right_SA * (B->count - pivot) / B_SA;
}

int x_cmp(const void *a, const void *b)
{
	Face *fa = (Face *)a;
	Face *fb = (Face *)b;

	if (fa->center.x > fb->center.x)
		return (1);
	else if (fa->center.x < fb->center.x)
		return (-1);
	else
		return (0);
}

int y_cmp(const void *a, const void *b)
{
	Face *fa = (Face *)a;
	Face *fb = (Face *)b;

	if (fa->center.y > fb->center.y)
		return (1);
	else if (fa->center.y < fb->center.y)
		return (-1);
	else
		return (0);
}

int z_cmp(const void *a, const void *b)
{
	Face *fa = (Face *)a;
	Face *fb = (Face *)b;

	if (fa->center.z > fb->center.z)
		return (1);
	else if (fa->center.z < fb->center.z)
		return (-1);
	else
		return (0);
}

int find_pivot(tree_box *B)
{
	//find best pivot in B->faces
	float best_SAH = FLT_MAX;
	int best_ind = 0;

	int step = B->count / BIN_COUNT;
	if (step)
		for (int i = 1; i < BIN_COUNT; i++)
		{
			float SAH = sah(B, i * step);
			if (SAH < best_SAH)
			{
				best_SAH = SAH;
				best_ind = i * step;
			}
		}
	else
		for (int i = 1; i < B->count - 1; i++)
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

void split(tree_box *B)
{
	//aspects of how i'm doing this are wildly inefficient.
	//however, this way is easy to think about and maintain.
	//furthermore the construction time of the BVH is small compared to rendering,
	//and investing time in making a very good one is worth it.
	//once this has been thoroughly tested I will improve it, but it's not a priority.

	//this is an "object split" approach (ie, Faces end up in exactly one child)
	//full SBVH construction also considers "spatial split" approach (ie, face references can be duplicated for smaller SAH)
	//duplicating face references complicates things, though. I really like keeping faces in one array.
	//my current plan to work around this is to tesselate down to some average Face size so split references wouldn't be needed
	//but that obviously increases face count, possibly quite a bit in highly varied scenes.

	qsort(B->faces, B->count, sizeof(Face), x_cmp);
	int px = find_pivot(B);
	float SAHx = sah(B, px);

	qsort(B->faces, B->count, sizeof(Face), y_cmp);
	int py = find_pivot(B);
	float SAHy = sah(B, py);

	qsort(B->faces, B->count, sizeof(Face), z_cmp);
	int pz = find_pivot(B);
	float SAHz = sah(B, pz);
 
	int p;
	if (SAHx < SAHy && SAHx < SAHz)
	{
		qsort(B->faces, B->count, sizeof(Face), x_cmp);
		p = px;
		// printf("splitting along x axis, %d in left %d in right, SAH %f\n", p, B->count - p, SAHx);
	}
	else if (SAHy < SAHx && SAHy < SAHz)
	{
		qsort(B->faces, B->count, sizeof(Face), y_cmp);
		p = py;
		// printf("splitting along y axis, %d in left %d in right, SAH %f\n", p, B->count - p, SAHy);
	}
	else
	{
		p = pz;
		// printf("splitting along z axis, %d in left %d in right, SAH %f\n", p, B->count - p, SAHz);
	}

	tree_box *L = calloc(1, sizeof(tree_box));
	L->faces = B->faces;
	L->count = p;
	if (L->count)
	{
		set_bounds_box(L);
		B->left = L;
	}
	else
		free(L);

	tree_box *R = calloc(1, sizeof(tree_box));
	R->faces = &B->faces[p];
	R->count = B->count - p;
	if (R->count)
	{
		set_bounds_box(R);
		B->right = R;
	}
	else
		free(R);
}

void build_sbvh(Scene *S)
{
	//S->faces is a flat array of cpu-side faces (ie they have their VNs and VTs with them)

	//make root tree box
	//it is its own queue
	//pop queue, split, push results to queue
	//does not get pushed if 1 or 0 elements
	//stop when queue empty
	//original root is now head of whole, nice tree

	//this design should allow for the split function to be entirely modular

	tree_box *root_box = calloc(1, sizeof(tree_box));
	root_box->faces = S->faces;
	root_box->count = S->face_count;
	set_bounds_box(root_box);

	tree_box *Q = root_box;
	tree_box *B = NULL;

	int box_count = 1;

	while ((B = pop(&Q)))
	{
		split(B);
		if (B->left)
			box_count++;
		if (B->right)
			box_count++;
		if (B->left && B->left->count > 1)
			push(&Q, B->left);
		if (B->right && B->right->count > 1)
			push(&Q, B->right);
	}

	//set S->bvh
	printf("%d boxes\n", box_count);
}


/*
stray thoughts / optimization ideas:
track wich axis sorted by, use ray direction sign to go L->R or R->L
with just left/right children, can consider a simple [2n], [2n + 1] style array structure
*/