#include <stdio.h>
#include <time.h> // Only used to randomize the seed
#include <Windows.h>

#define MS_BACKGROUND_WHITE (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY)
#define MS_BACKGROUND_HIGHLIGHT (BACKGROUND_RED)

#define MS_GRID_HIDDEN_BACKGROUND_EVEN MS_BACKGROUND_WHITE
#define MS_GRID_HIDDEN_BACKGROUND_ODD 0

#define MS_GRID_REVEALED_BACKGROUND BACKGROUND_INTENSITY

#define MS_GRID_MARKED_BACKGROUND (BACKGROUND_BLUE | BACKGROUND_GREEN)
#define MS_GRID_MARKED_FOREGROUND 0
#define MS_GRID_MARKED_CHAR L'X'

#define MS_V2_NULL (ms_v2){ -1, -1 }

// TODO: 
// * Fazer um sistema de seed pra gerar os mapas
// * Dar uma documentada e uma refatorada em tudo praticamente (bem no estilo da ms_grid_create que ja ta boa)
// * Tem algumas coisas meio hard-coded, dar uma ajeitada nisso adicionando mais #define e coisas do tipo
// * Mudar o nome de algumas variáveis (principalmente dentro da função ms_grid_update) q tão mto aleatórios e sem sentido ou 
//   que tão dificultando o entendimento
// * Adicionar mais umas quebras de linhas em algumas linhas muito longas, ou melhor, tentar diminuir elas dando uma abstraida melhor
// * Se pa que da pra melhorar ainda mais a eficiencia do algoritmo de achar todos os tiles vazios
// * Fazer uma interfacezinha mostrando algumas infos úteis tipo tiles restantes pra encontrar, etc...
// * Dar uma reduzida em códigos duplicados (principalmente checks feitos desnecessáriamente mais de uma vez em ifs aninhados)
// * Talvez aproveitar e usar um pouco de multithreading pra melhorar ao maximo a performace de algumas partes (principalmente a de 
//   gerar o grid com as bombas)
// * Depois que tiver tudo estável e funcionando adicionar várias coisas legaizinhas, tipo uma animaçãozinha de bomba explodindo quando
//   clica numa bomba (funcionaria +- assim: ir pintando de amarelo e laranja os tiles em formato citcular usando sen e cos), dai daria 
//   pra criar uma função tipo ms_grid_explode_tile(ms_grid *grid, ms_v2 tile_pos) e dar uma brincada
// * Tentar juntar alguns checks de if que estão separados pra diminuir a quantidade de blocos aninhados de código
// * Ajeitar o bug que da no console quando muda o tamanho da janela pra poder fazer grids muito maiores (até grids scrollaveis)
// * Implementar um bot que joga e tenta resolver o Minesweeper. Printar o highlight no tile que ele estiver presetes a revelar, pra
//   ficar mais visual o que ele ta fazendo (adiciar opção pra ir indo aos pouquinhos ou pra rushar e resolver o grid rapidão); 
//   Seria legal também se ele fosse escrevendo umas frases meio explicando cada play que ele ta fazendo, e também umas piadinhas e tal.
//   Talvez fazer o bot usando IA (eu teria que dar uma boa estudada) e não meio brute-force como provavelmente vai ser
// * Talvez fazer a melodia de I Me Mine de fundo meio que em 8-bits usando a WINAPI
// * Talvez fazer um sistema de escolha de temas pra mudar as cores e a musiquinha de fundo
// * Adicionar uma licença (copiar a mesma de algum outro projeto, como a do shdiopp)
// * Deixar o repositório no github mais organizado, mais bonito e com mais informações. Quando ficar melhor tirar ele do privado

typedef struct
{
    int x, y;
} ms_v2;

int ms_v2_compare(ms_v2 vec_a, ms_v2 vec_b)
{
    return vec_a.x == vec_b.x && vec_a.y == vec_b.y;
}

COORD ms_v2_as_coord(ms_v2 vec)
{
    return (COORD){ vec.x, vec.y };
}

typedef struct ms_node_v2 ms_node_v2;
struct ms_node_v2
{
    ms_v2 vec;
    ms_node_v2 *next;
};

typedef struct
{
    ms_node_v2 *front, *back;
} ms_queue_v2;

ms_queue_v2 *ms_queue_v2_create()
{
    ms_queue_v2 *queue = malloc(sizeof(ms_queue_v2));
    queue->front = NULL;
    queue->back = NULL;

    return queue;
}

