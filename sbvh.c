#include "rt.h"

#define SPLIT_COUNT 15
#define LEAF_THRESHOLD 4

#define MAX_START (cl_float3){-1.0f * FLT_MAX, -1.0f * FLT_MAX, -1.0f * FLT_MAX}
#define MIN_START (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX}

static void push(tree_box **BVH, tree_box *node)
{
	if (*BVH)
		node->next = *BVH;
	*BVH = node;
}

static tree_box *pop(tree_box **BVH)
{
	tree_box *popped = NULL;
	if (*BVH)
	{
		popped = *BVH;
		*BVH = (*BVH)->next;
	}
	return popped;
}

static void set_bounds(Face *faces, int count, cl_float3 *min, cl_float3 *max)
{
	//convert this to use the new defines at some point

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

	*min = (cl_float3){min_x - ERROR, min_y - ERROR, min_z - ERROR};
	*max = (cl_float3){max_x + ERROR, max_y + ERROR, max_z + ERROR};
}

static void set_bounds_box(tree_box *B)
{
	set_bounds(B->faces, B->count, &B->min, &B->max);
}

typedef struct s_bin {
	cl_float3 min;
	cl_float3 max;
}				Bin;

static float SA(Bin b)
{
	cl_float3 span = vec_sub(b.max, b.min);
	return 2 * span.x * span.y + 2 * span.y * span.z + 2 * span.x * span.z;
}

static float sah(tree_box *B, int pivot)
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

int in_bounds(Face f, Bin b)
{
	if (b.min.x <= f.center.x && f.center.x <= b.max.x)
		if (b.min.y <= f.center.y && f.center.y <= b.max.y)
			if (b.min.z <= f.center.z && f.center.z <= b.max.z)
				return (1);
	return (0);
}

void adjust_bounds(Face f, Bin *b)
{
	for (int i = 0; i < f.shape; i++)
	{
		b->min.x = fmin(b->min.x, f.verts[i].x);
		b->max.x = fmax(b->max.x, f.verts[i].x);
		b->min.y = fmin(b->min.y, f.verts[i].y);
		b->max.y = fmax(b->max.y, f.verts[i].y);
		b->min.z = fmin(b->min.z, f.verts[i].z);
		b->max.z = fmax(b->max.z, f.verts[i].z);
	}
}
void print_bin(Bin b)
{
	print_vec(b.min);
	print_vec(b.max);
	printf("\n");
}

static float SAH(Bin a, Bin b, float pivot, float total, float parent_SA)
{
	//printf("SA(a) %f SA(b) %f, parent_SA %f\n", SA(a), SA(b), parent_SA);
	return SA(a) * pivot / parent_SA + SA(b) * (total - pivot) / parent_SA;
}

void center_span(tree_box *B, cl_float3 *out_min, cl_float3 *out_max)
{
	cl_float3 min = MIN_START;
	cl_float3 max = MAX_START;

	for (int i = 0; i < B->count; i++)
	{
		min.x = fmin(min.x, B->faces[i].center.x);
		max.x = fmax(max.x, B->faces[i].center.x);
		min.y = fmin(min.y, B->faces[i].center.y);
		max.y = fmax(max.y, B->faces[i].center.y);
		min.z = fmin(min.z, B->faces[i].center.z);
		max.z = fmax(max.z, B->faces[i].center.z);
	}
	*out_min = min;
	*out_max = max;
}

static Bin best_split(tree_box *B, int *count)
{
	//scan array for bounding box (n)
	cl_float3 min, max;
	center_span(B, &min, &max);
	cl_float3 span = vec_sub(max, min);
	//print_vec(span);
	//printf("%d\n", B->count);
	Bin *bins = calloc(3 * SPLIT_COUNT, sizeof(Bin));
	for (int i = 0; i < SPLIT_COUNT; i++)
	{
		bins[i] = (Bin){min, (cl_float3){min.x + (float)(i + 1) * span.x / (float)(SPLIT_COUNT + 1), max.y, max.z}};
		bins[i + SPLIT_COUNT] = (Bin){min, (cl_float3){max.x, min.y + (float)(i + 1) * span.y / (float)(SPLIT_COUNT + 1), max.z}};
		bins[i + 2 * SPLIT_COUNT] = (Bin){min, (cl_float3){max.x, max.y, min.z + (float)(i + 1) * span.z / (float)(SPLIT_COUNT + 1)}};
	}

	Bin boxes[2 * 3 * SPLIT_COUNT];
	int counts[3 * SPLIT_COUNT];
	bzero(counts, sizeof(int) * 3 * SPLIT_COUNT);
	for (int i = 0; i < 2 * 3 * SPLIT_COUNT; i++)
		boxes[i] = (Bin){MIN_START, MAX_START};
	
	for (int i = 0; i < B->count; i++)
		for (int j = 0; j < (3 * SPLIT_COUNT); j++)
			if (in_bounds(B->faces[i], bins[j]))
			{
				counts[j]++;
				adjust_bounds(B->faces[i], &boxes[2 * j]);
			}
			else
				adjust_bounds(B->faces[i], &boxes[2 * j + 1]);
	
	//which pair is best split?
	float parent_SA = SA((Bin){min, max});
	float min_SA = FLT_MAX;
	int min_ind = -1;
	for (int i = 0; i < 3 * SPLIT_COUNT; i++)
	{
		if (counts[i] == 0 || counts[i] == B->count)
			continue;
		float score = SAH(boxes[2 * i], boxes[2 * i + 1], (float)counts[i], (float)B->count, parent_SA);
		//printf("%f\n", score);
		if (score < min_SA)
		{
			min_SA = score;
			min_ind = i;
		}
	}
	//printf("picked split %d\n", min_ind);
	*count = counts[min_ind];
	return bins[min_ind];
}

static void bin_sort(tree_box *B, Bin split, int count)
{
	Face *scratch = calloc(B->count, sizeof(Face));
	int in_index = 0;
	int out_index = count;
	for (int i = 0; i < B->count; i++)
		if (in_bounds(B->faces[i], split))
			scratch[in_index++] = B->faces[i];
		else
			scratch[out_index++] = B->faces[i];
	memcpy(B->faces, scratch, B->count * sizeof(Face));
	free(scratch);
}

void new_split(tree_box *B)
{
	int count;
	Bin bin = best_split(B, &count);
	bin_sort(B, bin, count);

	tree_box *L = calloc(1, sizeof(tree_box));
	L->faces = B->faces;
	L->count = count;
	set_bounds_box(L);
	B->left = L;

	tree_box *R = calloc(1, sizeof(tree_box));
	R->faces = &B->faces[count];
	R->count = B->count - count;
	set_bounds_box(R);
	B->right = R;
}

//takes flat array of faces returns root of tree and box_count of tree
tree_box *super_bvh(Face *faces, int count, int *box_count)
{
	tree_box *root_box = calloc(1, sizeof(tree_box));
	root_box->faces = faces;
	root_box->count = count;
	set_bounds_box(root_box);

	tree_box *Q = root_box;
	tree_box *B = NULL;

	int bcount = 1;

	while ((B = pop(&Q)))
	{
		new_split(B);
		//printf("split %d-%d\n", B->left->count, B->right->count);
		//getchar();
		bcount += 2;
		if (B->left->count > LEAF_THRESHOLD)
			push(&Q, B->left);
		if (B->right->count > LEAF_THRESHOLD)
			push(&Q, B->right);
	}

	printf("%d boxes\n", bcount);
	*box_count = bcount;
	return root_box;
}