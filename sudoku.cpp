#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <unordered_set>
#include <Windows.h>

const char sample_grid_0[] = {
	" 3 6  4  "
	"       6 "
	" 6   9  8"
	"  1 26 4 "
	"3   5 7  "
	"2 6  3  1"
	" 8 19    "
	"  534   7"
	"427   9  "
};

const char sample_grid_1[] = {
	"8 2     5"
	"  4    38"
	"5  9  2  "
	"         "
	"    4 69 "
	"  5  64 2"
	"    29 6 "
	"  63   1 "
	"34 5     "
};

#define SIZE             (9*9)
#define BIT_ALL  0x1FF // 1 1111 1111
#define BIT(num) (1 << (num-1))
#define IDX(x, y)  ((y) * 9 + (x))
#define NONE 0x00 // no attribute
#define INIT 0x01 // number that placed at first
#define CURR 0x02 // new number
#define GRAY 0x04 // non number text
#define EQUL 0x10 // same numbers
#define ERRR 0x20 // error

static void su__random_num(int *outa, int *outb) {
	int a, b;
	do {
		a = rand() % 9;
		b = rand() % 9;
	} while (a == b);
	*outa = 1+a;
	*outb = 1+b;
}
static void su__random_line_in_block(int *outa, int *outb) {
	// ブロックを1つだけ選択
	int block = rand() % 3;
		
	// そのブロックの中の行（列）を二つ選択
	int la, lb;
	do {
		la = rand() % 3;
		lb = rand() % 3;
	} while (la == lb);

	*outa = block * 3 + la;
	*outb = block * 3 + lb;
}
static void su__decode(int *num, const char *s) {
	assert(num);
	assert(s);
	memset(num, 0, SIZE);
	for (int y=0; y<9; y++) {
		for (int x=0; x<9; x++) {
			char c = s[IDX(x, y)];
			if (isdigit(c)) {
				int n = c - '0';
				if (1 <= n && n <= 9) {
					num[IDX(x, y)] = n;
				}
			}
		}
	}
}
static void su__copy(int *dst, const int *src) {
	memcpy(dst, src, sizeof(int) * SIZE);
}
static void su__zero(int *dst) {
	memset(dst, 0, sizeof(int) * SIZE);
}
static void su__set_text_attr(int attr) {
	// FOREGROUND_BLUE      0x0001 // text color contains blue.
	// FOREGROUND_GREEN     0x0002 // text color contains green.
	// FOREGROUND_RED       0x0004 // text color contains red.
	// FOREGROUND_INTENSITY 0x0008 // text color is intensified.
	WORD flag = 0;
	switch (attr & 0x0F) {
	case INIT:
		flag |= FOREGROUND_GREEN|FOREGROUND_INTENSITY;
		break;
//	case CURR:
//		flag |= FOREGROUND_RED|FOREGROUND_INTENSITY;
//		break;
	case GRAY:
		flag |= FOREGROUND_INTENSITY;
		break;
	default:
		flag |= FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED;
		break;
	}
	switch (attr & 0xF0) {
	case EQUL:
		flag |= BACKGROUND_BLUE|BACKGROUND_INTENSITY;
		break;
	}

	// 直前に置いたセル。他の属性よりも優先sるう
	if (attr & CURR) {
		flag = BACKGROUND_RED|BACKGROUND_INTENSITY|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED|FOREGROUND_INTENSITY;
	}
	if (attr & ERRR) {
		flag = BACKGROUND_RED|BACKGROUND_INTENSITY|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED|FOREGROUND_INTENSITY;
	}

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), flag);
}



