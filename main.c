#include <gtk/gtk.h>
#include <cairo.h>
#include <librsvg/rsvg.h>
#include <math.h>

#define N 8
#define DEFAULT_BOX_SIZE 50

#define WHITE_SQUARE_COLOR 0.94, 0.85, 0.72
#define BLACK_SQUARE_COLOR 0.70, 0.53, 0.39

#define OPEN 0
#define ANIMATE 0

#define FPS 1000

typedef struct {
    int rank, file;
} pos_t;

typedef struct {
    int *move;
    int n_legal;
} legal_move_t;

typedef struct {
    pos_t from, to;
    int lm_start, i_lm;
    legal_move_t legal_moves[8];
} move_t;

int LEGAL_MOVES[][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}};
int N_LEGAL = 8;

int board[N][N], i_moves;
gdouble square_size;
pos_t start_pos, knight_pos;
move_t moves[N*N+1];
gboolean will_tick;

static guint min(guint a, guint b) {
    return a < b ? a : b;
}

int is_valid(pos_t from, int r_to, int f_to) {
    int r_diff, f_diff;

    r_diff = abs(r_to - from.rank);
    f_diff = abs(f_to - from.file);

    if (r_diff < 1 || f_diff < 1 || r_diff + f_diff != 3 || r_to < 0 || r_to >= N || f_to < 0 || f_to >= N)
        return 0;

    return 1;
}

int make_move(int rank, int file) {
    if (rank < 0 || file < 0)
        return 0;

    if (!is_valid(knight_pos, rank, file))
        return 0;

    moves[++i_moves].from.rank = knight_pos.rank;
    moves[i_moves].from.file = knight_pos.file;
    moves[i_moves].to.rank = rank;
    moves[i_moves].to.file = file;

    knight_pos.rank = rank;
    knight_pos.file = file;
    board[rank][file] = 1;

    return 1;
}

void undo_last_move() {
    board[moves[i_moves].to.rank][moves[i_moves].to.file] = 0;
    knight_pos.rank = moves[--i_moves].to.rank;
    knight_pos.file = moves[i_moves].to.file;
}

int calculate_n_legal(pos_t p, int *m, int d) {
    pos_t tmp;
    int n_total = 0;

    if (!(p.rank + m[0] >= 0 && p.rank + m[0] < N
        && p.file + m[1] >= 0 && p.file + m[1] < N
        && board[p.rank + m[0]][p.file + m[1]] == 0))
        return 0;

    if (d >= 1)
        return 1;

    tmp.rank = p.rank + m[0];
    tmp.file = p.file + m[1];
    board[tmp.rank][tmp.file] = 1;

    for (int i = 0; i < N_LEGAL; i++)
        n_total += calculate_n_legal(tmp, LEGAL_MOVES[i], d+1);

    board[tmp.rank][tmp.file] = 0;

    return n_total;
}

void sort_legal_moves(legal_move_t *lm_arr) {
    legal_move_t lm, tmp;
    for (int i = 1; i < N_LEGAL; i++) {
        lm = lm_arr[i];
        for (int j = i-1; j >= 0 && lm.n_legal < lm_arr[j].n_legal; j--) {
            tmp = lm_arr[j];
            lm_arr[j] = lm;
            lm_arr[j+1] = tmp;
        }
    }
}

int solve_r() {
    int i;

    for (int mi = 0; mi < N_LEGAL; mi++) {
        moves[i_moves].legal_moves[mi].move = LEGAL_MOVES[mi];
        moves[i_moves].legal_moves[mi].n_legal = calculate_n_legal(knight_pos, LEGAL_MOVES[mi], 0);
    };

    sort_legal_moves(moves[i_moves].legal_moves);

    for (i = moves[i_moves].lm_start; i < N_LEGAL; i++) {
        if (i_moves < N*N-2) {
            if (moves[i_moves].legal_moves[i].n_legal > 0) {
                break;
            }
        }
        else if (i_moves >= N*N-2){
            if ((is_valid(start_pos, knight_pos.rank + moves[i_moves].legal_moves[i].move[0], knight_pos.file + moves[i_moves].legal_moves[i].move[1])
                || OPEN) && board[knight_pos.rank + moves[i_moves].legal_moves[i].move[0]][knight_pos.file + moves[i_moves].legal_moves[i].move[1]] == 0) {
                break;
            }
        }
    }

    moves[i_moves].i_lm = i;

    // Backtrack
    if (i >= N_LEGAL) {
        undo_last_move();
        moves[i_moves].lm_start = moves[i_moves].i_lm + 1;
        return 0;
    }

    make_move(knight_pos.rank + moves[i_moves].legal_moves[i].move[0], knight_pos.file + moves[i_moves].legal_moves[i].move[1]);
    moves[i_moves].lm_start = 0;

    return 1;
}

