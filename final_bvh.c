#include "rt.h"

typedef struct s_BVH
{
	Face *faces;
	int face_count;
	cl_float3 min;
	cl_float3 max;
	struct s_BVH *left;
	struct s_BVH *right;

	struct s_BVH *next;
	int depth;
}				BVH;

#define INF (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX}
#define NEG_INF (cl_float3){-1.0f * FLT_MAX, -1.0f * FLT_MAX, -1.0f * FLT_MAX}

void shrink_wrap(BVH *box, BVH *bag)
{
	//I think this is right??
	box->min.x = fmax(box->min.x, bag->min.x);
	box->max.x = fmin(box->max.x, bag->max.x);
	box->min.y = fmax(box->min.y, bag->min.y);
	box->max.y = fmin(box->max.y, bag->max.y);
	box->min.z = fmax(box->min.z, bag->min.z);
	box->max.z = fmin(box->max.z, bag->max.z);
}

BVH *new_box(cl_float3 min, cl_float3 max)
{
	BVH *box = calloc(1, sizeof(BVH));
	box->min = min;
	box->max = max;
	return box;
}

void add_to_box(Face *face, BVH *box)
{
	Face *add = malloc(sizeof(Face));
	memcpy(add, face, sizeof(Face));
	add->next = box->faces;
	box->faces = add;
	//box->face_count++;
}

void add_to_bag(Face *face, BVH *box)
{
	box->face_count++;
	for (int i = 0; i < face->shape; i++)
	{
		box->min.x = fmin(box->min.x, face->verts[i].x);
		box->max.x = fmax(box->max.x, face->verts[i].x);
		box->min.y = fmin(box->min.y, face->verts[i].y);
		box->max.y = fmax(box->max.y, face->verts[i].y);
		box->min.z = fmin(box->min.z, face->verts[i].z);
		box->max.z = fmax(box->max.z, face->verts[i].z);
	}
}

void push(BVH **stack, BVH *box)
{
	if (*stack)
		box->next = *stack;
	*stack = box;
}

BVH *pop(BVH **stack)
{
	BVH *popped = NULL;
	if (*stack)
	{
		popped = *stack;
		*stack = (*stack)->next;
	}
	return popped;
}

int in_box(Face *f, BVH *box)
{
	// this is more complicated now!
	cl_float3 center = vec_scale(vec_add(box->min, box->max), 0.5f);
	cl_float3 h = vec_sub(box->max, center);
	cl_float3 v0 = f->verts[0];
	cl_float3 v1 = f->verts[1];
	cl_float3 v2 = f->verts[2];

	float boxcenter[3] = {center.x, center.y, center.z};
	float half[3] = {h.x, h.y, h.z};

	float tris[3][3] = {{v0.x, v0.y, v0.z}, {v1.x, v1.y, v1.z}, {v2.x, v2.y, v2.z}};

	return triBoxOverlap(boxcenter, half, tris);
}

float area(BVH *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	return fabs(span.x) * fabs(span.y) * fabs(span.z);
}

float SA(BVH *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	return 2 * span.x * span.y + 2 * span.y * span.z + 2 * span.x * span.z;
}

float SAH(BVH *a, BVH *b, float parent_SA)
{
	//printf("SA(a) %.f SA(b) %.f, parent_SA %.f\n", SA(a), SA(b), parent_SA);
	return SA(a) * a->face_count / parent_SA + SA(b) * (b->face_count) / parent_SA;
}

void print_box(BVH *box)
{
	print_vec(box->min);
	print_vec(box->max);
	printf("area %.f SA %.f\n", area(box), SA(box));
	printf("%d faces\n", box->face_count);
}

void free_list(Face *faces)
{
	while(faces)
	{
		Face *tmp = faces;
		faces = faces->next;
		free(tmp);
	}
}

#define splits_per_axis 8
#define LEAF_THRESHOLD 32 //weird performance black hole when this goes much lower