class CSudokuGrid {
	int m_num[SIZE];
	int m_attr[SIZE];
	int m_hint[SIZE];
	int m_lastx;
	int m_lasty;
	char m_lastmsg[256];
public:
	CSudokuGrid() {
		clear();
	}
	int get(int x, int y) const {
		assert(0 <= x && x < 9);
		assert(0 <= y && y < 9);
		return m_num[IDX(x, y)];
	}
	void set(int x, int y, int num) {
		assert(0 <= x && x < 9);
		assert(0 <= y && y < 9);
		assert(0 <= num && num <= 9);
		m_num[IDX(x, y)] = num;

		sethint_zero(x, y);
		for (int i=0; i<9; i++) {
			delhint(i, y, num);
			delhint(x, i, num);
		}
		int subx = (x / 3) * 3;
		int suby = (y / 3) * 3;
		for (int j=suby; j<suby+3; j++) {
			for (int i=subx; i<subx+3; i++) {
				delhint(i, j, num);
			}
		}
		m_lastx = x;
		m_lasty = y;
	}
	void sethint_zero(int x, int y) {
		m_hint[IDX(x, y)] = 0;
	}
	void sethint_all(int x, int y) {
		m_hint[IDX(x, y)] = BIT_ALL;
	}
	void addhint(int x, int y, int num) {
		m_hint[IDX(x, y)] |= BIT(num);
	}
	void delhint(int x, int y, int num) {
		m_hint[IDX(x, y)] &= ~BIT(num);
	}
	bool hashint(int x, int y, int num) const {
		int m = m_hint[IDX(x, y)];
		return m & BIT(num);
	}
	int ishint_unique(int x, int y, int num) const {
		int m = m_hint[IDX(x, y)];
		return m == BIT(num);
	}
	void set_how(const char *s) {
		strcpy_s(m_lastmsg, sizeof(m_lastmsg), s);
	}
	void clear() {
		m_lastx = -1;
		m_lasty = -1;
		su__zero(m_num);
		su__zero(m_attr);
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				sethint_all(x, y);
			}
		}
		set_how("");
	}
	void loadFromString(const char *s) {
		int num[SIZE];
		su__decode(num, s);
		loadFromArray(num);
	}
	void loadFromArray(int *m) {
		clear();
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				int n = m[IDX(x, y)];
				if (1 <= n && n <= 9) {
					set(x, y, n);
					m_attr[IDX(x, y)] = INIT;
				}
			}
		}
		m_lastx = -1;
		m_lasty = -1;
	}
	void print_hr(char c) const {
		for (int x=0; x<9+2; x++) {
			putchar(c);
		}
		putchar('\n');
	}
	void print_num(int x, int y) const {
		int lastn = 0;
		if (m_lastx >= 0 && m_lasty >= 0) {
			lastn = get(m_lastx, m_lasty);
		}

		int n = get(x, y);
		if (n > 0) {
			int attr = 0;
			if (n == lastn) {
				attr |= EQUL;
			}
			if (m_attr[IDX(x, y)] == INIT) {
				attr |= INIT;
			} else if (x==m_lastx && y==m_lasty) {
				attr |= CURR;
			}
			su__set_text_attr(attr);
			printf("%d", n);
			su__set_text_attr(NONE);

		} else {
			putchar(' ');
		}
	}

	void print() const {
		#define N(x, y) print_num(x, y)
		#define SEP  putchar('|')
		#define CR  putchar('\n')
		printf("+---+---+---+\n");
		SEP, N(0,0), N(1,0), N(2,0), SEP, N(3,0), N(4,0), N(5,0), SEP, N(6,0), N(7,0), N(8,0); SEP; CR;
		SEP, N(0,1), N(1,1), N(2,1), SEP, N(3,1), N(4,1), N(5,1), SEP, N(6,1), N(7,1), N(8,1); SEP; CR;
		SEP, N(0,2), N(1,2), N(2,2), SEP, N(3,2), N(4,2), N(5,2), SEP, N(6,2), N(7,2), N(8,2); SEP; CR;
		printf("+---+---+---+\n");
		SEP, N(0,3), N(1,3), N(2,3), SEP, N(3,3), N(4,3), N(5,3), SEP, N(6,3), N(7,3), N(8,3); SEP; CR;
		SEP, N(0,4), N(1,4), N(2,4), SEP, N(3,4), N(4,4), N(5,4), SEP, N(6,4), N(7,4), N(8,4); SEP; CR;
		SEP, N(0,5), N(1,5), N(2,5), SEP, N(3,5), N(4,5), N(5,5), SEP, N(6,5), N(7,5), N(8,5); SEP; CR;
		printf("+---+---+---+\n");
		SEP, N(0,6), N(1,6), N(2,6), SEP, N(3,6), N(4,6), N(5,6), SEP, N(6,6), N(7,6), N(8,6); SEP; CR;
		SEP, N(0,7), N(1,7), N(2,7), SEP, N(3,7), N(4,7), N(5,7), SEP, N(6,7), N(7,7), N(8,7); SEP; CR;
		SEP, N(0,8), N(1,8), N(2,8), SEP, N(3,8), N(4,8), N(5,8), SEP, N(6,8), N(7,8), N(8,8); SEP; CR;
		printf("+---+---+---+\n");
		#undef CR
		#undef SEP
		#undef N
		if (m_lastmsg[0]) {
			printf("[%s]\n", m_lastmsg);
		}
	}
	bool stepSolve() {
		set_how("");
		for (int y=0; y<9; y++) {
			if (step_last_cell_in_row(y)) {
				return true;
			}
		}
		for (int x=0; x<9; x++) {
			if (step_last_cell_in_col(x)) {
				return true;
			}
		}
		for (int by=0; by<3; by++) {
			for (int bx=0; bx<3; bx++) {
				if (step_last_cell_in_block(bx, by)) {
					return true;
				}
			}
		}

		for (int y=0; y<9; y++) {
			for (int n=1; n<=9; n++) {
				if (step_row_uq(y, n)) {
					return true;
				}
			}
		}
		for (int x=0; x<9; x++) {
			for (int n=1; n<=9; n++) {
				if (step_col_uq(x, n)) {
					return true;
				}
			}
		}
		for (int n=1; n<=9; n++) {
			if (step_cell_uq(n)) {
				return true;
			}
		}
		for (int suby=0; suby<3; suby++) {
			for (int subx=0; subx<3; subx++) {
				for (int n=1; n<=9; n++) {
					if (step_block_uq(subx, suby, n)) {
						return true;
					}
				}
			}
		}
		return false;
	}
	bool hasError() const {
		// 行の数字が重複している
		for (int y=0; y<9; y++) {
			int m = 0;
			for (int x=0; x<9; x++) {
				int n = get(x, y);
				if (n > 0) {
					int bit = BIT(n);
					if (m & bit) {
						return true; // 重複
					}
					m |= bit;
				}
			}
		}
		// 列の数字が重複しない
		for (int x=0; x<9; x++) {
			int m = 0;
			for (int y=0; y<9; y++) {
				int n = get(x, y);
				if (n > 0) {
					int bit = BIT(n);
					if (m & bit) {
						return true; // 重複
					}
					m |= bit;
				}
			}
		}
		// ブロック内の数字が重複しない
		for (int by=0; by<3; by++) {
			for (int bx=0; bx<3; bx++) {
				int m = 0;
				for (int y=0; y<3; y++) {
					for (int x=0; x<3; x++) {
						int n = get(bx*3+x, by*3+y);
						if (n > 0) {
							int bit = BIT(n);
							if (m & bit) {
								return true; // 重複
							}
							m |= bit;
						}
					}
				}
			}
		}
		return false;
	}
	bool isSolved() const {
		// 全てのマスに数字が入っている
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				if (get(x, y) == 0) {
					return false;
				}
			}
		}
		// 重複なし？
		if (hasError()) {
			return false;
		}
		return true;
	}
	void make() {
		loadFromString(
			"123456789"
			"456789123"
			"789123456"
			"234567891"
			"567891234"
			"891234567"
			"345678912"
			"678912345"
			"912345678"
		);
		assert(isSolved());
	}
	bool canSolve() { // 問題を解くことができる？
		CSudokuGrid grid;
		grid.loadFromArray(m_num);
		while (grid.stepSolve()) {
		}
		return grid.isSolved();
	}
	bool removeRandomOne() { // 問題が解ける状態を維持したまま、ランダムで数字を一つ消す

		// 数字が入っているセルのインデックスを並べる
		int pos[SIZE] = {0};
		int cnt = 0;
		{
			for (int i=0; i<SIZE; i++) {
				if (m_num[i] > 0) {
					pos[cnt] = i;
					cnt++;
				}
			}
		}
		// シャッフル
		for (int i=0; i<cnt*2; i++) {
			int a = rand() % cnt;
			int b = rand() % cnt;
			std::swap(pos[a], pos[b]);
		}

		// 数字を一つ消しても解けるか確認する。解けなければ次のセル数字を消してみる
		for (int i=0; i<cnt; i++) {
			// 盤面複製
			int tmp[SIZE];
			su__copy(tmp, m_num);

			// 数字を一つ消す
			int p = pos[i];
			tmp[p] = 0;

			// 解ける？
			CSudokuGrid grid;
			grid.loadFromArray(tmp);
			if (grid.canSolve()) {
				// OK. この盤面をセットする
				loadFromArray(tmp);
				m_lastx = p % 9;
				m_lasty = p / 9;
				return true;
			}
		}
		return false;
	}

	void swap_row(int y0, int y1) { // 行の入れ替え
		assert(isSolved());
		if (y0==y1) return;
		for (int x=0; x<9; x++) {
			std::swap(m_num[IDX(x, y0)], m_num[IDX(x, y1)]);
		}
		assert(isSolved());
	}
	void swap_col(int x0, int x1) { // 列の入れ替え
		assert(isSolved());
		if (x0==x1) return;
		for (int y=0; y<9; y++) {
			std::swap(m_num[IDX(x0, y)], m_num[IDX(x1, y)]);
		}
		assert(isSolved());
	}
	void swap_num(int n0, int n1) { // 数値の入れ替え
		assert(isSolved());
		if (n0==n1) return;
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				int i = IDX(x, y);
				// n0 <==> n1
				if (m_num[i] == n0) {
					m_num[i] = n1;
				} else if (m_num[i] == n1) {
					m_num[i] = n0;
				}
			}
		}
		assert(isSolved());
	}
