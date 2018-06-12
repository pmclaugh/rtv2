#include "rt.h"

void	draw_pixels(t_sdl *sdl)
{
	Uint32 *pixels = (Uint32*)sdl->screen->pixels;
	int	r, g, b;

	SDL_LockSurface(sdl->screen);
	for (int y = 0; y < sdl->screen->h; y++)
	{
		for (int x = 0; x < sdl->screen->w; x++)
		{
			r = sdl->pixels[x + (y * sdl->screen->w)].x * 255;
			g = sdl->pixels[x + (y * sdl->screen->w)].y * 255;
			b = sdl->pixels[x + (y * sdl->screen->w)].z * 255;
			r = fmax(0.0, fmin(r, 255.0));
			g = fmax(0.0, fmin(g, 255.0));
			b = fmax(0.0, fmin(b, 255.0));
			pixels[x + (y * sdl->screen->w)] = SDL_MapRGB(sdl->screen->format, r, g, b);
		}
	}
	SDL_UnlockSurface(sdl->screen);
	SDL_UpdateWindowSurface(sdl->win);
}

void		init_sdl_pt(t_env *env)
{
	env->pt = malloc(sizeof(t_sdl));
	if (env->mode == IA)
	{
		int	pos_x, pos_y;
		SDL_GetWindowPosition(env->ia->win, &pos_x, &pos_y);
		env->pt->win = SDL_CreateWindow("CLIVE - Bidirectional Path Tracer", pos_x + DIM_IA, pos_y, DIM_PT, DIM_PT, 0);
	}
	else
		env->pt->win = SDL_CreateWindow("CLIVE - Bidirectional Path Tracer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, DIM_PT, DIM_PT, 0);

	env->pt->screen = SDL_GetWindowSurface(env->pt->win);
	SDL_SetSurfaceRLE(env->pt->screen, 1);
	env->pt->pixels = calloc(sizeof(cl_float3), (DIM_PT * DIM_PT));
	env->pt->total_clr = calloc(sizeof(cl_float3), (DIM_PT * DIM_PT));
	env->pt->count = calloc(sizeof(cl_int), (DIM_PT * DIM_PT));
}

void		init_sdl_ia(t_env *env)
{
	env->ia = malloc(sizeof(t_sdl));
	env->ia->win = SDL_CreateWindow("CLIVE - Interactive Mode", 500, 500, DIM_IA, DIM_IA, 0);
	env->ia->screen = SDL_GetWindowSurface(env->ia->win);
	SDL_SetSurfaceRLE(env->ia->screen, 1);
	env->ia->pixels = calloc(sizeof(cl_float3), (DIM_IA * DIM_IA));
	env->ia->total_clr = NULL;
	env->ia->count = NULL;
}

void		run_sdl(t_env *env)
{
	SDL_Init(SDL_INIT_VIDEO);
	
	if (env->mode == IA)
		init_sdl_ia(env);

	while (env->running) //MAIN LOOP
	{
		env->previous_tick = env->current_tick;
		env->current_tick = SDL_GetTicks();
		while (SDL_PollEvent(&env->event))
		{
			if (env->event.type == SDL_QUIT)
				exit_hook(env);
			else if (env->event.type == SDL_KEYDOWN)
				key_press(env->event.key.keysym.sym, env);
			else if (env->event.type == SDL_KEYUP)
				key_release(env->event.key.keysym.sym, env);
		}
		handle_input(env);
	}
}