static gboolean tick(gpointer darea) {
    if (will_tick) {
        solve_r();
        if (ANIMATE)
            gtk_widget_queue_draw(GTK_WIDGET(darea));
    }

    return i_moves < N*N-1;
}

static void make_move_click(int rank, int file) {
    if (i_moves > 0)
        return;

    moves[0].from.rank = knight_pos.rank;
    moves[0].from.file = knight_pos.file;
    moves[0].to.rank = rank;
    moves[0].to.file = file;

    knight_pos.rank = rank;
    knight_pos.file = file;
    start_pos.rank = rank;
    start_pos.file = file;

    board[rank][file] = 1;
}

static void draw_moves(cairo_t *cr) {
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_set_line_width(cr, 1);

    for (int i = 1; i <= i_moves; i++) {
        gdouble xc_from, yc_from, xc_to, yc_to;

        xc_from = (moves[i].from.file + 0.5) * square_size;
        yc_from = (moves[i].from.rank + 0.5) * square_size;
        xc_to = (moves[i].to.file + 0.5) * square_size;
        yc_to = (moves[i].to.rank + 0.5) * square_size;

        cairo_move_to(cr, xc_from, yc_from);
        cairo_line_to(cr, xc_to, yc_to);
        cairo_stroke(cr);

        cairo_arc(cr, xc_from, yc_from, square_size / 10, 0, 2*M_PI);
        cairo_fill(cr);
    }
}

static void draw_knight(cairo_t *cr, RsvgHandle *svg) {
    RsvgRectangle rect = {
        .x = knight_pos.file * square_size,
        .y = knight_pos.rank * square_size,
        .width = square_size,
        .height = square_size
    };

    if (knight_pos.rank < 0 || knight_pos.file < 0)
        return;

    rsvg_handle_render_document(svg, cr, &rect, NULL);
}

static void draw_board(cairo_t *cr) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if ((i+j) % 2 == 0)
                cairo_set_source_rgb(cr, WHITE_SQUARE_COLOR);
            else
                cairo_set_source_rgb(cr, BLACK_SQUARE_COLOR);
            cairo_rectangle(cr, j*square_size, i*square_size, square_size, square_size);
            cairo_fill(cr);
        }
    }
}

static gboolean draw(GtkWidget *darea, cairo_t *cr, gpointer knight_svg) {
    guint width, height;

    width = gtk_widget_get_allocated_width(darea);
    height = gtk_widget_get_allocated_height(darea);

    square_size = (gdouble) min(width, height) / N;

    draw_board(cr);
    draw_moves(cr);
    draw_knight(cr, RSVG_HANDLE(knight_svg));

    return FALSE;
}

static void on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer darea) {
    switch (event->keyval) {
        case GDK_KEY_space:
            if (knight_pos.rank >= 0 && knight_pos.file >= 0) {
                will_tick = !will_tick;
                if (!ANIMATE) {
                    gboolean cont = TRUE;
                    while (cont)
                        cont = tick(NULL);
                    gtk_widget_queue_draw(GTK_WIDGET(darea));
                }
            }
            break;
    }
}

static gboolean on_click(GtkWidget *widget, GdkEventButton *event, gpointer darea) {
    int r, f;

    r = event->y / square_size;
    f = event->x / square_size;

    make_move_click(r, f);

    g_print("%d %d\n", start_pos.rank, start_pos.file);

    gtk_widget_queue_draw(GTK_WIDGET(darea));

    return TRUE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window, *darea;
    RsvgHandle *knight_svg;
    GdkGeometry geometry = {
            .min_width = N*DEFAULT_BOX_SIZE,
            .min_height = N*DEFAULT_BOX_SIZE,
            .min_aspect = 1,
            .max_aspect = 1
    };

    // Set up the main window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "The Knight Tour");
    gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry, GDK_HINT_ASPECT);
    gtk_window_set_default_size(GTK_WINDOW(window), N*DEFAULT_BOX_SIZE, N*DEFAULT_BOX_SIZE);

    // Load the knight svg
    knight_svg = rsvg_handle_new_from_file("./bN.svg", NULL);

    // Set up the drawing area
    darea = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), darea);
    g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(draw), knight_svg);

    // Listen for mouse click events
    gtk_widget_add_events(window, GDK_BUTTON_RELEASE_MASK);
    g_signal_connect(G_OBJECT(window), "button-release-event", G_CALLBACK(on_click), darea);

    // Listen to key press events
    gtk_widget_add_events(window, GDK_KEY_RELEASE_MASK);
    g_signal_connect(G_OBJECT(window), "key-release-event", G_CALLBACK(on_key_press), darea);

    // Start the clock
    will_tick = FALSE;
    if (ANIMATE)
        g_timeout_add(1000/FPS, tick, darea);

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    knight_pos.rank = -1;
    knight_pos.file = -1;
    i_moves = 0;
    moves[0].lm_start = 0;

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            board[i][j] = 0;

    app = gtk_application_new(NULL, G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