private:
	// 行の9マスのうち8マスが既に埋まっている
	bool step_last_cell_in_row(int y) {
		// ひとつだけ未使用の数字を探す
		assert(0 <= y && y < 9);
		std::unordered_set<int> s;
		s.insert({1,2,3,4,5,6,7,8,9});
		int xx = -1;
		for (int x=0; x<9; x++) {
			int n = get(x, y);
			if (n > 0) {
				s.erase(n);
			} else {
				xx = x;
			}
		}
		if (s.size() == 1) { // ひとつだけセルが空いている。余った数字を入れる
			assert(xx >= 0);
			int n = *s.begin();
			set(xx, y, n);
			set_how("Last cell in row");
			return true;
		}
		return false;
	}
	// 列の9マスのうち8マスが既に埋まっている
	bool step_last_cell_in_col(int x) {
		assert(0 <= x && x < 9);
		// 使用済みの数字を消す
		std::unordered_set<int> s;
		s.insert({1,2,3,4,5,6,7,8,9});
		int yy = -1;
		for (int y=0; y<9; y++) {
			int n = get(x, y);
			if (n > 0) {
				s.erase(n);
			} else {
				yy = y;
			}
		}
		if (s.size() == 1) { // ひとつだけセルが空いている。余った数字を入れる
			assert(yy >= 0);
			int n = *s.begin();
			set(x, yy, n);
			set_how("Last cell in column");
			return true;
		}
		return false;
	}
	// ブロックの9マスのうち8マスが既に埋まっている
	bool step_last_cell_in_block(int subx, int suby) {
		assert(0 <= subx && subx < 3);
		assert(0 <= suby && suby < 3);
		// 使用済みの数字を消す
		std::unordered_set<int> s;
		s.insert({1,2,3,4,5,6,7,8,9});
		int xx = -1;
		int yy = -1;
		for (int y=0; y<3; y++) {
			for (int x=0; x<3; x++) {
				int cx = subx * 3 + x;
				int cy = suby * 3 + y;
				int n = get(cx, cy);
				if (n > 0) {
					s.erase(n);
				} else {
					xx = cx;
					yy = cy;
				}
			}
		}
		if (s.size() == 1) { // ひとつだけセルが空いている。余った数字を入れる
			assert(xx >= 0 && yy >= 0);
			int n = *s.begin();
			set(xx, yy, n);
			set_how("Last cell in block");
			return true;
		}
		return false;
	}
	bool step_cell_uq(int num) {
		// そのマスには num しか入らない
		assert(1 <= num && num <= 9);
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				if (ishint_unique(x, y, num)) {
					set(x, y, num);
					set_how("Uniq num");
					return true;
				}
			}
		}
		return false;
	}
	bool step_block_uq(int subx, int suby, int num) {
		assert(0 <= subx && subx < 3);
		assert(0 <= suby && suby < 3);
		assert(1 <= num && num <= 9);
		// サブブロックに入る num は一か所しかない
		int cx=-1;
		int cy=-1;
		bool found = false;
		for (int y=0; y<3; y++) {
			for (int x=0; x<3; x++) {
				int xx = subx * 3 + x;
				int yy = suby * 3 + y;
				if (hashint(xx, yy, num)) {
					if (found) {
						return false; // 重複
					}
					cx = xx;
					cy = yy;
					found = true;
				}
			}
		}
		if (found) {
			set(cx, cy, num);
			set_how("Uniq in block");
			return true;
		}
		return false;
	}
	bool step_row_uq(int y, int num) {
		assert(0 <= y && y < 9);
		assert(1 <= num && num <= 9);
		// 行に入る num は一か所しかない
		int cx=-1;
		bool found = false;
		for (int x=0; x<9; x++) {
			if (hashint(x, y, num)) {
				if (found) {
					return false; // 重複
				}
				cx = x;
				found = true;
			}
		}
		if (found) {
			set(cx, y, num);
			set_how("Uniq in row");
			return true;
		}
		return false;
	}
	bool step_col_uq(int x, int num) {
		assert(0 <= x && x < 9);
		assert(1 <= num && num <= 9);
		// 列に入る num は一か所しかない
		int cy=-1;
		bool found = false;
		for (int y=0; y<9; y++) {
			if (hashint(x, y, num)) {
				if (found) {
					return false; // 重複
				}
				cy = y;
				found = true;
			}
		}
		if (found) {
			set(x, cy, num);
			set_how("Uniq in column");
			return true;
		}
		return false;
	}
};

