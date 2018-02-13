#include "rt.h"

#define INF (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX}
#define NEG_INF (cl_float3){-1.0f * FLT_MAX, -1.0f * FLT_MAX, -1.0f * FLT_MAX}

#define SPLIT_TEST_NUM 10
#define LEAF_THRESHOLD 10000

enum axis{
	X_AXIS,
	Y_AXIS,
	Z_AXIS
};

typedef struct s_AABB
{
	cl_float3 min;
	cl_float3 max;

	struct s_AABB *members;
	int member_count;
	struct s_AABB *left;
	struct s_AABB *right;
	struct s_AABB *next;

	Face *f;
}				AABB;

cl_float3 center(AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	return vec_add(box->min, vec_scale(span, 0.5));
}

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

AABB *empty_box()
{
	AABB *empty = calloc(1, sizeof(AABB));
	empty->min = INF;
	empty->max = NEG_INF;
	return empty;
}

AABB *box_from_points(cl_float3 *points, int pt_count)
{
	 AABB *box = empty_box();

	 for (int i = 0; i < pt_count; i++)
	 {
	 	box->min.x = fmin(box->min.x, points[i].x);
		box->min.y = fmin(box->min.y, points[i].y);
		box->min.z = fmin(box->min.z, points[i].z);

		box->max.x = fmax(box->max.x, points[i].x);
		box->max.y = fmax(box->max.y, points[i].y);
		box->max.z = fmax(box->max.z, points[i].z);
	 }

	 return box;
}

AABB *box_from_face(Face *face)
{
	AABB *box = empty_box();
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

void flex_box(AABB *box, AABB *added)
{
	box->min.x = fmin(box->min.x, added->min.x);
	box->min.y = fmin(box->min.y, added->min.y);
	box->min.z = fmin(box->min.z, added->min.z);

	box->max.x = fmax(box->max.x, added->max.x);
	box->max.y = fmax(box->max.y, added->max.y);
	box->max.z = fmax(box->max.z, added->max.z);
}

AABB *box_from_boxes(AABB *boxes)
{
	AABB *box = empty_box();
	for (AABB *b = boxes; b; b = b->next)
	{
		flex_box(box, b);
		push(&box->members, b);
	}

	//all pointers in new_box (ie left, right, next) are null
	return box;
}

AABB *dupe_box(AABB* box)
{
	//NB duped box will point at SAME face NOT COPY of face
	AABB *dupe = calloc(1, sizeof(AABB));
	memcpy(dupe, box, sizeof(AABB));
	return dupe;
}

int point_in_box(cl_float3 point, AABB *box)
{
	if (box->min.x <= point.x && point.x <= box->max.x)
		if (box->min.y < point.y && point.y <= box->max.y)
			if (box->min.z < point.z && point.z <= box->max.z)
				return 1;
	return 0;
}

int box_in_box(AABB *box, AABB *in)
{
	//not generic box-box intersection, but adequate for context
	//just check all corners of the smaller box, true if any are in big box
	cl_float3 span = vec_sub(box->max, box->min);

	if (point_in_box(box->min, in))
		return 1;
	if (point_in_box(box->max, in))
		return 1;
	if (point_in_box((cl_float3){box->max.x, box->min.y, box->min.z}, in))
		return 1;
	if (point_in_box((cl_float3){box->min.x, box->max.y, box->min.z}, in))
		return 1;
	if (point_in_box((cl_float3){box->min.x, box->min.y, box->max.z}, in))
		return 1;
	if (point_in_box((cl_float3){box->max.x, box->max.y, box->min.z}, in))
		return 1;
	if (point_in_box((cl_float3){box->min.x, box->max.y, box->max.z}, in))
		return 1;
	if (point_in_box((cl_float3){box->max.x, box->min.y, box->max.z}, in))
		return 1;
	return 0;
}

typedef struct s_split
{
	AABB *left;
	AABB *left_flex;
	AABB *right;
	AABB *right_flex;
	int left_count;
	int right_count;
}				Split;

//intersect_box
int edge_clip(cl_float3 A, cl_float3 B, AABB *clipping, cl_float3 *hitpoint_a, cl_float3 *hitpoint_b)
{
	cl_float3 origin = A;
	cl_float3 edge = vec_sub(B, A);
	cl_float3 direction = unit_vec(edge);
	cl_float3 inv_dir = (cl_float3){1.0f / direction.x, 1.0f / direction.y, 1.0f / direction.z};

	float edge_t = edge.x * inv_dir.x;

	float tx0 = (clipping->min.x - origin.x) * inv_dir.x;
	float tx1 = (clipping->max.x - origin.x) * inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	float ty0 = (clipping->min.y - origin.y) * inv_dir.y;
	float ty1 = (clipping->max.y - origin.y) * inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);


	if ((tmin >= tymax) || (tymin >= tmax))
		return 0;

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (clipping->min.z - origin.z) * inv_dir.z;
	float tz1 = (clipping->max.z - origin.z) * inv_dir.z;
	float tzmin = fmin(tz0, tz1);
	float tzmax = fmax(tz0, tz1);

	if ((tmin >= tzmax) || (tzmin >= tmax))
		return 0;

    tmin = fmax(tzmin, tmin);
	tmax = fmin(tzmax, tmax);

	//need to make sure we're not updating with a further point

	if (tmin > 0 && tmin < edge_t)
	*hitpoint_a = vec_add(A, vec_scale(B, tmin));
	if (tmax > 0 && tmax < edge_t)
	*hitpoint_b = vec_add(A, vec_scale(B, tmax));
	return 1;
}

