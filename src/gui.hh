#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../../chess_engine/src/globals.hh"

#define CHAR_COUNT 150

namespace chess {

typedef struct {
	SDL_Texture *tx;
	int w;
	int h;
} Texture;

enum ALIGNMENT {
	ALIGN_LEFT,
	ALIGN_RIGHT,
	ALIGN_CENTER
};

class gui {
  public:
	SDL_Window *window;
	SDL_Renderer *renderer;

	int client;
	bool running = true;

	Board board;
	COLOR me;

	SDL_Texture *white_textures[6];
	SDL_Texture *black_textures[6];
	Texture *LETTERS;

	int w = 600;
	int h = 600;
	int margin = 100;
	int win_w;
	int win_h;
	float block_size;

	bool start_selected = false;
	bool end_selected = false;
	Pos start;
	Pos end;

	bool flipped = false;

	int cached_score = 0;

	bool register_listen() {
		client = socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in addr;

		addr.sin_port = htons(PORT);
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_family = AF_INET;

		int conn = connect(client, (sockaddr *)&addr, sizeof(addr));

		if (conn < 0) {
			printf("err\n");
			return false;
		}

		int buf[TCP_BUF_LEN];
		int res = recv(client, buf, TCP_BUF_LEN, 0);
		me = (COLOR)buf[0];

		printf("playing as %s\n", color_codes[me]);

		fcntl(client, F_SETFL, O_NONBLOCK);

		return true;

		// printf("continue?");
		// char c;
		// scanf("%c", &c);

		// const char *msg = "sending message beep boop";
		// send(client, msg, strlen(msg), 0);
		// printf("sending \"%s\" of length %li\n", msg, strlen(msg));
	}

	void listen() {
		int buf[TCP_BUF_LEN];
		int res = recv(client, buf, sizeof(packet), 0);

		if (res == 0) {
			// inactive connection
			printf("connection lost\n");
			running = false;

		} else if (res < 0) {
			// error occured

			if (no_msg()) {
				// the error is only signifying that there are no packets to poll for
			} else {
				printf("err\n");
			}
		} else {
			// a response consisting of res bytes is received
			printf("received board (%i bits)\n", res);
			packet_to_board(board, *((packet *)buf));
			cached_score = board.get_score();
		}
	}
	}

	void load_texture(SDL_Texture *textures[6], const char *filename, int index) {
		SDL_Surface *surface = IMG_Load(filename);
		textures[index - 1] = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_DestroySurface(surface);
	}

	void render_text(int pos_x, int pos_y, int align, int offset_y, Texture font[], const char *text, ...) {

		char render_string[150];
		va_list args;
		va_start(args, text);
		vsprintf(render_string, text, args);
		va_end(args);

		int offset_x = 0;
		if (align == ALIGN_RIGHT) {
			for (size_t i = 0; i < strlen(render_string); i++) {
				offset_x -= font[(int)render_string[i]].w;
			}
		} else if (align == ALIGN_CENTER) {
			for (size_t i = 0; i < strlen(render_string); i++) {
				offset_x -= font[(int)render_string[i]].w / 2;
			}
		}

		for (size_t i = 0; i < strlen(render_string); i++) {
			Texture l = font[(int)render_string[i]];
			SDL_FRect r = {pos_x + offset_x, pos_y - offset_y * font['a'].h, l.w, l.h};
			offset_x += l.w;

			SDL_RenderTexture(renderer, l.tx, NULL, &r);
		}
	}

	void init_font() {
		LETTERS = (Texture *)malloc(sizeof(Texture) * CHAR_COUNT);

		TTF_Init();
		TTF_Font *font_normal = TTF_OpenFont("assets/slkscr.ttf", 32);

		SDL_Color color = {255, 255, 255, 255};
		for (int i = 1; i < CHAR_COUNT; i++) {
			char letter[10];
			sprintf(letter, "%c", i);

			SDL_Surface *surface = TTF_RenderText_Solid(font_normal, letter, 0, color);
			SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

			LETTERS[i].tx = texture;
			LETTERS[i].w = surface->w;
			LETTERS[i].h = surface->h;

			SDL_DestroySurface(surface);
		}

		TTF_CloseFont(font_normal);
	}