// 問題作る（すでにパターンがあるものとする）
void prob(CSudokuGrid grid) {
	while (1) {
		grid.print();
		printf("\n");
		printf("[1] 削除可能な数字を適当に選んで消す\n");
		printf("[0] 終了\n");
		printf("    ヒント: 1111 のように、選択肢をまとめて入力することもできます\n");
		printf(">> ");
		char s[256] = {0};
		fgets(s, 256, stdin);
		for (char *c=s; *c; c++) {
			if (*c == '1') {
				if (grid.removeRandomOne()) {
					grid.print();
				} else {
					printf("これ以上数字を消せません");
				}
			}
			if (*c == '0') {
				return;
			}
		}
	}
}

// パターン作る
void gen() {
	CSudokuGrid grid;
	grid.make();
	grid.print();
	while (1) {
		printf("\n");
		printf("[1] ランダムに選んだ数字同士を入れ替える\n");
		printf("[2] ランダムに選んだ列を入れ替える\n");
		printf("[3] ランダムに選んだ行を入れ替える\n");
		printf("[9] 問題作成モードへ\n");
		printf("[0] 終了\n");
		printf("    ヒント: 例えば 1322111 のように、選択肢をまとめて入力することもできます\n");
		printf(">> ");

		char s[256] = {0};
		fgets(s, 256, stdin);
		for (char *c=s; *c; c++) {
			if (*c == '1') {
				int a, b;
				su__random_num(&a, &b);
				printf("Num %d <==> %d\n", a, b);
				grid.swap_num(a, b);
			//	grid.print();
			}
			if (*c == '2') {
				int a, b;
				su__random_line_in_block(&a, &b);
				printf("Col %d <==> %d\n", a, b);
				grid.swap_col(a, b);
			//	grid.print();
			}
			if (*c == '3') {
				int a, b;
				su__random_line_in_block(&a, &b);
				printf("Row %d <==> %d\n", a, b);
				grid.swap_row(a, b);
			//	grid.print();
			}
			if (*c == '9') {
				prob(grid);
				return;
			}
			if (*c == '0') {
				printf("END!!");
				return;
			}
			if (*c == '\n') {
				grid.print();
			}
		}
	}
}

