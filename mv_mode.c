#include "rt.h"

void	print_key_frames(t_env *env)
{
	Key_frame	*key_frames = env->key_frames;
	for (int i = 0; i < env->num_key_frames; i++)
	{
		printf("--------------------------------\n");
		printf("frames: %d\n", key_frames[i].frame_count);
		printf("position:\t");
		print_vec(key_frames[i].position);
		printf("direction:\t");
		print_vec(key_frames[i].direction);
		printf("translate:\t");
		print_vec(key_frames[i].translate);
		printf("rot_x: %f\nrot_y: %f\n", key_frames[i].rotate_x, key_frames[i].rotate_y);
		printf("--------------------------------\n");
	}
}

void		init_key_frames(t_env *env)
{
	t_camera	cam = env->cam;
	cl_float3	next_pos, next_dir, next_dir_hor, camera_dir_hor;
	float		inv_frame_count;
	
	for (int i = 0; i < env->num_key_frames; i++)
	{
		next_pos = env->key_frames[i].position;
		next_dir = env->key_frames[i].direction;
		inv_frame_count = 1.0f / env->key_frames[i].frame_count;

		set_camera(&cam, DIM_PT);
		env->key_frames[i].translate = vec_scale(vec_sub(next_pos, cam.pos), inv_frame_count);
		next_dir_hor = unit_vec((cl_float3){next_dir.x, 0.0f, next_dir.z});
		camera_dir_hor = unit_vec((cl_float3){cam.dir.x, 0.0f, cam.dir.z});
		env->key_frames[i].rotate_x = acos(dot(next_dir_hor, camera_dir_hor)) * inv_frame_count * ((scalar_project(next_dir, cam.d_x) >= 0) ? 1 : -1);
		camera_dir_hor = vec_rotate_xz(cam.dir, env->key_frames[i].rotate_x * env->key_frames[i].frame_count);
		env->key_frames[i].rotate_y = acos(dot(next_dir, camera_dir_hor)) * inv_frame_count * ((next_dir.y >= cam.dir.y) ? -1 : 1);
		cam.pos = next_pos;
		cam.dir = next_dir;

		if (env->key_frames[i].rotate_x != env->key_frames[i].rotate_x)
			env->key_frames[i].rotate_x = 0;
		if (env->key_frames[i].rotate_y != env->key_frames[i].rotate_y)
			env->key_frames[i].rotate_y = 0;
	}
}

void		movie_mode(t_env *env)
{
	if (env->mode == MV)
		path_tracer(env);
	else if (env->mode == PV)
		interactive(env);
	if (env->samples == 0)
	{
		if (env->key_frame < env->num_key_frames)
		{
			Key_frame	key_frame = env->key_frames[env->key_frame];
			if (env->frame < key_frame.frame_count)
			{
				env->cam.pos = vec_add(env->cam.pos, key_frame.translate);
				if (key_frame.rotate_x != 0)
					env->cam.dir = vec_rotate_xz(env->cam.dir, key_frame.rotate_x);
				if (key_frame.rotate_y != 0)
				{
					t_3x3		rot_matrix = rotation_matrix(unit_vec((cl_float3){env->cam.dir.x, 0, env->cam.dir.z}), UNIT_Z);
					cl_float3	z_aligned = mat_vec_mult(rot_matrix, env->cam.dir);
					z_aligned = vec_rotate_yz(z_aligned, key_frame.rotate_y);
					env->cam.dir = transposed_mat_vec_mult(rot_matrix, z_aligned);
				}
				env->frame++;
				env->total_frame++;
			}
			if (env->frame >= key_frame.frame_count)
			{
				env->frame = 0;
				env->key_frame++;
			}
		}
	}
	if (env->key_frame >= env->num_key_frames)
		exit_hook(env);
}