void ms_queue_v2_destroy(ms_queue_v2 *queue)
{
    ms_node_v2 *node = queue->front;
    while (node != NULL)
    {
        ms_node_v2 *next_node = node->next;
        free(node);
        node = next_node;
    }

    free(queue);
}

int ms_queue_v2_is_empty(ms_queue_v2 *queue)
{
    return queue->front == NULL && queue->back == NULL;
}

void ms_queue_v2_enqueue(ms_queue_v2 *queue, ms_v2 vec)
{
    ms_node_v2 *node = malloc(sizeof(ms_node_v2));
    node->vec = vec;
    node->next = NULL;
    
    if (ms_queue_v2_is_empty(queue))
        queue->front = node;
    else
        queue->back->next = node;

    queue->back = node;
}

ms_v2 ms_queue_v2_dequeue(ms_queue_v2 *queue)
{
    if (!ms_queue_v2_is_empty(queue))
    {
        ms_node_v2 *node = queue->front;
        ms_v2 vec = node->vec;
        queue->front = node->next;
        free(node);

        if (queue->front == NULL)
            queue->back = NULL;

        return vec;
    }

    return MS_V2_NULL;
}

// Searches the queue beginning in the front to the back
// Returns the depth index of found element or -1 if not found
int ms_queue_v2_find(ms_queue_v2 *queue, ms_v2 vec)
{
    // QUEUE GRID FIND BENCHMARKS: 
    // --------------------------------------------------------
    // (500, 500, 22500, 1) - Singly linked queue / Normal find
    // Delta Times: 3.896 | 3.861 | 3.825 -> 3.861 ✅
    // Search Iterations: 27571342                 ❌ 
    // --------------------------------------------------------
    // (500, 500, 22500, 1) - Doubly linked queue / Normal find
    // Delta Times: 4.045 | 4.153 | 4.038 -> 4.079 ❌ 
    // Search Iterations: 27571342                 ❌
    // --------------------------------------------------------
    // (500, 500, 22500, 1) - Doubly linked queue / Reverse find
    // Delta Times: 3.992 | 3.965 | 3.949 -> 3.969 ❌
    // Search Iterations: 24930195                 ✅
    // --------------------------------------------------------

    ms_node_v2 *node = queue->front;
    for (int i = 0; node != NULL; i++)
    {
        if (ms_v2_compare(node->vec, vec))
            return i;

        node = node->next;
    }

    return -1;
}

typedef struct 
{
    int is_bomb, is_hidden, is_marked;
    int adjacent_bombs;
} ms_tile;

typedef struct 
{
    ms_tile **tiles;
    int width, height;
    int bombs;
    int hidden_count; // TODO: Se hidden_count chegar no número de bombas ganhou a partida
} ms_grid;

ms_grid *ms_grid_create(int width, int height, int bombs, unsigned int seed)
{
    ms_grid *grid = malloc(sizeof(ms_grid));
    grid->width = width;
    grid->height = height;
    grid->bombs = bombs;
    grid->hidden_count = width * height;
    grid->tiles = malloc(height * sizeof(ms_tile *));
    for (int y = 0; y < height; y++)
    {
        grid->tiles[y] = malloc(width * sizeof(ms_tile));
        for (int x = 0; x < width; x++)
            grid->tiles[y][x] = (ms_tile){ .is_bomb = 0, .is_hidden = 1, .is_marked = 0, .adjacent_bombs = 0 };
    }

    srand(seed);
    
    return grid;
}

void ms_grid_destroy(ms_grid *grid)
{
    for (int i = 0; i < grid->height; i++)
        free(grid->tiles[i]);
    free(grid->tiles);
    free(grid);
}

// TODO: Talvez mudar o nome dessa func pra ms_grid_remove_bombs ou algo do gênero pra ficar mais claro oque ela faz
void ms_grid_clear_bombs(ms_grid *grid)
{
    for (int y = 0; y < grid->height; y++)
    {
        for (int x = 0; x < grid->width; x++)
            grid->tiles[y][x] = (ms_tile){ .is_bomb = 0, .is_hidden = 1, .is_marked = grid->tiles[y][x].is_marked, .adjacent_bombs = 0 };
    }
}

// TODO: Criar umas funçõezinhas pra debug
// clock_t ms_debug_start_timer()
// {
//     return clock();
// }
// 
// void ms_debug_stop_timer(clock_t timer, ms_v2 print_pos)
// {
//     WriteConsoleOutput("Delta time: ")
// }