// 問題解く
void solve() {
	char in[256] = {0};
	memset(in, 0, sizeof(in));
	printf("数独の問題を解きます。\n");
	printf("数字の配置を１行ごと（つまり９文字ずつ）入力していってください\n");
	printf("1～9の数字以外は全て空白（数字が入っていないマス）になります\n");
	printf("行末の空白は省略できます\n");
	printf("例えば 1..3..5.. という入力と 1..3..5 は同じです（この場合は空白をドット . で表しています\n");
	printf("\n");
	printf("      +--+--+--\n");
	for (int y=0; y<9; y++) {
		char s[128] = {0};
		printf("[%d] > ", 1+y);
		fgets(s, 256, stdin);
		if (strlen(s) <= 9+1) {
			memcpy(in+y*9, s, strlen(s));
			continue;
		}
		if (strlen(s) == 0+1) {
		//	strcpy_s(in, sizeof(in), sample_grid_0);
			strcpy_s(in, sizeof(in), sample_grid_1);
			break;
		}
		printf("[ERROR!]\n");

		printf("------+--+--+--\n");
		for (int yy=0; yy<y; yy++) {
			printf("[%d] > ", y);
			for (int xx=0; xx<9; xx++) {
				printf("%c", in[yy * 9]);
			}
			printf("\n");
		}
	}

	CSudokuGrid grid;
	grid.loadFromString(in);
	grid.print();

	if (grid.hasError()) {
		su__set_text_attr(ERRR);
		printf("[ERROR!]\n");
		su__set_text_attr(NONE);
	}

	while (getchar()) {
		grid.stepSolve();
		grid.print();

		if (grid.isSolved()) {
			printf("SOLVED!!\n");
		//	break;
		}
	}
}

int main() {
	while (1) {
		printf("[1] パターンを作る\n");
		printf("[2] 問題を解く\n");
		printf("[0] 終了\n");
		printf(">> ");
		char c = getchar();
		if (c == '1') {
			getchar(); // skip \n
			gen();
			return 0;
		}
		if (c == '2') {
			getchar(); // skip \n
			solve();
			return 0;
		}
		if (c == '0') {
			getchar(); // skip \n
			return 0;
		}
	}
	return 0;
}