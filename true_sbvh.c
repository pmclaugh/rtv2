#include "rt.h"

#define INF (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX}
#define NEG_INF (cl_float3){-1.0f * FLT_MAX, -1.0f * FLT_MAX, -1.0f * FLT_MAX}

#define SPLIT_TEST_NUM 32

#define LEAF_THRESHOLD 16
#define BOOST_DEPTH 11

#define ALPHA 0.001f

#define CENTER_SORT 1
#define SPATIAL_ENABLE 1

#define VERBOSE 0

enum axis{
	X_AXIS,
	Y_AXIS,
	Z_AXIS
};

cl_float3 center(const AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	return vec_add(box->min, vec_scale(span, 0.5));
}

static void push(AABB **stack, AABB *box)
{
	box->next = *stack;
	*stack = box;
}

static AABB *pop(AABB **stack)
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

typedef struct s_split
{
	AABB *left;
	AABB *left_flex;
	AABB *right;
	AABB *right_flex;
	int left_count;
	int right_count;
	int both_count;

	enum axis ax;
	float ratio;
}				Split;

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

	if (tmin > 0 && tmin < edge_t)
	{
		points[*pt_count] = vec_add(A, vec_scale(direction, tmin));
		*pt_count = *pt_count + 1;
		*res_a = *res_a + 1;
	}
	if (tmax > 0 && tmax < edge_t)
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

#define SPLIT_SPOT ((float)i + 1.0f) / (SPLIT_TEST_NUM + 1.0f) 

Split **allocate_splits(AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	enum axis ax;
	if (span.x > span.y && span.x > span.z)
		ax = X_AXIS;
	else if (span.y > span.x && span.y > span.z)
		ax = Y_AXIS;
	else
		ax = Z_AXIS;

	Split **spatials = calloc(SPLIT_TEST_NUM, sizeof(Split *));

	for (int i = 0; i < SPLIT_TEST_NUM; i++)
		spatials[i] = new_split(box, ax, SPLIT_SPOT);
	return spatials;
}

