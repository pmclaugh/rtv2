#include "rt.h"

#define MOVE_KEYS		(key == SDLK_w || key == SDLK_a || key == SDLK_s || key == SDLK_d || key == SDLK_SPACE || key == SDLK_LSHIFT)
#define ARR_KEYS		(key == SDLK_LEFT || key == SDLK_RIGHT || key <= SDLK_UP || key == SDLK_DOWN)
#define PRESSED_KEYS	(env->key.w || env->key.a || env->key.s || env->key.d || env->key.space || env->key.shift || env->key.larr || env->key.rarr || env->key.uarr || env->key.darr || env->key.plus || env->key.minus)

#define MOVE_SPEED	10
#define TURN_SPEED	M_PI / 60

void	print_config(t_env *env)
{
	printf("camera.position= %f, %f, %f\n", env->cam.pos.x, env->cam.pos.y, env->cam.pos.z);
	printf("camera.normal= %f, %f, %f\n", env->cam.dir.x, env->cam.dir.y, env->cam.dir.z);
}

void		exit_hook(t_env *env)
{
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

	if (env->mode == IA)
	{
		SDL_DestroyWindow(env->ia->win);
		// SDL_FreeSurface(env->ia->screen);
		free(env->ia->pixels);
		free(env->ia);
	}
	if (env->pt)
	{
		SDL_DestroyWindow(env->pt->win);
		// SDL_FreeSurface(env->pt->screen);
		free(env->pt->pixels);
		free(env->pt->total_clr);
		free(env->pt->count);
		free(env->pt);
	}
	
	free(env);
	SDL_Quit();

	exit(0);
}

void		key_press(int key, t_env *env)
{
	// printf("%d\n", key);
	if (key == SDLK_ESCAPE)
		exit_hook(env);
	if (key == SDLK_RETURN)
		env->render = 1;
	if (key == SDLK_TAB)
	{
		env->view++;
		if (env->view > 4)
			env->view = 1;
	}
	if (MOVE_KEYS || ARR_KEYS)
	{
		if (MOVE_KEYS)
		{
			if (key == SDLK_w)
				env->key.w = 1;
			if (key == SDLK_a)
				env->key.a = 1;
			if (key == SDLK_s)
				env->key.s = 1;
			if (key == SDLK_d)
				env->key.d = 1;
			if (key == SDLK_SPACE)
				env->key.space = 1;
			if (key == SDLK_LSHIFT)
				env->key.shift = 1;
		}
		if (ARR_KEYS)
		{
			if (key == SDLK_LEFT)
				env->key.larr = 1;
			if (key == SDLK_RIGHT)
				env->key.rarr = 1;
			if (key == SDLK_DOWN)
				env->key.darr = 1;
			if (key == SDLK_UP)
				env->key.uarr = 1;
		}
	}
	if (key == SDLK_KP_PLUS || key == SDLK_KP_MINUS)
	{
		if (key == SDLK_KP_PLUS)
			env->key.plus = 1;
		if (key == SDLK_KP_MINUS)
			env->key.minus = 1;
	}
	if (key == SDLK_p)
		print_config(env);
	if (key == SDLK_BACKSPACE)
		env->show_paths = (!env->show_paths) ? 1 : 0;
}

void		key_release(int key, t_env *env)
{
	if (MOVE_KEYS || ARR_KEYS)
	{
		if (MOVE_KEYS)
		{
			if (key == SDLK_w)
				env->key.w = 0;
			if (key == SDLK_a)
				env->key.a = 0;
			if (key == SDLK_s)
				env->key.s = 0;
			if (key == SDLK_d)
				env->key.d = 0;
			if (key == SDLK_SPACE)
				env->key.space = 0;
			if (key == SDLK_LSHIFT)
				env->key.shift = 0;
		}
		if (ARR_KEYS)
		{
			if (key == SDLK_LEFT)
				env->key.larr = 0;
			if (key == SDLK_RIGHT)
				env->key.rarr = 0;
			if (key == SDLK_DOWN)
				env->key.darr = 0;
			if (key == SDLK_UP)
				env->key.uarr = 0;
		}
	}
	if (key == SDLK_KP_PLUS || key == SDLK_KP_MINUS)
	{
		if (key == SDLK_KP_PLUS)
			env->key.plus = 0;
		if (key == SDLK_KP_MINUS)
			env->key.minus = 0;
	}
}

void		handle_input(t_env *env)
{
	float	interval = ((float)env->current_tick - (float)env->previous_tick) * ((float)FPS / 1000.0f);
	float	move_dist = interval * MOVE_SPEED;
	// printf("%f\n", FPS / interval); //printf fps
	if (env->mode == IA && !env->render && PRESSED_KEYS)
	{
		if (env->key.w || env->key.a || env->key.s || env->key.d)
		{
			float	move_x;
			float	move_z;

			if (env->key.w || env->key.s)
			{
				move_x = cos(env->cam.angle_x) * move_dist;
				move_z = sin(env->cam.angle_x) * move_dist;
				if (env->key.w)
				{
					env->cam.pos.x += move_x;
					env->cam.pos.z += move_z;
				}
				if (env->key.s)
				{
					env->cam.pos.x -= move_x;
					env->cam.pos.z -= move_z;
				}
			}
			if (env->key.a || env->key.d)
			{
				move_x = cos(env->cam.angle_x - (M_PI / 2)) * move_dist;
				move_z = sin(env->cam.angle_x - (M_PI / 2)) * move_dist;
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
			env->cam.pos.y += move_dist;
		if (env->key.shift)
			env->cam.pos.y -= move_dist;
		if (env->key.larr || env->key.rarr || env->key.uarr || env->key.darr)
		{
			if (env->key.larr)
				env->cam.dir = vec_rotate_xz(env->cam.dir, -TURN_SPEED * interval);
			if (env->key.rarr)
				env->cam.dir = vec_rotate_xz(env->cam.dir, TURN_SPEED * interval);
			if (env->key.uarr)
			{
				t_3x3		rot_matrix = rotation_matrix(unit_vec((cl_float3){env->cam.dir.x, 0, env->cam.dir.z}), UNIT_Z);
				cl_float3	z_aligned = mat_vec_mult(rot_matrix, env->cam.dir);
				z_aligned = vec_rotate_yz(z_aligned, -TURN_SPEED * interval);
				env->cam.dir = transposed_mat_vec_mult(rot_matrix, z_aligned);
			}
			if (env->key.darr)
			{
				t_3x3		rot_matrix = rotation_matrix(unit_vec((cl_float3){env->cam.dir.x, 0, env->cam.dir.z}), UNIT_Z);
				cl_float3	z_aligned = mat_vec_mult(rot_matrix, env->cam.dir);
				z_aligned = vec_rotate_yz(z_aligned, TURN_SPEED * interval);
				env->cam.dir = transposed_mat_vec_mult(rot_matrix, z_aligned);
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
	interactive(env);
}