void clip_box(AABB *box, AABB *bound)
{
	//initial clip is easy because they're AABBs
	if (point_in_box(box->min, bound))
	{
		//min is in so we update max
		box->max.x = fmin(box->max.x, bound->max.x);
		box->max.y = fmin(box->max.y, bound->max.y);
		box->max.z = fmin(box->max.z, bound->max.z);
	}
	else
	{
		//max is in so we update min
		box->min.x = fmax(box->min.x, bound->min.x);
		box->min.y = fmax(box->min.y, bound->min.y);
		box->max.z = fmax(box->min.z, bound->min.z);
	}

	cl_float3 A, B, C;
	A = box->f->verts[0];
	B = box->f->verts[1];
	C = box->f->verts[2];

	int res_a = 0;
	int res_b = 0;
	int res_c = 0;

	cl_float3 points[6];
	int pt_count = 0;

	if(edge_clip(A, B, bound, &points[pt_count], &points[pt_count + 1]))
	{
		res_a++;
		res_b++;
		pt_count += 2;
	}

	if(edge_clip(A, C, bound, &points[pt_count], &points[pt_count + 1]))
	{
		res_a++;
		res_c++;
		pt_count += 2;
	}

	if(edge_clip(B, C, bound, &points[pt_count], &points[pt_count + 1]))
	{
		res_b++;
		res_c++;
		pt_count += 2;
	}

	//basically, if we get an "update" for a, b or c, we don't consider the original point anymore.
	//but we do consider both possible "updates", and if we don't get any we consider the original point.
	if (res_a == 0)
		points[pt_count++] = A;
	if (res_b == 0)
		points[pt_count++] = B;
	if (res_c == 0)
		points[pt_count++] = C;

	AABB *clippy = box_from_points(points, pt_count);

	//i hope this is this easy.

	box->max.x = fmin(clippy->max.x, bound->max.x);
	box->max.y = fmin(clippy->max.y, bound->max.y);
	box->max.z = fmin(clippy->max.z, bound->max.z);

	box->min.x = fmax(clippy->min.x, bound->min.x);
	box->min.y = fmax(clippy->min.y, bound->min.y);
	box->min.z = fmax(clippy->min.z, bound->min.z);
}

Split *new_split(AABB *box, enum axis a, int i, int total)
{
	cl_float3 span = vec_sub(box->max, box->min);
	Split *split = calloc(1, sizeof(Split));
	split->left = dupe_box(box);
	split->left_flex = empty_box();
	split->right = dupe_box(box);
	split->right_flex = empty_box();
	if (a == X_AXIS)
	{
		split->left->max.x = split->left->min.x + span.x * (float)i / (float)total;
		split->right->min.x = split->left->max.x;
	}
	if (a == Y_AXIS)
	{
		split->left->max.y = split->left->min.y + span.y * (float)i / (float)total;
		split->right->min.y = split->left->max.y;
	}
	if (a == Z_AXIS)
	{
		split->left->max.z = split->left->min.z + span.z * (float)i / (float)total;
		split->right->min.z = split->left->max.z;
	}
	return split;
}