void best_split(BVH *box)
{
	// printf("enter best split\n");

	BVH **lefts = calloc(3 * splits_per_axis, sizeof(BVH *));
	BVH **left_bags = calloc(3 * splits_per_axis, sizeof(BVH *));
	BVH **rights = calloc(3 * splits_per_axis, sizeof(BVH *));
	BVH **right_bags = calloc(3 * splits_per_axis, sizeof(BVH *));
	cl_float3 span = vec_sub(box->max, box->min);
	for (int i = 0; i < splits_per_axis; i++)
	{
		lefts[i] = new_box(box->min, (cl_float3){box->min.x + (float)(i + 1) * span.x / (float)(splits_per_axis + 1), box->max.y, box->max.z});
		rights[i] = new_box((cl_float3){box->min.x + (float)(i + 1) * span.x / (float)(splits_per_axis + 1), box->min.y, box->min.z}, box->max);

		lefts[i + splits_per_axis] = 		new_box(box->min, (cl_float3){box->max.x, box->min.y + (float)(i + 1) * span.y / (float)(splits_per_axis + 1), box->max.z});
		rights[i + splits_per_axis] = 	new_box((cl_float3){box->min.x, box->min.y + (float)(i + 1) * span.y / (float)(splits_per_axis + 1), box->min.z}, box->max);

		lefts[i + 2 * splits_per_axis] = 	new_box(box->min, (cl_float3){box->max.x, box->max.y, box->min.z + (float)(i + 1) * span.z / (float)(splits_per_axis + 1)});
		rights[i + 2 * splits_per_axis] = new_box((cl_float3){box->min.x, box->min.y, box->min.z + (float)(i + 1) * span.z / (float)(splits_per_axis + 1)}, box->max);
	}

	// for (int i = 0; i < 3 * splits_per_axis; i++)
	// {
	// 	print_box(lefts[i]);
	// 	print_box(rights[i]);
	// }
	// getchar();

	for (int i = 0; i < 3 * splits_per_axis; i++)
	{
		left_bags[i] = new_box(INF, NEG_INF);
		right_bags[i] = new_box(INF, NEG_INF);
	}

	for (int i = 0; i < 3 * splits_per_axis; i++)
		for (Face *f = box->faces; f; f = f->next)
		{
			if (in_box(f, lefts[i]))
				add_to_bag(f, left_bags[i]);
			if (in_box(f, rights[i]))
				add_to_bag(f, right_bags[i]);
		}

	for (int i = 0; i < 3 * splits_per_axis; i++)
	{
		shrink_wrap(rights[i], right_bags[i]);
		shrink_wrap(lefts[i], left_bags[i]);
		rights[i]->face_count = right_bags[i]->face_count;
		lefts[i]->face_count = left_bags[i]->face_count;
	}

	int best_SAH_ind = 0;
	float best_SAH = FLT_MAX;
	float parent_SA = SA(box);
	for(int i = 0; i < 3 * splits_per_axis; i++)
	{
		float this_SAH = SAH(lefts[i], rights[i], parent_SA);
		if (this_SAH < best_SAH)
		{
			best_SAH_ind = i;
			best_SAH = this_SAH;
			//printf("best: %f\n", best_SAH);
		}
	}

	box->left = lefts[best_SAH_ind];
	box->left->depth = box->depth + 1;
	box->right = rights[best_SAH_ind];
	box->right->depth = box->depth + 1;

	//add to box!
	for (Face *f = box->faces; f; f = f->next)
	{
		if (in_box(f, box->left))
			add_to_box(f, box->left);
		if (in_box(f, box->right))
			add_to_box(f, box->right);
	}

	// print_box(box->left);
	// print_box(box->right);
	// getchar();

	free_list(box->faces);
	for(int i = 0; i < 3 * splits_per_axis; i++)
	{
		free(left_bags[i]);
		free(right_bags[i]);
		if (i == best_SAH_ind)
			continue ;
		free(lefts[i]);
		free(rights[i]);
	}
	free(lefts);
	free(rights);
	free(left_bags);
	free(right_bags);
}



BVH *best_bvh(Face *faces, int *box_count)
{
	BVH *root = new_box(INF, NEG_INF);
	for (Face *f = faces; f; f = f->next)
	{
		add_to_box(f, root);
		add_to_bag(f, root);
	}
	print_box(root);

	int count = 0;
	BVH *stack = root;
	BVH *box = NULL;
	BVH *leaves = NULL;
	while (stack)
	{
		box = pop(&stack);
		best_split(box);
		count += 2;

		if (box->left->face_count < LEAF_THRESHOLD)
			push(&leaves, box->left);
		else
			push(&stack, box->left);

		if (box->right->face_count < LEAF_THRESHOLD)
			push(&leaves, box->right);
		else
			push(&stack, box->right);
	}

	float area_total = 0.0f;
	int tricount = 0;
	int leafcount = 0;
	int maxdepth = 0;
	int totaldepth = 0;
	while(leaves)
	{
		leafcount++;
		if (leaves->depth > maxdepth)
			maxdepth = leaves->depth;
		totaldepth += leaves->depth;
		tricount += leaves->face_count;
		area_total += area(leaves);
		leaves = leaves->next;
	}
	printf("ended as %d triangles\n", tricount);
	printf("%f leaf / total\n", area_total / area(root));
	printf("%d leaves, max depth %d avg depth %.1f\n", leafcount, maxdepth, (float)totaldepth / (float)leafcount);
	*box_count = count;
	return root;
}