int ms_grid_is_valid_pos(ms_grid *grid, ms_v2 tile_pos)
{
    return tile_pos.x >= 0 && tile_pos.x < grid->width && tile_pos.y >= 0 && tile_pos.y < grid->height;
}

ms_v2 ms_grid_get_hovered_tile_pos(ms_grid *grid)
{
    HANDLE std_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_FONT_INFO font_info;
    GetCurrentConsoleFont(std_out_handle, 0, &font_info);

    CONSOLE_SCREEN_BUFFER_INFO screen_buffer_info;
    GetConsoleScreenBufferInfo(std_out_handle, &screen_buffer_info);

    POINT mouse_pos;
    GetCursorPos(&mouse_pos);
    ScreenToClient(GetConsoleWindow(), &mouse_pos);

    // TODO: Talvez retornar MS_V2_NULL se tiver fora dos limites de width e height do grid
    return (ms_v2){ (mouse_pos.x / font_info.dwFontSize.X) + screen_buffer_info.srWindow.Left, 
                    (mouse_pos.y / font_info.dwFontSize.Y) + screen_buffer_info.srWindow.Top };
}

ms_tile *ms_grid_get_tile(ms_grid *grid, ms_v2 tile_pos)
{
    return &grid->tiles[tile_pos.y][tile_pos.x];
}

CHAR_INFO ms_grid_get_tile_char_info(ms_grid *grid, ms_v2 tile_pos)
{
    CHAR_INFO char_info;
    SMALL_RECT read_region = (SMALL_RECT){ .Left = tile_pos.x, .Top = tile_pos.y, .Right = tile_pos.x, .Bottom = tile_pos.y };
    ReadConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE), &char_info, (COORD){ 1, 1 }, (COORD){ 0, 0 }, &read_region);

    return char_info;
}

// TODO: Talvez mudar o nome dessa função pra "ms_grid_draw_tile" ou algo assim pra ficar mais claro oque ela faz
void ms_grid_set_tile_char_info(ms_grid *grid, ms_v2 tile_pos, CHAR_INFO char_info)
{
    SMALL_RECT write_region = (SMALL_RECT){ .Left = tile_pos.x, .Top = tile_pos.y, .Right = tile_pos.x, .Bottom = tile_pos.y };
    WriteConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE), &char_info, (COORD){ 1, 1 }, (COORD){ 0, 0 }, &write_region);
}

// TODO: Talvez mudar o nome dessa func pra ms_grid_print_field já que é um Mine Field
void ms_grid_print_board(ms_grid *grid)
{
    for (int y = 0; y < grid->height; y++)
    {
        for (int x = 0; x < grid->width; x++)
            ms_grid_set_tile_char_info(grid, (ms_v2){ x, y }, (CHAR_INFO){ .Char.UnicodeChar = L' ', .Attributes = (x + y) % 2 == 0 ? MS_GRID_HIDDEN_BACKGROUND_EVEN : MS_GRID_HIDDEN_BACKGROUND_ODD });
    }
}

// If find_blank == 1, generates bombs on the grid until the desired tile is a blank space
// Returns the number of iterations until success
int ms_grid_generate_bombs(ms_grid *grid, ms_v2 tile_pos, int find_blank) // TODO: Precisa de algum tipo de check pra saber se é possível achar um tile vazio no grid, de acordo com quantidade de bombas e da width e height
{                                                                         // TODO: Ta precisando urgentemente de uma melhora na performace, em grids com muitas bombas ta muito lento
    ms_tile *tile = ms_grid_get_tile(grid, tile_pos);

    int tries = 0; // Number of tries
    do 
    {
        ms_grid_clear_bombs(grid);

        for (int i = 0; i < grid->bombs; i++)
        {
            // Find a random valid tile
            ms_v2 random_tile_pos;
            ms_tile *random_tile;
            do
            {
                random_tile_pos.x = rand() % grid->width;
                random_tile_pos.y = rand() % grid->height;
                random_tile = ms_grid_get_tile(grid, random_tile_pos);
            }
            while (random_tile->is_bomb); // TODO: Achar um jeito mais eficiente de não pegar randoms reptido

            // Flag the tile as a bomb tile
            random_tile->is_bomb = 1;

            // Increase adjacency count at the tiles around the bomb
            for (int y_offset = -1; y_offset <= 1; y_offset++)
            {
                for (int x_offset = -1; x_offset <= 1; x_offset++)
                {
                    ms_v2 adjacent_tile_pos = (ms_v2){ random_tile_pos.x + x_offset, random_tile_pos.y + y_offset };

                    if (ms_grid_is_valid_pos(grid, adjacent_tile_pos)) // Check if the adjacent tile is in a valid location within the grid
                    {
                        ms_tile *adjacent_tile = ms_grid_get_tile(grid, adjacent_tile_pos);

                        if (!adjacent_tile->is_bomb)
                            adjacent_tile->adjacent_bombs++;
                    }
                }
            }
        }

        tries++;
    } 
    while (find_blank && (tile->is_bomb || tile->adjacent_bombs > 0));

    return tries;
}

