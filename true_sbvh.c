#include "rt.h"

#define SPATIAL_ENABLE 1

#define VERBOSE 0

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

		if (queue_head->left)
		{
			queue_head->left->next = queue_head->right;
			queue_tail->next = queue_head->left;
			queue_tail = queue_head->right;
			queue_tail->next = NULL;
		}

		queue_head = queue_head->next;
	}

	//second pass to actually populate the gpu_bins
	queue_head = scene->bins;
	queue_tail = scene->bins;

	while (queue_head)
	{
		bins[queue_head->flat_ind] = bin_from_box(queue_head);
		if (queue_head->left)
		{
			queue_head->left->next = queue_head->right;
			queue_tail->next = queue_head->left;
			queue_tail = queue_head->right;
			queue_tail->next = NULL;
		}

		queue_head = queue_head->next;
	}

	*boost = bins;
	*boost_count = 2048 > scene->bin_count ? scene->bin_count : 2048;

	return bins;
}

gpu_bin *flatten_bvh_depth_first(Scene *scene, gpu_bin **boost, int *boost_count)
{
	gpu_bin *bins = calloc(scene->bin_count, sizeof(gpu_bin));
	int bin_ind = 0;
	int boost_ind = 0;

	//do a "dummy" traversal and fill in flat_ind for each AABB

	AABB *stack = scene->bins;
	scene->bins->next = NULL;

	while (stack)
	{
		stack->flat_ind = bin_ind++;
		AABB *popped = stack;
		stack = stack->next;
		if (popped->left)
		{
			AABB *l = popped->left;
			AABB *r = popped->right;
			r->next = stack;
			stack = r;
			l->next = stack;
			stack = l;
		}
	}

	//second pass to actually populate the gpu_bins
	stack = scene->bins;
	scene->bins->next = NULL;

	while (stack)
	{
		AABB *popped = stack;
		stack = stack->next;
		bins[stack->flat_ind] = bin_from_box(popped);
		if (popped->left)
		{
			AABB *l = popped->left;
			AABB *r = popped->right;
			r->next = stack;
			stack = r;
			l->next = stack;
			stack = l;
		}
	}

	*boost = bins;
	*boost_count = 2048 > scene->bin_count ? scene->bin_count : 2048;

	return bins;
}