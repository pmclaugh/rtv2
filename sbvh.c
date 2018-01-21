#include "rt.h"

#define SPLIT_COUNT 16
#define LEAF_THRESHOLD 16

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
	set_bounds(&B->faces[B->face_ind], B->count, &B->min, &B->max);
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

static float area(cl_float3 min, cl_float3 max)
{
	cl_float3 span = vec_sub(max, min);
	return span.x * span.y * span.z;
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
		min.x = fmin(min.x, B->faces[B->face_ind + i].center.x);
		max.x = fmax(max.x, B->faces[B->face_ind + i].center.x);
		min.y = fmin(min.y, B->faces[B->face_ind + i].center.y);
		max.y = fmax(max.y, B->faces[B->face_ind + i].center.y);
		min.z = fmin(min.z, B->faces[B->face_ind + i].center.z);
		max.z = fmax(max.z, B->faces[B->face_ind + i].center.z);
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
			if (in_bounds(B->faces[B->face_ind + i], bins[j]))
			{
				counts[j]++;
				adjust_bounds(B->faces[B->face_ind + i], &boxes[2 * j]);
			}
			else
				adjust_bounds(B->faces[B->face_ind + i], &boxes[2 * j + 1]);
	
	//which pair is best split?
	float parent_SA = SA((Bin){B->min, B->max});
	//printf("parent_SA %f\n", parent_SA);
	float min_SA = FLT_MAX;
	int min_ind = -1;
	for (int i = 0; i < 3 * SPLIT_COUNT; i++)
	{
		if (counts[i] == 0 || counts[i] == B->count)
			continue;
		float score = SAH(boxes[2 * i], boxes[2 * i + 1], (float)counts[i], (float)B->count, parent_SA);
		//printf("score %f for split %d\n", score, i);
		if (score < min_SA)
		{
			min_SA = score;
			min_ind = i;
		}
	}
	// printf("picked split %d, %d - %d\n", min_ind, counts[min_ind], B->count - counts[min_ind]);
	// printf("P SA %9.0f P area %12.0f\nL SA %9.0f L area %12.0f\nR SA %9.0f R area %12.0f\n", parent_SA, area(B->min, B->max),
	// 		SA(boxes[2 * min_ind]), area(boxes[2 * min_ind].min, boxes[2 * min_ind].max),
	// 		SA(boxes[2 * min_ind + 1]), area(boxes[2 * min_ind + 1].min, boxes[2 * min_ind + 1].max));
	// getchar();
	if (min_ind == -1)
	{
		for (int i = 0; i < B->count; i++)
		{
			print_vec(B->faces[B->face_ind + i].center);
			for (int j = 0; j < B->faces[B->face_ind + i].shape; j++)
				print_vec(B->faces[B->face_ind + i].verts[j]);
			printf("\n");
		}
		getchar();
	}

	*count = counts[min_ind];
	return bins[min_ind];
}

static void bin_sort(tree_box *B, Bin split, int count)
{
	Face *scratch = calloc(B->count, sizeof(Face));
	int in_index = 0;
	int out_index = count;
	for (int i = 0; i < B->count; i++)
		if (in_bounds(B->faces[B->face_ind + i], split))
			scratch[in_index++] = B->faces[B->face_ind + i];
		else
			scratch[out_index++] = B->faces[B->face_ind + i];
	memcpy(&(B->faces[B->face_ind]), scratch, B->count * sizeof(Face));
	free(scratch);
}

void new_split(tree_box *B)
{
	int count;
	Bin bin = best_split(B, &count);
	bin_sort(B, bin, count);

	tree_box *L = calloc(1, sizeof(tree_box));
	L->faces = B->faces;
	L->face_ind = B->face_ind;
	L->count = count;
	set_bounds_box(L);
	B->left = L;

	tree_box *R = calloc(1, sizeof(tree_box));
	R->faces = B->faces;
	R->count = B->count - count;
	R->face_ind = B->face_ind + count;
	set_bounds_box(R);
	B->right = R;
}

//takes flat array of faces returns root of tree and box_count of tree
tree_box *super_bvh(Face *faces, int count, int *box_count)
{
	tree_box *root_box = calloc(1, sizeof(tree_box));
	root_box->faces = faces;
	root_box->face_ind = 0;
	root_box->count = count;
	root_box->parent = NULL;
	set_bounds_box(root_box);

	tree_box *Q = root_box;
	tree_box *B = NULL;

	int bcount = 1;

	while ((B = pop(&Q)))
	{
		new_split(B);
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

gpu_bin *flatten_bvh(tree_box *bvh, int box_count)
{
	gpu_bin *bins = calloc(box_count, sizeof(gpu_bin));
	tree_box *Q = bvh;
	tree_box *B = NULL;

	int i = 0;
	while ((B = pop(&Q)))
	{
		cl_float3 min = B->min;
		cl_float3 max = B->max;
		bins[i] = (gpu_bin){min.x, min.y, min.z, 0, max.x, max.y, max.z, 0};

		if (B->left) // all nodes either have left AND right or neither.
		{
			B->left->parent = &bins[i];
			push(&Q, B->left);
			B->right->parent = &bins[i];
			push(&Q, B->right);
		}
		else
		{
			bins[i].lind = -3 * B->face_ind;
			bins[i].rind = -3 * B->count;
		}
		if (B->parent)
		{
			if (B->parent->lind)
				B->parent->rind = i;
			else
				B->parent->lind = i;
		}
		i++;
	}

	//well, it's a little weird, but i've achieved my goal of a 32-byte box size.
	
	// for (int i = 0; i < box_count; i++)
	// {
	// 	printf("\nbin %d\n", i);
	// 	printf("min: %.0f %.0f %.0f\n", bins[i].minx, bins[i].miny, bins[i].minz);
	// 	printf("max: %.0f %.0f %.0f\n", bins[i].maxx, bins[i].maxy, bins[i].maxz);
	// 	printf("lind %d rind %d\n", bins[i].lind, bins[i].rind);
	// }

	return bins;
}