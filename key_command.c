#include "rt.h"

#define KEY_ESC		53
#define KEY_W		13
#define KEY_A		0
#define KEY_S		1
#define KEY_D		2
#define KEY_LARR	123
#define KEY_RARR	124
#define KEY_DARR	125
#define KEY_UARR	126
#define KEY_SPACE	49
#define KEY_SHIFT	257
#define KEY_TAB		48
#define KEY_F		3
#define KEY_ENTER	36
#define KEY_PLUS	24
#define KEY_MINUS	27

#define MOVE_KEYS		(key == KEY_W || (key >= KEY_A && key <= KEY_D) || key == KEY_SPACE || key == KEY_SHIFT)
#define ARR_KEYS		(key >= KEY_LARR  && key <= KEY_UARR)
#define PRESSED_KEYS	(env->key.w || env->key.a || env->key.s || env->key.d || env->key.space || env->key.shift || env->key.larr || env->key.rarr || env->key.uarr || env->key.darr || env->key.plus || env->key.minus)

#define MOVE_SPEED	10
#define TURN_SPEED	M_PI / 30

int		exit_hook(int key, t_env *env)
{
	if (key == KEY_ESC)
	{
		if (env->mode == IA)
		{
			mlx_destroy_image(env->mlx, env->ia->img);
			mlx_destroy_window(env->mlx, env->ia->win);
			free(env->ia->pixels);
			if (env->ia->total_clr != NULL)
				free(env->ia->total_clr);
			free(env->ia);
		}
		mlx_destroy_image(env->mlx, env->pt->img);
		mlx_destroy_window(env->mlx, env->pt->win);
		for (int i = 0; i < env->scene->mat_count; i++)
		{
			if (env->scene->materials[i].friendly_name != NULL)
				free(env->scene->materials[i].friendly_name);
			if (env->scene->materials[i].map_Ka_path != NULL)
				free(env->scene->materials[i].map_Ka_path);
			if (env->scene->materials[i].map_Ka != NULL)
			{
				free(env->scene->materials[i].map_Ka->pixels);
				free(env->scene->materials[i].map_Ka);
			}
			if (env->scene->materials[i].map_Kd_path != NULL)
				free(env->scene->materials[i].map_Kd_path);
			if (env->scene->materials[i].map_Kd != NULL)
			{
				free(env->scene->materials[i].map_Kd->pixels);
				free(env->scene->materials[i].map_Kd);
			}
			if (env->scene->materials[i].map_bump_path != NULL)
				free(env->scene->materials[i].map_bump_path);
			if (env->scene->materials[i].map_bump != NULL)
			{
				free(env->scene->materials[i].map_bump->pixels);
				free(env->scene->materials[i].map_bump);
			}
			if (env->scene->materials[i].map_d_path != NULL)
				free(env->scene->materials[i].map_d_path);
			if (env->scene->materials[i].map_d != NULL)
			{
				free(env->scene->materials[i].map_d->pixels);
				free(env->scene->materials[i].map_d);
			}
			if (env->scene->materials[i].map_Ks_path != NULL)
				free(env->scene->materials[i].map_Ks_path);
			if (env->scene->materials[i].map_Ks != NULL)
			{
				free(env->scene->materials[i].map_Ks->pixels);
				free(env->scene->materials[i].map_Ks);
			}
			if (env->scene->materials[i].map_Ke_path != NULL)
				free(env->scene->materials[i].map_Ke_path);
			if (env->scene->materials[i].map_Ke != NULL)
			{
				free(env->scene->materials[i].map_Ke->pixels);
				free(env->scene->materials[i].map_Ke);
			}
		}
		free(env->scene->materials);
		free(env->scene->faces);
		free(env->scene);
		free(env->pt->pixels);
		free(env->pt->total_clr);
		free(env->pt);
		free(env);
		exit(0);
	}
	return 0;
}

int		key_press(int key, t_env *env)
{
	// printf("%d\n", key);
	if (key == KEY_ESC)
		exit_hook(key, env);
	if (key == KEY_ENTER)
		env->render = 1;
	else if (key == KEY_TAB)
	{
		env->view++;
		if (env->view > 4)
			env->view = 1;
	}
	else if (key == KEY_F)
		env->show_fps = (!env->show_fps) ? 1 : 0;
	else if (MOVE_KEYS || ARR_KEYS)
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
	else if (key == KEY_PLUS || key == KEY_MINUS)
	{
		if (key == KEY_PLUS)
			env->key.plus = 1;
		if (key == KEY_MINUS)
			env->key.minus = 1;
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
	else if (key == KEY_PLUS || key == KEY_MINUS)
	{
		if (key == KEY_PLUS)
			env->key.plus = 0;
		if (key == KEY_MINUS)
			env->key.minus = 0;
	}
	return 0;
}

int		forever_loop(t_env *env)
{
	if (env->mode == IA && !env->render && PRESSED_KEYS)
	{
		if (env->key.w || env->key.a || env->key.s || env->key.d)
		{
			float	move_x;
			float	move_z;

			if (env->key.w || env->key.s)
			{
				// move_x = cos(env->cam.angle_x) * MOVE_SPEED;
				// move_z = sin(env->cam.angle_x) * MOVE_SPEED;
				if (env->key.w)
				{
					env->cam.pos = vec_add(env->cam.pos, vec_scale(env->cam.dir, MOVE_SPEED));
					// env->cam.pos.x += move_x;
					// env->cam.pos.z -= move_z;
				}
				if (env->key.s)
				{
					env->cam.pos = vec_add(env->cam.pos, vec_scale(env->cam.dir, -MOVE_SPEED));
					// env->cam.pos.x -= move_x;
					// env->cam.pos.z += move_z;
				}
			}
			if (env->key.a || env->key.d)
			{
				move_x = cos(env->cam.angle_x - (M_PI / 2)) * MOVE_SPEED;
				move_z = sin(env->cam.angle_x - (M_PI / 2)) * MOVE_SPEED;
				if (env->key.a)
				{
					env->cam.pos.x -= move_x;
					env->cam.pos.z -= move_z;
				}
				if (env->key.d)
				{
					env->cam.pos.x += move_x;
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
				env->cam.dir = vec_rotate_xz(env->cam.dir, -TURN_SPEED);
			if (env->key.rarr)
				env->cam.dir = vec_rotate_xz(env->cam.dir, TURN_SPEED);
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
		if (env->key.plus || env->key.minus)
		{
			if (env->key.plus)
				env->eps *= 1.1;
			if (env->key.minus)
				env->eps /= 1.1;
		}
	}
	if (env->render && env->samples < env->spp)
		path_tracer(env);
	else if (env->mode == IA)
		interactive(env);
	return 0;
}
