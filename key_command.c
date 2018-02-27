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
	if (key == KEY_LARR || key == KEY_RARR || key == KEY_W || (key >= KEY_A && key <= KEY_D))
	{
		if (key == KEY_LARR || key == KEY_RARR)
		{
			if (key == KEY_LARR)
				env->key.larr = 1;
			if (key == KEY_RARR)
				env->key.rarr = 1;
		}
		if (key == KEY_W || (key >= KEY_A && key <= KEY_D))
		{
			if (key == KEY_W)
				env->key.w = 1;
			if (key == KEY_A)
				env->key.a = 1;
			if (key == KEY_S)
				env->key.s = 1;
			if (key == KEY_D)
				env->key.d = 1;
		}
	}
	return 0;
}

int		key_release(int key, t_env *env)
{
	if (key == KEY_LARR || key == KEY_RARR || key == KEY_W || (key >= KEY_A && key <= KEY_D))
	{
		if (key == KEY_LARR || key == KEY_RARR)
		{
			if (key == KEY_LARR)
				env->key.larr = 0;
			if (key == KEY_RARR)
				env->key.rarr = 0;
		}
		if (key == KEY_W || (key >= KEY_A && key <= KEY_D))
		{
			if (key == KEY_W)
				env->key.w = 0;
			if (key == KEY_A)
				env->key.a = 0;
			if (key == KEY_S)
				env->key.s = 0;
			if (key == KEY_D)
				env->key.d = 0;
		}
	}
	return 0;
}

int		forever_loop(t_env *env)
{
	if (env->key.w || env->key.a || env->key.s || env->key.d)
	{
		if (env->key.w)
			env->cam.pos.x += 10;
		if (env->key.s)
			env->cam.pos.x -= 10;
		if (env->key.a)
			env->cam.pos.z += 10;
		if (env->key.d)
			env->cam.pos.z += 10;
		set_camera(&env->cam, XDIM, YDIM);
	}
	greyscale(env);
	draw_pixels(env->img, XDIM, YDIM, env->pixels);
	mlx_put_image_to_window(env->mlx, env->win, env->img, 0, 0);
	return 0;
}