void ms_grid_reveal_tile(ms_grid *grid, ms_v2 tile_pos)
{
    DWORD tile_foreground_attributes[] = 
    {                                                            // Adjacent bombs:
        FOREGROUND_BLUE | FOREGROUND_INTENSITY,                  // 1 - Blue
        FOREGROUND_GREEN | FOREGROUND_INTENSITY,                 // 2 - Green
        FOREGROUND_RED | FOREGROUND_INTENSITY,                   // 3 - Red
        FOREGROUND_BLUE,                                         // 4 - Dark blue
        FOREGROUND_GREEN,                                        // 5 - Dark green
        FOREGROUND_RED,                                          // 6 - Dark red
        FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY, // 7 - Magenta
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY // 8 - Yellow                        
    };

    ms_tile *tile = ms_grid_get_tile(grid, tile_pos);
    if (tile->is_hidden)
    {
        tile->is_marked = 0;
        tile->is_hidden = 0;
        grid->hidden_count--;

        CHAR_INFO tile_new_char_info;
        tile_new_char_info.Attributes = MS_GRID_REVEALED_BACKGROUND;
        if (tile->adjacent_bombs > 0)
        {
            tile_new_char_info.Char.UnicodeChar = tile->adjacent_bombs + 48;
            tile_new_char_info.Attributes |= tile_foreground_attributes[tile->adjacent_bombs - 1];
        }
        else
            tile_new_char_info.Char.UnicodeChar = L' ';

        ms_grid_set_tile_char_info(grid, tile_pos, tile_new_char_info);
    }
}

void ms_grid_reveal_blank_space(ms_grid *grid, ms_v2 tile_pos)
{
    ms_queue_v2 *empty_tiles_queue = ms_queue_v2_create();
    ms_queue_v2_enqueue(empty_tiles_queue, tile_pos);

    while (!ms_queue_v2_is_empty(empty_tiles_queue))
    {
        ms_v2 empty_tile_pos = ms_queue_v2_dequeue(empty_tiles_queue);

        // Iterate around the empty tile, trying to find more empty tiles or reveal non-empty ones
        for (int y_offset = -1; y_offset <= 1; y_offset++)
        {
            for (int x_offset = -1; x_offset <= 1; x_offset++)
            {
                ms_v2 sub_tile_pos = (ms_v2){ empty_tile_pos.x + x_offset, empty_tile_pos.y + y_offset };

                if (ms_grid_is_valid_pos(grid, sub_tile_pos) &&   // Check if sub_tile_pos is within the grid and does not go out of bounds
                    !ms_v2_compare(empty_tile_pos, sub_tile_pos)) // Check if sub_tile isn't the middle tile, since we only want the tiles around it
                {
                    ms_tile *sub_tile = ms_grid_get_tile(grid, sub_tile_pos);

                    if (!sub_tile->is_bomb && sub_tile->is_hidden) // Tile checks to avoid unnecessary tile drawing and stuff later
                    {
                        if (sub_tile->adjacent_bombs == 0) // If sub_tile is an empty tile
                        {
                            // OBS: Using a "reverse find" reduces by a bit the number of queue search iterations, but at the end it 
                            //      gets even more resource intesive, since it is not worth managing a doubly linked queue just for it
                            if (ms_queue_v2_find(empty_tiles_queue, sub_tile_pos) == -1) // That line is funny, without it your PC will run out of memory in about 1 minute 🙃
                                ms_queue_v2_enqueue(empty_tiles_queue, sub_tile_pos);
                        }
                        else
                            ms_grid_reveal_tile(grid, sub_tile_pos);
                    }
                }
            }
        }

        ms_grid_reveal_tile(grid, empty_tile_pos);
    }

    ms_queue_v2_destroy(empty_tiles_queue);
}