	void init() {
		block_size = w / 8;
		win_w = w + 2 * margin;
		win_h = h + 2 * margin;

		SDL_CreateWindowAndRenderer("Chess GUI", win_w, win_h, 0, &window, &renderer);
		SDL_SetDefaultTextureScaleMode(renderer, SDL_SCALEMODE_NEAREST);

		load_texture(black_textures, "assets/image/pawn_black.png", PIECE_PAWN);
		load_texture(black_textures, "assets/image/rook_black.png", PIECE_ROOK);
		load_texture(black_textures, "assets/image/knight_black.png", PIECE_KNIGHT);
		load_texture(black_textures, "assets/image/bishop_black.png", PIECE_BISHOP);
		load_texture(black_textures, "assets/image/queen_black.png", PIECE_QUEEN);
		load_texture(black_textures, "assets/image/king_black.png", PIECE_KING);

		load_texture(white_textures, "assets/image/pawn_white.png", PIECE_PAWN);
		load_texture(white_textures, "assets/image/rook_white.png", PIECE_ROOK);
		load_texture(white_textures, "assets/image/knight_white.png", PIECE_KNIGHT);
		load_texture(white_textures, "assets/image/bishop_white.png", PIECE_BISHOP);
		load_texture(white_textures, "assets/image/queen_white.png", PIECE_QUEEN);
		load_texture(white_textures, "assets/image/king_white.png", PIECE_KING);

		init_font();
	}

	void run() {
		while (running) {
			listen();
			events();
			render();
		}

		close_gui();
	}

	void close_gui() {
		printf("shutting down\n");
		SDL_Quit();
		running = false;
	}

	void events() {
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0) {
			if (event.type == SDL_EVENT_QUIT) {
				running = 0;
				break;
			} else if (event.type == SDL_EVENT_KEY_DOWN) {
				switch (event.key.key) {
				case SDLK_F:
					flipped = !flipped;
					break;
				case SDLK_Q:
				case SDLK_ESCAPE:
					running = false;
					break;

				default:
					break;
				}
			} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
				Pos pos = get_pos(event.button.x, event.button.y);
				printf("%i, %i\n", pos.x, pos.y);
				if (!start_selected) {
					if (board[pos.y][pos.x].color != NONE) {
						start = pos;
						start_selected = true;
						end_selected = false;
					}
				} else {
					if (pos.x == start.x && pos.y == start.y) {
						start_selected = false;
						end_selected = false;
					} else {
						end_selected = true;
						end = pos;

						send_move(Move(start, end));
					}
				}
			}
		}
	}

	void send_move(Move move) {
		char buf[20];
		move.to_string(buf);

		start_selected = false;
		end_selected = false;
		send(client, buf, 20, 0);
	}

	Pos get_pos(int x, int y) {
		x -= margin;
		y -= margin;

		int sx = x / block_size;
		int sy = 7 - (int)(y / block_size);

		if (flipped) {
			sx = 7 - (int)(x / block_size);
			sy = y / block_size;
		}

		return Pos(sx, sy);
	}

	SDL_FRect get_square_rect(int x, int y) {

		SDL_FRect rect = {x * block_size, (7 - y) * block_size, block_size, block_size};
		if (flipped) {
			rect.x = (7 - x) * block_size;
			rect.y = y * block_size;
		}

		rect.x += margin;
		rect.y += margin;
		return rect;
	}

	void draw_board() {
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				if ((x + y) % 2 == 1) SDL_SetRenderDrawColor(renderer, 245, 220, 190, 255);
				else SDL_SetRenderDrawColor(renderer, 80, 40, 20, 255);

				SDL_FRect rect = get_square_rect(x, y);
				SDL_RenderFillRect(renderer, &rect);
			}
		}
	}

	void draw_selections() {
		if (start_selected) {
			int x = start.x;
			int y = start.y;

			SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
			SDL_FRect rect = get_square_rect(x, y);
			SDL_RenderFillRect(renderer, &rect);
		}
		if (end_selected) {
			int x = end.x;
			int y = end.y;

			SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
			SDL_FRect rect = get_square_rect(x, y);
			SDL_RenderFillRect(renderer, &rect);
		}

		SDL_SetRenderDrawColor(renderer, 0, 255, 255, 200);
	}

	void draw_pieces() {
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				SDL_FRect rect = get_square_rect(x, y);

				rect.x += (rect.w - rect.h / 2) / 2;
				rect.w = rect.h / 2;

				if (board[y][x].color == WHITE) SDL_RenderTexture(renderer, white_textures[board[y][x].type - 1], NULL, &rect);
				else if (board[y][x].color == BLACK) SDL_RenderTexture(renderer, black_textures[board[y][x].type - 1], NULL, &rect);
			}
		}
	}

	void draw_ui() {
		render_text(0, 0, ALIGN_LEFT, 0, LETTERS, "%s's turn", color_codes[board.player_turn]);
		render_text(0, 0, ALIGN_LEFT, -1, LETTERS, "score: %i", cached_score);
	}

	void render() {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		draw_board();
		draw_selections();
		draw_pieces();
		draw_ui();

		SDL_RenderPresent(renderer);
	}
};

} // namespace chess