void free_split(Split *split)
{
	free(split->left);
	free(split->right);
	free(split->left_flex);
	free(split->right_flex);
	free(split);
}

float SA(AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	return 2 * span.x * span.y + 2 * span.y * span.z + 2 * span.x * span.z;
}

float SAH(Split *split, AABB *parent)
{
	return SA(split->left_flex) * split->left_count / SA(parent) + SA(split->right_flex) * split->right_count / SA(parent);
}

Split *best_spatial_split(AABB *box)
{
	Split **spatials = calloc(SPLIT_TEST_NUM * 3, sizeof(Split));
	for (int i = 0; i < SPLIT_TEST_NUM; i++)
	{
		spatials[i] = new_split(box, X_AXIS, i, SPLIT_TEST_NUM);
		spatials[SPLIT_TEST_NUM + i] = new_split(box, Y_AXIS, i, SPLIT_TEST_NUM);
		spatials[2 * SPLIT_TEST_NUM + i] = new_split(box, Z_AXIS, i, SPLIT_TEST_NUM);
	}

	//for each member, "add" to each split (dont actually make copies)
	for (AABB *b = box->members; b; b = b->next)
		for (int i = 0; i < SPLIT_TEST_NUM * 3; i++)
		{
			if (box_in_box(b, spatials[i]->left) && box_in_box(b, spatials[i]->right))
			{
				AABB *lclip = dupe_box(b);
				AABB *rclip = dupe_box(b);

				clip_box(lclip, spatials[i]->left);
				clip_box(rclip, spatials[i]->right);

				flex_box(spatials[i]->left_flex, lclip);
				flex_box(spatials[i]->right_flex, rclip);
				
				free(lclip);
				free(rclip);
			}
			else if (box_in_box(b, spatials[i]->left))
			{
				flex_box(spatials[i]->left_flex, b);
				spatials[i]->left_count++;
			}
			else
			{
				flex_box(spatials[i]->right_flex, b);
				spatials[i]->right_count++;
			}
		}

	//measure and choose best split

	float min_SAH = FLT_MAX;
	int min_ind = 0;
	for (int i = 0; i < SPLIT_TEST_NUM * 3; i++)
	{
		float res = SAH(spatials[i], box);
		if (res < min_SAH)
		{
			min_SAH = res;
			min_ind = i;
		}
	}

	//clean up
	Split *winner = spatials[min_ind];
	for (int i = 0; i < SPLIT_TEST_NUM * 3; i++)
		if (i == min_ind)
			continue;
		else
			free_split(spatials[i]);
	free(spatials);

	return winner;
}

void partition(AABB *box)
{
	Split *spatial = best_spatial_split(box);

	box->left = dupe_box(spatial->left_flex);
	box->right = dupe_box(spatial->right_flex);

	for (AABB *b = box->members; b; b = b->next)
		if (box_in_box(b, box->left) && box_in_box(b, box->right))
		{
			AABB *lclip = dupe_box(b);
			AABB *rclip = dupe_box(b);

			clip_box(lclip, box->left);
			clip_box(rclip, box->right);

			push(&box->left->members, lclip);
			box->left->member_count++;
			push(&box->left->members, lclip);
			box->right->member_count++;

			free(b);
		}
		else if (box_in_box(b, spatial->left))
		{
			push(&box->left->members, b);
			box->left->member_count++;
		}
		else
		{
			push(&box->right->members, b);
			box->right->member_count++;
		}
	free_split(spatial);
}

AABB *sbvh(Face *faces, int *box_count)
{
	//put all faces in AABBs
	AABB *boxes = NULL;
	for (Face *f = faces; f; f = f->next)
		push(&boxes, box_from_face(f));

	AABB *root_box = box_from_boxes(boxes);

	AABB *stack = root_box;
	int count = 1;
	while (stack)
	{
		AABB *box = pop(&stack);
		partition(box);
		count += 2;
		if (box->left->member_count > LEAF_THRESHOLD)
			push(&stack, box->left);
		if (box->right->member_count > LEAF_THRESHOLD)
			push(&stack, box->right);
	}
	printf("done?? %d boxes?", count);
	return root_box;
}