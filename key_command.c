#include "rt.h"

int		exit_hook(int key, t_env *env)
{
	mlx_destroy_image(env->mlx, env->img);
	mlx_destroy_window(env->mlx, env->win);
	exit(0);
}

int		key_press(int key, t_env *env)
{
	if (key == KEY_ESC)
		exit_hook(0, env);
	if (MOVE_KEYS || ARR_KEYS)
	{
		if (MOVE_KEYS)
		{
			if (key == KEY_W)
				env->key.w = 1;
			if (key == KEY_A)
				env->key.a = 1;
			if (key == KEY_S)
				env->key.s = 1;
			if (key == KEY_D)
				env->key.d = 1;
			if (key == KEY_SPACE)
				env->key.space = 1;
			if (key == KEY_SHIFT)
				env->key.shift = 1;
		}
		if (ARR_KEYS)
		{
			if (key == KEY_LARR)
				env->key.larr = 1;
			if (key == KEY_RARR)
				env->key.rarr = 1;
			if (key == KEY_DARR)
				env->key.darr = 1;
			if (key == KEY_UARR)
				env->key.uarr = 1;
		}
	}
	return 0;
}

int		key_release(int key, t_env *env)
{
	if (MOVE_KEYS || ARR_KEYS)
	{
		if (MOVE_KEYS)
		{
			if (key == KEY_W)
				env->key.w = 0;
			if (key == KEY_A)
				env->key.a = 0;
			if (key == KEY_S)
				env->key.s = 0;
			if (key == KEY_D)
				env->key.d = 0;
			if (key == KEY_SPACE)
				env->key.space = 0;
			if (key == KEY_SHIFT)
				env->key.shift = 0;
		}
		if (ARR_KEYS)
		{
			if (key == KEY_LARR)
				env->key.larr = 0;
			if (key == KEY_RARR)
				env->key.rarr = 0;
			if (key == KEY_DARR)
				env->key.darr = 0;
			if (key == KEY_UARR)
				env->key.uarr = 0;
		}
	}
	return 0;
}

int		forever_loop(t_env *env)
{
	if (env->key.w || env->key.a || env->key.s || env->key.d || env->key.space || env->key.shift || env->key.larr || env->key.rarr || env->key.uarr || env->key.darr)
	{
		if (env->key.w || env->key.a || env->key.s || env->key.d)
		{
			float	move_x;
			float	move_z;

			if (env->key.w || env->key.s)
			{
				move_x = cos(env->cam.angle_x) * MOVE_SPEED;
				move_z = sin(env->cam.angle_x) * MOVE_SPEED;
				if (env->key.w)
				{
					// env->cam.pos = vec_add(env->cam.pos, vec_scale(env->cam.dir, MOVE_SPEED));
					env->cam.pos.x += move_x;
					env->cam.pos.z -= move_z;
				}
				if (env->key.s)
				{
					// env->cam.pos = vec_add(env->cam.pos, vec_scale(env->cam.dir, -MOVE_SPEED));
					env->cam.pos.x -= move_x;
					env->cam.pos.z += move_z;
				}
			}
			if (env->key.a || env->key.d)
			{
				move_x = cos(env->cam.angle_x - (M_PI / 2)) * MOVE_SPEED;
				move_z = sin(env->cam.angle_x - (M_PI / 2)) * MOVE_SPEED;
				if (env->key.a)
				{
					env->cam.pos.x += move_x;
					env->cam.pos.z -= move_z;
				}
				if (env->key.d)
				{
					env->cam.pos.x -= move_x;
					env->cam.pos.z += move_z;
				}
			}
		}
		if (env->key.space)
			env->cam.pos.y += MOVE_SPEED;
		if (env->key.shift)
			env->cam.pos.y -= MOVE_SPEED;
		if (env->key.larr || env->key.rarr || env->key.uarr || env->key.darr)
		{
			if (env->key.larr)
			{
				env->cam.dir = vec_rotate_xz(env->cam.dir, -TURN_SPEED);
				env->cam.angle_x -= TURN_SPEED;
			}
			if (env->key.rarr)
			{
				env->cam.dir = vec_rotate_xz(env->cam.dir, TURN_SPEED);
				env->cam.angle_x += TURN_SPEED;
			}
			if (env->key.uarr && env->cam.dir.y < 1)
			{
				env->cam.dir.y += .1;
				env->cam.dir = unit_vec(env->cam.dir);
			}
			if (env->key.darr && env->cam.dir.y > -1)
			{
				env->cam.dir.y -= .1;
				env->cam.dir = unit_vec(env->cam.dir);
			}
			while (env->cam.angle_x < 0)
				env->cam.angle_x += 2 * M_PI;
			while (env->cam.angle_x > 2 * M_PI)
				env->cam.angle_x -= 2 * M_PI;
			while (env->cam.angle_y < 0)
				env->cam.angle_y += 2 * M_PI;
			while (env->cam.angle_y > 2 * M_PI)
				env->cam.angle_y -= 2 * M_PI;
		}
		set_camera(&env->cam);
	}
	greyscale(env);
	draw_pixels(env->img, XDIM, YDIM, env->pixels);
	mlx_put_image_to_window(env->mlx, env->win, env->img, 0, 0);
	return 0;
}