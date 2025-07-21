#include <mlx.h>
#include <stdlib.h>     // For exit() and NULL
#include <unistd.h>     // For write()

int close_window(int keycode, void *param)
{
    (void)keycode;
    (void)param;
    exit(0);
}

int main()
{
    void *mlx;
    void *win;

    // Initialize MiniLibX
    mlx = mlx_init();
    if (mlx == NULL)
    {
        write(2, "Failed to initialize MiniLibX\n", 30);
        return 1;
    }

    // Create a new window
    win = mlx_new_window(mlx, 800, 600, "MiniLibX Test");
    if (win == NULL)
    {
        write(2, "Failed to create a window\n", 26);
        return 1;
    }

    // Set a basic hook to close the window
    mlx_hook(win, 17, 0, close_window, NULL);

    // Display the window
    mlx_loop(mlx);

    return 0;
}