// TODO: Talvez separar essa função em ms_grid_mark_tile() e ms_grid_unmark_tile()
void ms_grid_mark_tile(ms_grid *grid, ms_v2 tile_pos)
{
    ms_tile *tile = ms_grid_get_tile(grid, tile_pos);
    CHAR_INFO tile_new_char_info;
    if (!tile->is_marked) // Mark tile
    {
        tile_new_char_info.Char.UnicodeChar = MS_GRID_MARKED_CHAR;
        tile_new_char_info.Attributes = MS_GRID_MARKED_BACKGROUND | MS_GRID_MARKED_FOREGROUND;
    }
    else // Unmark tile
    {
        tile_new_char_info.Char.UnicodeChar = L' '; 
        tile_new_char_info.Attributes = (tile_pos.x + tile_pos.y) % 2 == 0 ? MS_GRID_HIDDEN_BACKGROUND_EVEN : MS_GRID_HIDDEN_BACKGROUND_ODD;
    }
    tile->is_marked = !tile->is_marked; // Flip is_marked

    ms_grid_set_tile_char_info(grid, tile_pos, tile_new_char_info);
}

// TODO: Separar melhor essa função aqui em outras funções e dar uma boa refatorada
void ms_grid_update(ms_grid *grid)
{
    int entered_new_tile = 0, holding_left_button = 0;

    ms_v2 last_hovered_tile_pos = MS_V2_NULL;
    CHAR_INFO last_hovered_tile_char_info;

    while (1)
    {
        ms_v2 hovered_tile_pos = ms_grid_get_hovered_tile_pos(grid);
        ms_tile *hovered_tile = ms_grid_get_tile(grid, hovered_tile_pos);

        // Print cursor highlight
        if (!ms_v2_compare(hovered_tile_pos, last_hovered_tile_pos))
        {
            CHAR_INFO hovered_tile_char_info = ms_grid_get_tile_char_info(grid, hovered_tile_pos);

            // TODO: Se pá que da pra melhorar isso aqui
            CHAR_INFO hovered_tile_highlight_char_info = hovered_tile_char_info;
            hovered_tile_highlight_char_info.Attributes &= ~MS_BACKGROUND_WHITE; // Remove background
            hovered_tile_highlight_char_info.Attributes |= (hovered_tile_pos.x + hovered_tile_pos.y) % 2 == 0 ? MS_BACKGROUND_HIGHLIGHT | BACKGROUND_INTENSITY : MS_BACKGROUND_HIGHLIGHT;
            ms_grid_set_tile_char_info(grid, hovered_tile_pos, hovered_tile_highlight_char_info);

            if (!ms_v2_compare(last_hovered_tile_pos, MS_V2_NULL))
                ms_grid_set_tile_char_info(grid, last_hovered_tile_pos, last_hovered_tile_char_info);
 
            last_hovered_tile_pos = hovered_tile_pos;
            last_hovered_tile_char_info = hovered_tile_char_info;

            entered_new_tile = 1;
        }
        else
            entered_new_tile = 0;
        
        if (ms_grid_is_valid_pos(grid, hovered_tile_pos)) // If hovered tile is within the grid
        {
            if (GetKeyState(VK_LBUTTON) & 0x8000) // If left button is pressed
            {   
                if (!hovered_tile->is_marked && hovered_tile->is_hidden) // If hovered tile is not marked and is hidden
                {
                    // If it is the first tile clicked, genarate bombs so that the clicked tile is blank
                    if (grid->hidden_count == grid->width * grid->height)
                    {
                        int tries = ms_grid_generate_bombs(grid, hovered_tile_pos, 1);

                        // TODO: Isso aqui ta só pra debug por enquanto
                        {
                            char tries_str[16];
                            sprintf(tries_str, "Tries: %d", tries);

                            DWORD chars_written;
                            WriteConsoleOutputCharacterA(GetStdHandle(STD_OUTPUT_HANDLE), tries_str, strlen(tries_str), (COORD){ 0, grid->height }, &chars_written);
                        }
                    }

                    if (!hovered_tile->is_bomb) // If hovered tile is not a bomb
                    {
                        if (hovered_tile->adjacent_bombs > 0)
                            ms_grid_reveal_tile(grid, hovered_tile_pos);
                        else if (hovered_tile->adjacent_bombs == 0)
                            ms_grid_reveal_blank_space(grid, hovered_tile_pos);

                        // Update last_hovered_tile_char_info with the new changes
                        last_hovered_tile_char_info = ms_grid_get_tile_char_info(grid, hovered_tile_pos);

                        // TODO: Isso aqui ta só pra debug por enquanto, pra ver alguns valores úteis
                        {
                            HANDLE std_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);

                            DWORD chars_written;

                            char revealed_str[24];
                            sprintf(revealed_str, "Revealed tiles: %d", (grid->width * grid->height) - grid->hidden_count);
                            WriteConsoleOutputCharacterA(std_out_handle, revealed_str, strlen(revealed_str), (COORD){ 16 + 1, grid->height }, &chars_written);

                            char left_str[24];
                            sprintf(left_str, "Tiles left: %d", grid->hidden_count - grid->bombs);                    
                            FillConsoleOutputCharacter(std_out_handle, L' ', sizeof(left_str), (COORD){ 16 + 1 + sizeof(revealed_str) + 1, grid->height }, &chars_written);
                            WriteConsoleOutputCharacterA(std_out_handle, left_str, strlen(left_str), (COORD){ 16 + 1 + sizeof(revealed_str) + 1, grid->height }, &chars_written);

                            // char delta_time_str[32];
                            // sprintf(delta_time_str, "Time taken: %.5lf secs", delta_time);
                            // FillConsoleOutputAttribute(std_out_handle, MS_BACKGROUND_WHITE, strlen(delta_time_str), (COORD){ 16 + 1 + sizeof(revealed_str) + 1 + sizeof(left_str) + 1, 0 }, &chars_written);
                            // WriteConsoleOutputCharacterA(std_out_handle, delta_time_str, strlen(delta_time_str), (COORD){ 16 + 1 + sizeof(revealed_str) + 1 + sizeof(left_str) + 1, 0 }, &chars_written);

                            // char search_iterations_str[64];
                            // sprintf(search_iterations_str, "Search iterations: %d", search_iterations);                    
                            // FillConsoleOutputAttribute(std_out_handle, MS_BACKGROUND_WHITE, strlen(search_iterations_str), (COORD){ 16 + 1 + sizeof(revealed_str) + 1 + sizeof(left_str) + 1, 1 }, &chars_written);
                            // WriteConsoleOutputCharacterA(std_out_handle, search_iterations_str, strlen(search_iterations_str), (COORD){ 16 + 1 + sizeof(revealed_str) + 1 + sizeof(left_str) + 1, 1 }, &chars_written);

                            // char marked_str[16];
                            // sprintf(marked_str, "Tile marked: %d", grid->tiles[hovered_tile_pos.y][hovered_tile_pos.x].is_marked);
                            // WriteConsoleOutputCharacterA(std_out_handle, marked_str, strlen(marked_str), (COORD){ 0, grid->height + 1 }, &chars_written);
                        }
                    }
                    else // If hovered tile is a bomb
                    {
                        Beep(500, 750);
                        // return; // exit(0);
                    }
                }
            }
            else if (GetKeyState(VK_RBUTTON) & 0x8000) // If right button is pressed
            {
                if (entered_new_tile || !holding_left_button)
                {
                    if (hovered_tile->is_hidden)
                    {
                        ms_grid_mark_tile(grid, hovered_tile_pos);

                        // Update last_hovered_tile_char_info with the new changes
                        last_hovered_tile_char_info = ms_grid_get_tile_char_info(grid, hovered_tile_pos);
                    }
                }

                holding_left_button = 1;
            }
            else
                holding_left_button = 0;
        }

        Sleep(10); // To reduce CPU usage
    }
}

int main()
{
    HANDLE std_in_handle = GetStdHandle(STD_INPUT_HANDLE), std_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    // Disable window resize
    // SetWindowLong(GetConsoleWindow(), GWL_STYLE, GetWindowLong(GetConsoleWindow(), GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);

    // Hide cursor
    CONSOLE_CURSOR_INFO cursor_info;
    cursor_info.bVisible = 0;
    cursor_info.dwSize = 1;
    SetConsoleCursorInfo(std_out_handle, &cursor_info);
    
    // Enable raw mode
    DWORD default_mode;
    GetConsoleMode(std_in_handle, &default_mode); 
    SetConsoleMode(std_in_handle, ENABLE_EXTENDED_FLAGS | (default_mode & ~ENABLE_QUICK_EDIT_MODE));

    // Run Minesweeper
    ms_grid *grid = ms_grid_create(20, 10, 40, time(NULL)); // 20, 10, 40, time(NULL)
    ms_grid_print_board(grid);
    ms_grid_update(grid);
    ms_grid_destroy(grid);

    // Disable raw mode
    SetConsoleMode(std_in_handle, default_mode);

    return 0;
}