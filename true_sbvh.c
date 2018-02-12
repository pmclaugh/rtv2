#include "rt.h"

#define INF (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX}
#define NEG_INF (cl_float3){-1.0f * FLT_MAX, -1.0f * FLT_MAX, -1.0f * FLT_MAX}

typedef struct s_AABB
{
	cl_float3 min;
	cl_float3 max;

	struct s_AABB *members;
	struct s_AABB *left;
	struct s_AABB *right;
	struct s_AABB *next;

	Face *f;
}				AABB;

void push(AABB **stack, AABB *box)
{
	if (*stack)
		box->next = *stack;
	*stack = box;
}

AABB *pop(AABB **stack)
{
	AABB *popped = NULL;
	if (*stack)
	{
		popped = *stack;
		*stack = (*stack)->next;
	}
	return popped;
}


AABB *box_from_face(Face *face)
{
	AABB *box = calloc(1, sizeof(AABB));
	box->min = INF;
	box->max = NEG_INF;
	for (int i = 0; i < face->shape; i++)
	{
		box->min.x = fmin(box->min.x, face->verts[i].x);
		box->min.y = fmin(box->min.y, face->verts[i].y);
		box->min.z = fmin(box->min.z, face->verts[i].z);

		box->max.x = fmax(box->max.x, face->verts[i].x);
		box->max.y = fmax(box->max.y, face->verts[i].y);
		box->max.z = fmax(box->max.z, face->verts[i].z);
	}

	box->f = face;
	//calloced so everything else is already null like it should be.
	return box;
}

AABB *box_from_boxes(AABB *boxes)
{
	AABB *new_box = calloc(1, sizeof(AABB));
	new_box->min = INF;
	new_box->max = NEG_INF;
	for (AABB *b = boxes; b; b = b->next)
	{
		new_box->min.x = fmin(new_box->min.x, b->min.x);
		new_box->min.y = fmin(new_box->min.y, b->min.y);
		new_box->min.z = fmin(new_box->min.z, b->min.z);

		new_box->max.x = fmax(new_box->max.x, b->max.x);
		new_box->max.y = fmax(new_box->max.y, b->max.y);
		new_box->max.z = fmax(new_box->max.z, b->max.z);

		push(&new_box->members, b);
	}

	//all pointers in new_box (ie left, right, next) are null
	return new_box;
}

typedef struct s_split
{
	AABB *left;
	AABB *right;
	int left_count;
	int right_count;

	float surface_area_heuristic;
}

void split(AABB *box)
{
	//determine best spatial split
	//determine best object split

	//do one or the other
}

AABB *sbvh(Face *faces, int *box_count)
{
	//put all faces in AABBs
	AABB *boxes = NULL;
	for (Face *f = faces; f; f = f->next)
		push(&boxes, box_from_face(f));

	AABB *root_box = box_from_boxes(boxes);

	AABB *stack = root_box;
	while (stack)
	{
		AABB box = pop(&stack);
		split(box);
		//push children onto stack if they still need to be split further
	}
	return root_box;
}