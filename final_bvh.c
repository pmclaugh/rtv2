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
}				BVH;

#define INF (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX}
#define NEG_INF (cl_float3){-1.0f * FLT_MAX, -1.0f * FLT_MAX, -1.0f * FLT_MAX}

void shrink_wrap(BVH *box)
{
	//scan for min,max xyz
	//update if improved, dont change if worse

	float minx = FLT_MAX;
	float miny = FLT_MAX;
	float minz = FLT_MAX;
	float maxx = -1.0f * FLT_MAX;
	float maxy = -1.0f * FLT_MAX;
	float maxz = -1.0f * FLT_MAX;

	Face *f = box->faces;
	while(f)
	{
		for (int i = 0; i < f->shape; i++)
		{
			minx = fmin(minx, f->verts[i].x);
			maxx = fmax(maxx, f->verts[i].x);
			miny = fmin(miny, f->verts[i].y);
			maxy = fmax(maxy, f->verts[i].y);
			minz = fmin(minz, f->verts[i].z);
			maxz = fmax(maxz, f->verts[i].z);
		}
	}
	//I think this is right??
	box->min.x = fmax(box->min.x, minx);
	box->max.x = fmin(box->max.x, maxx);
	box->min.y = fmax(box->min.y, miny);
	box->max.y = fmin(box->max.y, maxy);
	box->min.z = fmax(box->min.z, minz);
	box->max.z = fmin(box->max.z, maxz);
}

BVH new_box(cl_float3 min, cl_float3 max)
{
	return (BVH){NULL, 0, min, max, NULL, NULL, NULL};
}

void add_to_box_updating(Face *face, BVH *box)
{
	Face *add = calloc(1, sizeof(Face));
	*add = *face;
	add->next = box->faces;
	box->faces = add;
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

void add_to_box(Face *face, BVH *box)
{
	Face *add = calloc(1, sizeof(Face));
	*add = *face;
	add->next = box->faces;
	box->faces = add;
	box->face_count++;
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

int in_box(Face *f, BVH box)
{
	if (box.min.x <= f->center.x && f->center.x <= box.max.x)
		if (box.min.y <= f->center.y && f->center.y <= box.max.y)
			if (box.min.z <= f->center.z && f->center.z <= box.max.z)
				return (1);
	return (0);
}

float SA(BVH *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	return 2 * span.x * span.y + 2 * span.y * span.z + 2 * span.x * span.z;
}

float SAH(BVH *a, BVH *b, float parent_SA)
{
	//printf("SA(a) %f SA(b) %f, parent_SA %f\n", SA(a), SA(b), parent_SA);
	return SA(a) * a->face_count / parent_SA + SA(b) * (b->face_count) / parent_SA;
}

#define splits_per_axis 10

void best_split(BVH *box)
{
	//left and right are null. find best possible left and right.
	cl_float3 span = vec_sub(box->max, box->min);

	BVH *splits = calloc(2 * 3 * splits_per_axis, sizeof(BVH));
	for (int i = 0; i < splits_per_axis; i++)
	{
		splits[i] = 								new_box(box->min, (cl_float3){box->min.x + (float)(i + 1) * span.x / (float)(splits_per_axis + 1), box->max.y, box->max.z});
		splits[2 * i + 1] = 						new_box((cl_float3){box->min.x + (float)(i + 1) * span.x / (float)(splits_per_axis + 1), box->min.y, box->min.z}, box->max);

		splits[2 * (i + splits_per_axis)] = 		new_box(box->min, (cl_float3){box->max.x, box->min.y + (float)(i + 1) * span.y / (float)(splits_per_axis + 1), box->max.z});
		splits[2 * (i + splits_per_axis) + 1] = 	new_box((cl_float3){box->min.x, box->min.y + (float)(i + 1) * span.y / (float)(splits_per_axis + 1), box->min.z}, box->max);

		splits[2 * (i + 2 * splits_per_axis)] = 	new_box(box->min, (cl_float3){box->max.x, box->max.y, box->min.z + (float)(i + 1) * span.z / (float)(splits_per_axis + 1)});
		splits[2 * (i + 2 * splits_per_axis) + 1] = new_box((cl_float3){box->min.x, box->min.y, box->min.z + (float)(i + 1) * span.z / (float)(splits_per_axis + 1)}, box->max);
	}

	for (int i = 0; i < 2 * 3 * splits_per_axis; i++)
		for (Face *f = box->faces; f; f = f->next)
			if (in_box(f, splits[i]))
				add_to_box(f, &splits[i]);

	for (int i = 0; i < 2 * 3 * splits_per_axis; i++)
		shrink_wrap(&splits[i]);

	int best_SAH_ind = 0;
	float best_SAH = FLT_MAX;
	float parent_SA = SA(box);
	for(int i = 0; i < 3 * splits_per_axis; i++)
	{
		float this_SAH = SAH(&splits[2 * i], &splits[2 * i + 1], parent_SA);
		if (this_SAH < best_SAH)
		{
			best_SAH_ind = 2 * i;
			best_SAH = this_SAH;
		}
	}

	box->left = calloc(1, sizeof(BVH));
	*(box->left) = splits[best_SAH_ind];
	box->right = calloc(1, sizeof(BVH));
	*(box->right) = splits[best_SAH_ind + 1];

}

BVH *best_bvh(Face *faces, int *box_count)
{
	BVH *root = calloc(1, sizeof(BVH));
	*root = new_box(INF, NEG_INF);
	for (Face *f = faces; f; f = f->next)
		add_to_box_updating(f, root);

	BVH *stack = root;
	BVH *box = NULL;
	BVH *leaves = NULL;
	while (stack)
	{
		pop(&stack);
		best_split(box);

		if (box->left->face_count < LEAF_THRESHOLD)
			push(&leaves, box->left);
		else
			push(&stack, box->left);

		if (box->right->face_count < LEAF_THRESHOLD)
			push(&leaves, box->right);
		else
			push(&stack, box->right);
	}
}
