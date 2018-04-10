#include "rt.h"

void	draw_pixels(t_sdl *sdl)
{
	SDL_LockSurface(sdl->screen);
	Uint32 *pixels = (Uint32*)sdl->screen->pixels;
	for (int y = 0; y < sdl->screen->h; y++)
		for (int x = 0; x < sdl->screen->w; x++)
		{
			pixels[x + (y * sdl->screen->w)] = SDL_MapRGB(sdl->screen->format, sdl->pixels[x + (y * sdl->screen->w)].x * 255, sdl->pixels[x + (y * sdl->screen->w)].y * 255, sdl->pixels[x + (y * sdl->screen->w)].z * 255);
		}
	SDL_UnlockSurface(sdl->screen);
	SDL_UpdateWindowSurface(sdl->win);
}

void	cap_framerate(Uint32 start_tick)
{
	if (1000 / FPS > SDL_GetTicks() - start_tick)
		SDL_Delay((1000 / FPS) - (SDL_GetTicks() - start_tick));
}

void		init_sdl_pt(t_env *env)
{
	env->pt = malloc(sizeof(t_sdl));
	env->pt->win = SDL_CreateWindow("CLIVE - Path Tracer", 900, 500, DIM_PT, DIM_PT, SDL_WINDOW_INPUT_FOCUS);
	env->pt->screen = SDL_GetWindowSurface(env->pt->win);
	SDL_SetSurfaceRLE(env->pt->screen, 1);
	env->pt->pixels = calloc(sizeof(cl_float3), (DIM_PT * DIM_PT));
	env->pt->total_clr = calloc(sizeof(cl_float3), (DIM_PT * DIM_PT));
}

void		init_sdl_ia(t_env *env)
{
	env->ia = malloc(sizeof(t_sdl));
	env->ia->win = SDL_CreateWindow("CLIVE - Interactive Mode", 500, 500, DIM_IA, DIM_IA, SDL_WINDOW_INPUT_FOCUS);
	env->ia->screen = SDL_GetWindowSurface(env->ia->win);
	SDL_SetSurfaceRLE(env->ia->screen, 1);
	env->ia->pixels = calloc(sizeof(cl_float3), (DIM_IA * DIM_IA));
	env->ia->total_clr = NULL;
}

void		run_sdl(t_env *env)
{
	SDL_Init(SDL_INIT_VIDEO);
	if (env->mode == IA)
		init_sdl_ia(env);

	Uint32	start_tick = SDL_GetTicks();
	while (env->running) //MAIN LOOP
	{
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
		cap_framerate(start_tick);
	}
}