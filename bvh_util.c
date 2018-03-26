#include "rt.h"

//storage place for known-good basic functions related to BVH construction

cl_float3 center(const AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	return vec_add(box->min, vec_scale(span, 0.5));
}

void push(AABB **stack, AABB *box)
{
	box->next = *stack;
	*stack = box;
}

AABB *pop(AABB **stack)
{
	AABB *popped = NULL;
	if (*stack)
	{
		popped = *stack;
		*stack = popped->next;
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
	for (AABB *b = boxes; b;)
	{
		AABB *tmp = b->next;
		flex_box(box, b);
		push(&box->members, b);
		box->member_count++;
		b = tmp;
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
		if (box->min.y <= point.y  && point.y <= box->max.y)
			if (box->min.z <= point.z && point.z <= box->max.z)
				return 1;
	return 0;
}

int box_in_box(AABB *box, AABB *in)
{
	//not generic box-box intersection, but adequate for context
	//just check all corners of the smaller box, true if any are in big box
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

int all_in(AABB *box, AABB *in)
{
	if (!point_in_box(box->min, in))
		return 0;
	if (!point_in_box(box->max, in))
		return 0;
	if (!point_in_box((cl_float3){box->max.x, box->min.y, box->min.z}, in))
		return 0;
	if (!point_in_box((cl_float3){box->min.x, box->max.y, box->min.z}, in))
		return 0;
	if (!point_in_box((cl_float3){box->min.x, box->min.y, box->max.z}, in))
		return 0;
	if (!point_in_box((cl_float3){box->max.x, box->max.y, box->min.z}, in))
		return 0;
	if (!point_in_box((cl_float3){box->min.x, box->max.y, box->max.z}, in))
		return 0;
	if (!point_in_box((cl_float3){box->max.x, box->min.y, box->max.z}, in))
		return 0;
	return 1;
}

void print_box(AABB *box)
{
	print_vec(box->min);
	print_vec(box->max);
	//printf("member count %d\nleft %p right %p\n", box->member_count, box->left, box->right);
}

void print_face(Face *f)
{
	for (int i = 0; i < f->shape; i++)
		print_vec(f->verts[i]);
}

//intersect_box
int edge_clip(cl_float3 A, cl_float3 B, AABB *clipping, cl_float3 *points, int *pt_count, int *res_a, int *res_b)
{
	cl_float3 origin = A;
	cl_float3 edge = vec_sub(B, A);
	cl_float3 direction = unit_vec(edge);
	cl_float3 inv_dir = (cl_float3){1.0f / direction.x, 1.0f / direction.y, 1.0f / direction.z};

	float edge_t = fmin(fmin(edge.x * inv_dir.x, edge.y * inv_dir.y), edge.z * inv_dir.z);
	//printf("edge_t is %f\n", edge_t);

	float tx0 = (clipping->min.x - origin.x) * inv_dir.x;
	float tx1 = (clipping->max.x - origin.x) * inv_dir.x;
	float tmin = fmin(tx0, tx1);
	float tmax = fmax(tx0, tx1);

	float ty0 = (clipping->min.y - origin.y) * inv_dir.y;
	float ty1 = (clipping->max.y - origin.y) * inv_dir.y;
	float tymin = fmin(ty0, ty1);
	float tymax = fmax(ty0, ty1);

	if ((tmin > tymax) || (tymin > tmax))
		return 0;

	tmin = fmax(tymin, tmin);
	tmax = fmin(tymax, tmax);

	float tz0 = (clipping->min.z - origin.z) * inv_dir.z;
	float tz1 = (clipping->max.z - origin.z) * inv_dir.z;
	float tzmin = fmin(tz0, tz1);
	float tzmax = fmax(tz0, tz1);

	if ((tmin > tzmax) || (tzmin > tmax))
		return 0;

    tmin = fmax(tzmin, tmin);
	tmax = fmin(tzmax, tmax);

	//need to make sure we're not updating with a point that goes past far vertex

	if (tmin > 0 && tmin <= edge_t)
	{
		points[*pt_count] = vec_add(A, vec_scale(direction, tmin));
		*pt_count = *pt_count + 1;
		*res_a = *res_a + 1;
	}
	if (tmax > 0 && tmax <= edge_t)
	{
		points[*pt_count] = vec_add(A, vec_scale(direction, tmax));
		*pt_count = *pt_count + 1;
		*res_b = *res_b + 1;
	}
	return 1;
}

void clip_box(AABB *box, AABB *bound)
{
	// printf("\n\nattempting to clip this box\n");
	// print_box(box);
	// printf("containing this face\n");
	// print_face(box->f);
	// printf("with this box\n");
	// print_box(bound);

	//initial clip is easy because they're AABBs
	if (point_in_box(box->min, bound))
	{
		if (!point_in_box((cl_float3){box->max.x, box->min.y, box->min.z}, bound))
			box->max.x = bound->max.x;
		else if (!point_in_box((cl_float3){box->min.x, box->max.y, box->min.z}, bound))
			box->max.y = bound->max.y;
		else
			box->max.z = bound->max.z;
	}
	else
	{
		if (!point_in_box((cl_float3){box->min.x, box->max.y, box->max.z}, bound))
			box->min.x = bound->min.x;
		else if (!point_in_box((cl_float3){box->max.x, box->min.y, box->max.z}, bound))
			box->min.y = bound->min.y;
		else
			box->min.z = bound->min.z;
	}

	// printf("after initial clip\n");
	// print_box(box);

	//code below has some sort of bug or misconception in it. it needs fixed though because it's vital for performance
	//UPDATE: I believe this is now resolved but needs deeper testing to be sure.

	cl_float3 A, B, C;
	A = box->f->verts[0];
	B = box->f->verts[1];
	C = box->f->verts[2];

	int res_a = 0;
	int res_b = 0;
	int res_c = 0;

	cl_float3 points[6];
	int pt_count = 0;

	edge_clip(A, B, box, points, &pt_count, &res_a, &res_b);
	edge_clip(A, C, box, points, &pt_count, &res_a, &res_c);
	edge_clip(B, C, box, points, &pt_count, &res_b, &res_c);

	//basically, if we get an "update" for a, b or c, we don't consider the original point anymore.
	//but we do consider both possible "updates", and if we don't get any we consider the original point.
	if (res_a == 0)
		points[pt_count++] = A;
	if (res_b == 0)
		points[pt_count++] = B;
	if (res_c == 0)
		points[pt_count++] = C;
	
	// printf("%d points in input\n", pt_count);
	// for (int i = 0; i < pt_count; i++)
	// 	print_vec(points[i]);

	AABB *clippy = box_from_points(points, pt_count);
	// printf("clippy box\n");
	// print_box(clippy);

	//the result we want is the overlap of clippy and box

	box->max.x = fmin(clippy->max.x, box->max.x);
	box->max.y = fmin(clippy->max.y, box->max.y);
	box->max.z = fmin(clippy->max.z, box->max.z);

	box->min.x = fmax(clippy->min.x, box->min.x);
	box->min.y = fmax(clippy->min.y, box->min.y);
	box->min.z = fmax(clippy->min.z, box->min.z);

	// printf("final result\n");
	// print_box(box);
	// getchar();
	free(clippy);
}

Split *new_split(AABB *box, enum axis a, float ratio)
{
	cl_float3 span = vec_sub(box->max, box->min);
	Split *split = calloc(1, sizeof(Split));
	split->left = dupe_box(box);
	split->left_flex = empty_box();
	split->right = dupe_box(box);
	split->right_flex = empty_box();

	if (a == X_AXIS)
	{
		split->left->max.x = split->left->min.x + span.x * ratio;
		split->right->min.x = split->left->max.x;
	}
	if (a == Y_AXIS)
	{
		split->left->max.y = split->left->min.y + span.y * ratio;
		split->right->min.y = split->left->max.y;
	}
	if (a == Z_AXIS)
	{
		split->left->max.z = split->left->min.z + span.z * ratio;
		split->right->min.z = split->left->max.z;
	}
	return split;
}

void free_split(Split *split)
{
	if(split->left)
		free(split->left);
	if(split->right)
		free(split->right);
	if(split->left_flex)
		free(split->left_flex);
	if(split->right_flex)
		free(split->right_flex);
	free(split);
}

void print_split(Split *split)
{
	if(split->left)
	{
		printf("left rigid:\n");
		print_vec(split->left->min);
		print_vec(split->left->max);
	}
	if (split->left_flex)
	{
		printf("left flex:\n");
		print_vec(split->left_flex->min);
		print_vec(split->left_flex->max);
	}
	printf("%d\n", split->left_count);
	if (split->right)
	{
		printf("right rigid:\n");
		print_vec(split->right->min);
		print_vec(split->right->max);
	}
	if(split->right_flex)
	{
		printf("right flex:\n");
		print_vec(split->right_flex->min);
		print_vec(split->right_flex->max);
	}
	printf("%d\n", split->right_count);

	printf("%d in both\n", split->both_count);
}

float SA(AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	span = (cl_float3){fabs(span.x), fabs(span.y), fabs(span.z)};
	return 2 * span.x * span.y + 2 * span.y * span.z + 2 * span.x * span.z;
}

float SAH(Split *split, AABB *parent)
{
	return (SA(split->left_flex) * split->left_count + SA(split->right_flex) * split->right_count) / SA(parent);
}

Split *pick_best(Split **splits, AABB *parent)
{
	float min_SAH = FLT_MAX;
	int min_ind = -1;
	for (int i = 0; i < SPLIT_TEST_NUM; i++)
	{
		if (splits[i]->left_count == 0 || splits[i]->right_count == 0)
			continue ;
		float res = SAH(splits[i], parent);
		if (res < min_SAH)
		{
			min_SAH = res;
			min_ind = i;
		}
	}

	//clean up
	Split *winner = min_ind == -1 ? NULL : splits[min_ind];
	for (int i = 0; i < SPLIT_TEST_NUM; i++)
		if (i == min_ind)
			continue;
		else
			free_split(splits[i]);
	free(splits);
	return winner;
}

int x_sort(const void *arg1, const void *arg2)
{
	AABB **ap = (AABB **)arg1;
	AABB **bp = (AABB **)arg2;

	AABB *a = *ap;
	AABB *b = *bp;

	cl_float3 ca = center(a);
	cl_float3 cb = center(b);

	if (ca.x > cb.x)
		return 1;
	else if (ca.x < cb.x)
		return -1;
	else
	{
		if (a->min.x > b->min.x)
			return 1;
		else if (a->min.x < b->min.x)
			return -1;
		else
		{
			//sort needs to be fully deterministic so use pointer address as tiebreaker
			if (a > b)
				return 1;
			else if (a < b)
				return -1;
			else
				return 0;
		}
	}
}

int y_sort(const void *arg1, const void *arg2)
{
	AABB **ap = (AABB **)arg1;
	AABB **bp = (AABB **)arg2;

	AABB *a = *ap;
	AABB *b = *bp;

	cl_float3 ca = center(a);
	cl_float3 cb = center(b);


	if (ca.y > cb.y)
		return 1;
	else if (ca.y < cb.y)
		return -1;
	else
	{
		if (a->min.y > b->min.y)
			return 1;
		else if (a->min.y < b->min.y)
			return -1;
		else
		{
			if (a > b)
				return 1;
			else if (a < b)
				return -1;
			else
				return 0;
		}
	}
}

int z_sort(const void *arg1, const void *arg2)
{
	AABB **ap = (AABB **)arg1;
	AABB **bp = (AABB **)arg2;

	AABB *a = *ap;
	AABB *b = *bp;

	cl_float3 ca = center(a);
	cl_float3 cb = center(b);

	if (ca.z > cb.z)
		return 1;
	else if (ca.z < cb.z)
		return -1;
	else
	{
		if (a->min.z > b->min.z)
			return 1;
		else if (a->min.z < b->min.z)
			return -1;
		else
		{
			if (a > b)
				return 1;
			else if (a < b)
				return -1;
			else
				return 0;
		}
	}
}

float SA_overlap(Split *split)
{
	AABB *L = split->left_flex;
	AABB *R = split->right_flex;
	if (!box_in_box(L, R) && !box_in_box(R,L))
		return 0.0f;

	if (all_in(L, R))
		return SA(R);
	if (all_in(R, L))
		return SA(L);

	AABB *overlap = empty_box();

	overlap->min.x = fmax(L->min.x, R->min.x);
	overlap->min.y = fmax(L->min.y, R->min.y);
	overlap->min.z = fmax(L->min.z, R->min.z);

	overlap->max.x = fmin(L->max.x, R->max.x);
	overlap->max.y = fmin(L->max.y, R->max.y);
	overlap->max.z = fmin(L->max.z, R->max.z);

	return SA(overlap);
}