Split **new_allocate_splits(AABB *box)
{
	cl_float3 span = vec_sub(box->max, box->min);
	float total = span.x + span.y + span.z;
	int x_splits = (int)ceil((float)SPLIT_TEST_NUM * span.x / total);
	int y_splits = (int)ceil((float)SPLIT_TEST_NUM * span.y / total);
	int z_splits = SPLIT_TEST_NUM - x_splits - y_splits;

	Split **splits = calloc(SPLIT_TEST_NUM, sizeof(Split));
	int s = 0;
	for (int i = 0; i < x_splits; i++, s++)
		splits[s] = new_split(box, X_AXIS, (float)(i + 1)/(float)(x_splits + 1));
	for (int i = 0; i < y_splits; i++, s++)
		splits[s] = new_split(box, Y_AXIS, (float)(i + 1) /(float)(y_splits + 1));
	for (int i = 0; i < z_splits; i++, s++)
		splits[s] = new_split(box, Z_AXIS, (float)(i + 1) / (float)(z_splits + 1));

	return splits;

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

Split *best_spatial_split(AABB *box)
{
	Split **spatials = new_allocate_splits(box);

	//for each member, "add" to each split (dont actually make copies)
	int count = 0;
	for (int i = 0; i < SPLIT_TEST_NUM; i++)
	{
		for (AABB *b = box->members; b != NULL; b = b->next)
			if (box_in_box(b, spatials[i]->left) && box_in_box(b, spatials[i]->right) && !all_in(b, spatials[i]->right) && !all_in(b, spatials[i]->left))
			{
				AABB *lclip = dupe_box(b);
				AABB *rclip = dupe_box(b);

				clip_box(lclip, spatials[i]->left);
				clip_box(rclip, spatials[i]->right);

				flex_box(spatials[i]->left_flex, lclip);
				flex_box(spatials[i]->right_flex, rclip);

				spatials[i]->left_count++;
				spatials[i]->right_count++;
				spatials[i]->both_count++;
				
				free(lclip);
				free(rclip);
			}
			else if (all_in(b, spatials[i]->left))
			{
				flex_box(spatials[i]->left_flex, b);
				spatials[i]->left_count++;
			}
			else if (all_in(b, spatials[i]->right))
			{
				flex_box(spatials[i]->right_flex, b);
				spatials[i]->right_count++;
			}
			else
			{
				printf("member not in any box (shouldnt happen) (spatial)\n");
				print_box(b);
				print_split(spatials[i]);
				//getchar();
			}
	}
	return pick_best(spatials, box);
}

int x_sort(const void *arg1, const void *arg2)
{
	AABB **ap = (AABB **)arg1;
	AABB **bp = (AABB **)arg2;

	AABB *a = *ap;
	AABB *b = *bp;

	cl_float3 ca = center(a);
	cl_float3 cb = center(b);

	if (CENTER_SORT)
	{
		if (ca.x > cb.x)
			return 1;
		else if (ca.x < cb.x)
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
	else
	{
		if (a->min.x > b->min.x)
			return 1;
		else if (a->min.x < b->min.x)
			return -1;
		else
			return 0;
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

	if (CENTER_SORT)
	{
		if (ca.y > cb.y)
			return 1;
		else if (ca.y < cb.y)
			return -1;
		else
		{
			if (a > b)
				return 1;
			else if (a < b)
				return -1;
			else
				return 0;
		};
	}
	else
	{
		if (a->min.y > b->min.y)
			return 1;
		else if (a->min.y < b->min.y)
			return -1;
		else
			return 0;
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

	if (CENTER_SORT)
	{
		if (ca.z > cb.z)
			return 1;
		else if (ca.z < cb.z)
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
	else
	{
		if (a->min.z > b->min.z)
			return 1;
		else if (a->min.z < b->min.z)
			return -1;
		else
			return 0;
	}
}

void axis_sort(AABB **boxes, int count, enum axis ax)
{
	if (ax == X_AXIS)
		qsort(boxes, count, sizeof(AABB *), x_sort);
	else if (ax == Y_AXIS)
		qsort(boxes, count, sizeof(AABB *), y_sort);
	else if (ax == Z_AXIS)
		qsort(boxes, count, sizeof(AABB *), z_sort);
}

void make_per_axis_splits(Split **splits, int count, enum axis ax)
{
	for (int i = 0; i < count; i++)
	{
		splits[i] = calloc(1, sizeof(Split));
		splits[i]->left_flex = empty_box();
		splits[i]->right_flex = empty_box();
		splits[i]->ax = ax;
		splits[i]->ratio = (float)(i + 1) / (float)(count + 1);
	}
}


Split *new_object_split(AABB *box)
{
	//put members into an array for easier sorting
	AABB **members = calloc(box->member_count, sizeof(AABB *));
	AABB *b = box->members;
	for (int i = 0; b; b = b->next, i++)
		members[i] = b;

	//distribute splits
	cl_float3 span = vec_sub(box->max, box->min);
	float total = span.x + span.y + span.z;
	int x_splits = (int)ceil((float)SPLIT_TEST_NUM * span.x / total);
	int y_splits = (int)((float)SPLIT_TEST_NUM * span.y / total);
	int z_splits = SPLIT_TEST_NUM - x_splits - y_splits;

	//printf("%d %d %d\n", x_splits, y_splits, z_splits);

	Split **splits = calloc(SPLIT_TEST_NUM, sizeof(Split *));

	int s = 0;
	make_per_axis_splits(&splits[s], x_splits, X_AXIS);
	axis_sort(members, box->member_count, X_AXIS);
	for (int i = 0; i < box->member_count; i++)
		for (int j = 0; j < x_splits; j++)
			if ((float) i / (float)box->member_count < splits[s + j]->ratio)
			{
				flex_box(splits[s + j]->left_flex, members[i]);
				splits[s + j]->left_count++;
			}
			else
			{
				flex_box(splits[s + j]->right_flex, members[i]);
				splits[s + j]->right_count++;
			}

	s += x_splits;
	make_per_axis_splits(&splits[s], y_splits, Y_AXIS);
	axis_sort(members, box->member_count, Y_AXIS);
	for (int i = 0; i < box->member_count; i++)
		for (int j = 0; j < y_splits; j++)
			if ((float) i / (float)box->member_count < splits[s + j]->ratio)
			{
				flex_box(splits[s + j]->left_flex, members[i]);
				splits[s + j]->left_count++;
			}
			else
			{
				flex_box(splits[s + j]->right_flex, members[i]);
				splits[s + j]->right_count++;
			}

	s += y_splits;
	make_per_axis_splits(&splits[s], z_splits, Z_AXIS);
	axis_sort(members, box->member_count, Z_AXIS);
	for (int i = 0; i < box->member_count; i++)
		for (int j = 0; j < z_splits; j++)
			if ((float) i / (float)box->member_count < splits[s + j]->ratio)
			{
				flex_box(splits[s + j]->left_flex, members[i]);
				splits[s + j]->left_count++;
			}
			else
			{
				flex_box(splits[s + j]->right_flex, members[i]);
				splits[s + j]->right_count++;
			}
	free(members);
	return pick_best(splits, box);
}

int spatial_wins;
int object_wins;

float root_SA;

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

void partition(AABB *box)
{
	if (VERBOSE)
		printf("trying to partition a box with MC %d\n", box->member_count);

	Split *object = new_object_split(box);
	Split *spatial = NULL;

	if (SPATIAL_ENABLE && (!object || SA_overlap(object) / root_SA > ALPHA))
	{
		spatial = best_spatial_split(box);
		if (VERBOSE)
			printf("SAH obj %.2f SAH spatial %.2f\n", SAH(object, box), SAH(spatial, box));
	}

	if (spatial && object)
	{
		if (SAH(spatial, box) < SA(box) * box->member_count && SAH(object, box) < SA(box) * box->member_count)
		{
			printf("best not to split\n");
			return;
		}
	}

	if (spatial == NULL && object == NULL)
	{
		printf("splits failed, bailing out!\n");
		return;
	}
	else if (spatial == NULL || (object != NULL && SAH(object, box) < SAH(spatial, box)))
	{
		if (VERBOSE)
			printf("CHOSE OBJECT\n");
		object_wins++;
		AABB **members = calloc(box->member_count, sizeof(AABB *));
		AABB *b = box->members;
		for (int i = 0; b; b = b->next, i++)
			members[i] = b;

		axis_sort(members, box->member_count, object->ax);

		box->left = dupe_box(object->left_flex);
		box->right = dupe_box(object->right_flex);
		for (int i = 0; i < box->member_count; i++)
		{
			if ((float)i / (float)box->member_count < object->ratio)
			{
				push(&box->left->members, members[i]);
				box->left->member_count++;
			}
			else
			{
				push(&box->right->members, members[i]);
				box->right->member_count++;
			}
		}
		free(members);
	}
	else
	{
		if (VERBOSE)
			printf("CHOSE SPATIAL\n");
		spatial_wins++;
		box->left = dupe_box(spatial->left_flex);
		box->right = dupe_box(spatial->right_flex);

		for (AABB *b = box->members; b != NULL;)
		{
			AABB *tmp = b->next;
			if (box_in_box(b, spatial->left) && box_in_box(b, spatial->right) && !all_in(b, spatial->left) && !all_in(b, spatial->right))
			{
				AABB *lclip = dupe_box(b);
				AABB *rclip = dupe_box(b);

				clip_box(lclip, spatial->left);
				clip_box(rclip, spatial->right);

				push(&box->left->members, lclip);
				box->left->member_count++;
				push(&box->right->members, rclip);
				box->right->member_count++;

				free(b);
			}
			else if (all_in(b, spatial->left))
			{
				push(&box->left->members, b);
				box->left->member_count++;
			}
			else if (all_in(b, spatial->right))
			{
				push(&box->right->members, b);
				box->right->member_count++;
			}
			else
			{
				printf("\n\nnot in any final box, real problem (spatial)\n");
				print_box(b);
				print_split(spatial);
				print_box(box);
				getchar();
			}
			b = tmp;
		}
	}
	if (object)
		free_split(object);
	if (spatial)
		free_split(spatial);

	if (VERBOSE)
		printf("the split resulted in: L:%d, R:%d\n", box->left->member_count, box->right->member_count);
}

AABB *sbvh(Face *faces, int *box_count, int *refs)
{
	Split *test = calloc(1, sizeof(Split));

	//put all faces in AABBs
	AABB *boxes = NULL;
	int fcount = 0;
	for (Face *f = faces; f; f = f->next)
	{
		push(&boxes, box_from_face(f));
		fcount++;
	}

	printf("faces are in boxes, %d\n", fcount);

	AABB *root_box = box_from_boxes(boxes);

	root_SA = SA(root_box);

	printf("root box made\n");
	print_vec(root_box->min);
	print_vec(root_box->max);

	AABB *stack = root_box;
	int count = 1;
	int ref_count = 0;
	spatial_wins = 0;
	object_wins = 0;
	while (stack)
	{
		AABB *box = pop(&stack);
		partition(box);
		//printf("partitioned\n");
		if (box->left)
		{
			box->left->parent = box;
			box->right->parent = box;
			count += 2;
			if (box->left->member_count > LEAF_THRESHOLD)
				push(&stack, box->left);
			else
				ref_count += box->left->member_count;
			if (box->right->member_count > LEAF_THRESHOLD)
				push(&stack, box->right);
			else
				ref_count += box->right->member_count;
		}
		else
		{
			printf("chose not to split this box\n");
			print_box(box);
		}
	}
	printf("done?? %d boxes?", count);
	printf("%d member references vs %d starting\n", ref_count, root_box->member_count);
	printf("pick rates: spatial %.2f object %.2f\n", 100.0f * (float)spatial_wins / (float)(spatial_wins + object_wins), 100.0f * (float)object_wins / (float)(spatial_wins + object_wins));
	*box_count = count;
	*refs = ref_count;

	return root_box;
}

///////FLATTENING SECTION//////////

void flatten_faces(Scene *scene)
{
	//make array of Faces that's ref_count big
	Face *faces = calloc(scene->face_count, sizeof(Face));
	int face_ind = 0;
	//traverse tree, finding leaf nodes
	scene->bins->next = NULL;
	AABB *stack = scene->bins;
	AABB *box = NULL;
	//populate face array with faces in leaf nodes
	while (stack)
	{
		box = pop(&stack);
		if (box->left) // boxes always have 0 or 2 children never 1
		{
			push(&stack, box->left);
			push(&stack, box->right);
		}
		else
		{
			box->start_ind = face_ind;
			for (AABB *node = box->members; node; node = node->next)
				memcpy(&faces[face_ind++], node->f, sizeof(Face));
		}
			
	}

	scene->faces = faces;
}

gpu_bin bin_from_box(AABB *box)
{
	gpu_bin bin;

	bin.minx = box->min.x;
	bin.miny = box->min.y;
	bin.minz = box->min.z;

	bin.maxx = box->max.x;
	bin.maxy = box->max.y;
	bin.maxz = box->max.z;

	if (box->left)
	{
		bin.lind = box->left->flat_ind;
		bin.rind = box->right->flat_ind;
	}
	else
	{
		bin.lind = -3 * box->start_ind;
		bin.rind = -3 * box->member_count;
	}

	return bin;
}

gpu_bin *flatten_bvh(Scene *scene, gpu_bin **boost, int *boost_count)
{
	gpu_bin *bins = calloc(scene->bin_count, sizeof(gpu_bin));
	int bin_ind = 0;
	int boost_ind = 0;

	//do a "dummy" traversal and fill in flat_ind for each AABB

	AABB *queue_head = scene->bins;
	AABB *queue_tail = scene->bins;

	while (queue_head)
	{
		queue_head->flat_ind = bin_ind++;
		if (depth(queue_head) == BOOST_DEPTH || (!queue_head->left && depth(queue_head) < BOOST_DEPTH))
			boost_ind++;

		if (queue_head->left)
		{
			queue_head->left->next = queue_head->right;
			queue_tail->next = queue_head->left;
			queue_tail = queue_head->right;
			queue_tail->next = NULL;
		}

		queue_head = queue_head->next;
	}

	printf("bin_ind got to %d, should equal %d\n", bin_ind, scene->bin_count);

	gpu_bin *boost_arr = calloc(boost_ind, sizeof(gpu_bin));
	boost_ind = 0;

	//second pass to actually populate the gpu_bins
	queue_head = scene->bins;
	queue_tail = scene->bins;

	while (queue_head)
	{
		bins[queue_head->flat_ind] = bin_from_box(queue_head);
		if (depth(queue_head) == BOOST_DEPTH || (!queue_head->left && depth(queue_head) < BOOST_DEPTH))
		{
			boost_arr[boost_ind] = bin_from_box(queue_head);
			boost_arr[boost_ind].lind = queue_head->flat_ind;
			boost_ind++;
		}
		if (queue_head->left)
		{
			queue_head->left->next = queue_head->right;
			queue_tail->next = queue_head->left;
			queue_tail = queue_head->right;
			queue_tail->next = NULL;
		}

		queue_head = queue_head->next;
	}

	// for (int i = 0; i < scene->bin_count; i++)
	// 	printf("%d, L %d R %d\n", i, bins[i].lind, bins[i].rind);
	printf("boost is %d nodes, %d bytes\n", boost_ind, boost_ind * 32);
	*boost = boost_arr;
	*boost_count = boost_ind;

	return bins;
}