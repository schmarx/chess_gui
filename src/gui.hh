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

namespace chess {
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

	int w = 600;
	int h = 600;
	float block_size;

	bool start_selected = false;
	bool end_selected = false;
	Pos start;
	Pos end;

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
		int res = recv(client, buf, TCP_BUF_LEN, 0);

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
			printf("received board\n");
			binary_to_board(board, buf);
		}
	}

	void load_texture(SDL_Texture *textures[6], const char *filename, int index) {
		SDL_Surface *surface = IMG_Load(filename);
		textures[index - 1] = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_DestroySurface(surface);
	}

	void init() {
		block_size = w / 8;
		SDL_CreateWindowAndRenderer("Chess GUI", w, h, 0, &window, &renderer);
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
		int sx = x / block_size;
		int sy = 7 - (int)(y / block_size);

		return Pos(sx, sy);
	}

	void render() {
		SDL_RenderClear(renderer);

		// draw board
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				if ((x + y) % 2 == 1) SDL_SetRenderDrawColor(renderer, 255, 230, 210, 255);
				else SDL_SetRenderDrawColor(renderer, 40, 20, 10, 255);

				SDL_FRect rect = {x * block_size, (7 - y) * block_size, block_size, block_size};
				SDL_RenderFillRect(renderer, &rect);
			}
		}

		// draw selections
		if (start_selected) {
			int x = start.x;
			int y = start.y;

			SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
			SDL_FRect rect = {x * block_size, (7 - y) * block_size, block_size, block_size};
			SDL_RenderFillRect(renderer, &rect);
		}
		if (end_selected) {
			int x = end.x;
			int y = end.y;

			SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
			SDL_FRect rect = {x * block_size, (7 - y) * block_size, block_size, block_size};
			SDL_RenderFillRect(renderer, &rect);
		}

		// draw pieces
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				SDL_FRect rect = {x * block_size, (7 - y) * block_size, block_size, block_size};

				rect.x += (rect.w - rect.h / 2) / 2;
				rect.w = rect.h / 2;
				if (board[y][x].color == WHITE) SDL_RenderTexture(renderer, white_textures[board[y][x].type - 1], NULL, &rect);
				else if (board[y][x].color == BLACK) SDL_RenderTexture(renderer, black_textures[board[y][x].type - 1], NULL, &rect);
			}
		}

		SDL_RenderPresent(renderer);
	}
};

} // namespace chess