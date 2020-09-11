#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <unordered_set>
#include <Windows.h>

const char su_SampleGridA[] = {
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

const char su_SampleGridB[] = {
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

static const int SU_SIZE = 9 * 9; // マスの数
static const int SU_BIT_ALL = 0x1FF; // 1 1111 1111
static int su_Bit(int num) {
	return 1 << (num-1);
}
static int su_IndexOf(int x, int y) {
	return y * 9 + x;
}

enum TEXTATTR_ {
	TEXTATTR_NONE = 0x00, // no attribute
	TEXTATTR_INIT = 0x01, // number that placed at first
	TEXTATTR_CUR  = 0x02, // new number
	TEXTATTR_GRAY = 0x04, // non number text
	TEXTATTR_EQUL = 0x10, // same numbers
	TEXTATTR_ERR  = 0x20, // error
};
typedef int TEXTATTRS;

// １～９の範囲で、重複しない二つの数字を選ぶ
static void su_GetRandomIntPair(int *outa, int *outb) {
	int a, b;
	do {
		a = rand() % 9;
		b = rand() % 9;
	} while (a == b);
	*outa = 1+a;
	*outb = 1+b;
}

// 同じブロックにある二つの行を重複せずに選ぶ
static void su_GetRandomLinePair(int *outa, int *outb) {
	// ブロックを1つだけ選択
	int block = rand() % 3;
	
	// そのブロックの中の行（列）を二つ選択
	// ※３行のうちの２行を順不同で選ぶということは、無関係な１行を選ぶのと同じ（残りの２行が自動的に重複なしの行になる）なので
	// 　別に while でやる必要もないのだが、なんとなく。
	int la, lb;
	do {
		la = rand() % 3;
		lb = rand() % 3;
	} while (la == lb);

	*outa = block * 3 + la;
	*outb = block * 3 + lb;
}

// １～９の数字の羅列からなる文字列 str を指定して、数字配列 result を得る
static void su_ImportNumbers(int *result, const char *str) {
	//
	assert(result);
	assert(str);
	memset(result, 0, sizeof(int) * SU_SIZE);

	int x = 0;
	int y = 0;
	for (const char *c=str; *c!='\0' && y<9; c++) {
		if (*c == '\n') { // 改行があったら残りのマスをスキップして次の行へ
			y++;
			x = 0;
			continue;
		}
		if (x < 9) {
			if (isdigit(*c)) { // 数字があったらその数字を入れる
				int n = *c - '0';
				if (1 <= n && n <= 9) {
					result[su_IndexOf(x, y)] = n;
				}
				x++;
			} else { // それ以外の文字だったら空白のままにする
				x++;
			}
		}
	}
}
static void su_Copy(int *dst, const int *src) {
	memcpy(dst, src, sizeof(int) * SU_SIZE);
}
static void su_ZeroClear(int *dst) {
	memset(dst, 0, sizeof(int) * SU_SIZE);
}
static void su_SetConsoleTextAttr(TEXTATTRS attr) {
	// FOREGROUND_BLUE      0x0001 // text color contains blue.
	// FOREGROUND_GREEN     0x0002 // text color contains green.
	// FOREGROUND_RED       0x0004 // text color contains red.
	// FOREGROUND_INTENSITY 0x0008 // text color is intensified.
	WORD flag = 0;
	switch (attr & 0x0F) {
	case TEXTATTR_INIT:
		flag |= FOREGROUND_GREEN|FOREGROUND_INTENSITY;
		break;
//	case TEXTATTR_CUR:
//		flag |= FOREGROUND_RED|FOREGROUND_INTENSITY;
//		break;
	case TEXTATTR_GRAY:
		flag |= FOREGROUND_INTENSITY;
		break;
	default:
		flag |= FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED;
		break;
	}
	switch (attr & 0xF0) {
	case TEXTATTR_EQUL:
		flag |= BACKGROUND_BLUE|BACKGROUND_INTENSITY;
		break;
	}

	// 直前に置いたセル。他の属性よりも優先する
	if (attr & TEXTATTR_CUR) {
		flag = BACKGROUND_RED|BACKGROUND_INTENSITY|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED|FOREGROUND_INTENSITY;
	}
	if (attr & TEXTATTR_ERR) {
		flag = BACKGROUND_RED|BACKGROUND_INTENSITY|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED|FOREGROUND_INTENSITY;
	}

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), flag);
}



class CSudokuGrid {
	int m_num[SU_SIZE];
	int m_attr[SU_SIZE];
	int m_hint[SU_SIZE];
	int m_lastx;
	int m_lasty;
	char m_lastmsg[256];
public:
	CSudokuGrid() {
		clear();
	}

	// 指定マスに入っている数字を得る
	// まだ数字が入っていない場合は 0 を返す
	int get(int x, int y) const {
		assert(0 <= x && x < 9);
		assert(0 <= y && y < 9);
		return m_num[su_IndexOf(x, y)];
	}

	// 指定マスに数字を入れる（このマスに入る数字が確定した）
	void set(int x, int y, int num) {
		assert(0 <= x && x < 9);
		assert(0 <= y && y < 9);
		assert(0 <= num && num <= 9);
		m_num[su_IndexOf(x, y)] = num;

		// 数字が確定したので、このマスのヒントを消す
		setHintZero(x, y);

		// 同じ列、同じ行にあるある他のマスに num が入らないことが確定した
		for (int i=0; i<9; i++) {
			removeHint(i, y, num); // 横１列分のマスのヒントから num を消す
			removeHint(x, i, num); // 縦１列分のマスのヒントから num を消す
		}

		// 同じ3x3ブロックにある他のマスに num が入らないことが確定した
		int subx = (x / 3) * 3;
		int suby = (y / 3) * 3;
		for (int j=suby; j<suby+3; j++) {
			for (int i=subx; i<subx+3; i++) {
				removeHint(i, j, num); // 3x3ブロック内のマスのヒントから num を消す
			}
		}

		// 最後に確定したマスを記録しておく（画面上で強調表示するため）
		m_lastx = x;
		m_lasty = y;
	}

	// 指定マスのヒント（このマスに入るべき数字の候補）をリセットする
	void setHintZero(int x, int y) {
		m_hint[su_IndexOf(x, y)] = 0;
	}

	// 指定マスに１～９すべてのヒントを入れる（このマスには１～９のどれもが入る可能性がある、という印）
	void setHintAll(int x, int y) {
		m_hint[su_IndexOf(x, y)] = SU_BIT_ALL;
	}

	// ヒントを追加する（このマスに入る可能性のある数字を追加する）
	void addHint(int x, int y, int num) {
		m_hint[su_IndexOf(x, y)] |= su_Bit(num);
	}

	// ヒントを削除する（このマスに数字 num が入る可能性がなくなった）
	void removeHint(int x, int y, int num) {
		m_hint[su_IndexOf(x, y)] &= ~su_Bit(num);
	}

	// 指定マスにヒント数字 num が入っているか（このマスに数字 num が入る可能性がるか）
	bool hasHint(int x, int y, int num) const {
		int m = m_hint[su_IndexOf(x, y)];
		return m & su_Bit(num);
	}

	// 指定マス入る可能性のある数字は num しかないか（このマスにはいる可能性のある数字が num しかない＝確定できる）
	int isHintUnique(int x, int y, int num) const {
		int m = m_hint[su_IndexOf(x, y)];
		return m == su_Bit(num);
	}

	// 説明テキストの設定
	void setHow(const char *s) {
		strcpy_s(m_lastmsg, sizeof(m_lastmsg), s);
	}

	// 盤面をリセットする（すべてのマスが空っぽになる）
	void clear() {
		m_lastx = -1;
		m_lasty = -1;
		su_ZeroClear(m_num);
		su_ZeroClear(m_attr);
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				setHintAll(x, y);
			}
		}
		setHow("");
	}

	// 盤面をロードする
	// 文字列 s は必ず 9x9=81 文字以上ないといけない
	// 数字が入るべき場所にはその数字が、空っぽのマスには数字以外の文字が入っているものとする
	// 例:
	//	loadFromString(
	//		"123456789"
	//		"456789123"
	//		"789123456"
	//		"234567891"
	//		"567891234"
	//		"891234567"
	//		"345678912"
	//		"678912345"
	//		"912345678"
	//	);
	void loadFromString(const char *s) {
		int num[SU_SIZE];
		su_ImportNumbers(num, s);
		loadFromArray(num);
	}

	// 盤面をロードする
	// num には 9x9=81 個の要素を持つ配列を指定する。
	// それぞれの要素は 0～9 の整数が入っている。0はそのマスが空っぽであることを示す
	void loadFromArray(const int *num) {
		clear();
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				int n = num[su_IndexOf(x, y)];
				if (1 <= n && n <= 9) {
					set(x, y, n);
					m_attr[su_IndexOf(x, y)] = TEXTATTR_INIT;
				}
			}
		}
		m_lastx = -1;
		m_lasty = -1;
	}

	// 指定マスの数字をプリント
	void printNum(int x, int y) const {
		int lastn = 0;
		if (m_lastx >= 0 && m_lasty >= 0) {
			lastn = get(m_lastx, m_lasty);
		}

		int n = get(x, y);
		if (n > 0) {
			int attr = 0;
			if (n == lastn) {
				attr |= TEXTATTR_EQUL;
			}
			if (m_attr[su_IndexOf(x, y)] == TEXTATTR_INIT) {
				attr |= TEXTATTR_INIT;
			} else if (x==m_lastx && y==m_lasty) {
				attr |= TEXTATTR_CUR;
			}
			su_SetConsoleTextAttr(attr);
			printf("%d", n);
			su_SetConsoleTextAttr(TEXTATTR_NONE);

		} else {
			putchar(' ');
		}
	}

	// 数字の並びをプリント
	void print() const {
		#define N(x, y) printNum(x, y)
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

	// 問題解決の手順を１段階だけ進める
	bool stepSolve() {
		setHow("");
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

	// どこかダメな点があるか？
	bool hasError() const {
		// 行の数字が重複している
		for (int y=0; y<9; y++) {
			int m = 0;
			for (int x=0; x<9; x++) {
				int n = get(x, y);
				if (n > 0) {
					int bit = su_Bit(n);
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
					int bit = su_Bit(n);
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
							int bit = su_Bit(n);
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

	// 完成した？
	bool isSolved() const {
		// 重複なし？
		if (hasError()) {
			su_SetConsoleTextAttr(TEXTATTR_ERR);
			printf("[エラー] 数字が重複しています");
			su_SetConsoleTextAttr(TEXTATTR_NONE);
			printf("\n\n");
			return false;
		}
		// 全てのマスに数字が入っている？
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				if (get(x, y) == 0) {
					return false;
				}
			}
		}
		return true;
	}

	// 正解条件を満たしている盤面を適当に作成する（すべてのマスに数字が埋まっている状態）
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

	// 問題を解くことができる？
	bool canSolve() {
		CSudokuGrid grid;
		grid.loadFromArray(m_num);
		while (grid.stepSolve()) {
		}
		return grid.isSolved();
	}

	// 「問題が解ける状態を維持したまま」ランダムで数字を一つ消す
	// どのマスを消しても問題が解けなくなってしまう場合は false を返す
	bool removeRandomOne() {
		// 数字が入っているセルのインデックスを並べる
		int pos[SU_SIZE] = {0};
		int cnt = 0;
		{
			for (int i=0; i<SU_SIZE; i++) {
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
			int tmp[SU_SIZE];
			su_Copy(tmp, m_num);

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

	// y0 と y1 にある行（横一列）を入れ替える
	void swapRow(int y0, int y1) {
		assert(isSolved());
		if (y0==y1) return;
		for (int x=0; x<9; x++) {
			std::swap(m_num[su_IndexOf(x, y0)], m_num[su_IndexOf(x, y1)]);
		}
		assert(isSolved());
	}

	// x0 と x1 にある列（縦一列）を入れ替える
	void swapCol(int x0, int x1) {
		assert(isSolved());
		if (x0==x1) return;
		for (int y=0; y<9; y++) {
			std::swap(m_num[su_IndexOf(x0, y)], m_num[su_IndexOf(x1, y)]);
		}
		assert(isSolved());
	}

	// 番号 n0 と n1 を全て入れ替える
	void swapNum(int n0, int n1) {
		assert(isSolved());
		if (n0==n1) return;
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				int i = su_IndexOf(x, y);
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
	// 行の9マスのうち8マスが既に埋まっているなら、残りの1マスの数字が確定できる
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
			setHow("Last cell in row");
			return true;
		}
		return false;
	}
	// 列の9マスのうち8マスが既に埋まっているなら、残りの1マスの数字が確定できる
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
			setHow("Last cell in column");
			return true;
		}
		return false;
	}
	// ブロックの9マスのうち8マスが既に埋まっているなら、残りの1マスの数字が確定できる
	// subx, suby ブロック番号。ブロックは 3x3 個あり、左から順に subx=0, 1, 2、上から順に suby=0, 1, 2 になる
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
			setHow("Last cell in block");
			return true;
		}
		return false;
	}
	// num しか入らないとわかっているマスがあるなら、そのマスの数字を num で確定する
	bool step_cell_uq(int num) {
		// そのマスには num しか入らない
		assert(1 <= num && num <= 9);
		for (int y=0; y<9; y++) {
			for (int x=0; x<9; x++) {
				if (isHintUnique(x, y, num)) { // このマスにあるヒントは num だけ ＝ このマスには num しか入る数字が無い ＝ このマスの数字は num で確定
					set(x, y, num);
					setHow("Uniq num");
					return true;
				}
			}
		}
		return false;
	}
	// 指定ブロック(3x3) にある9マスを調べる。
	// このうち、ヒントに num を含んでいるマスがただひとつしかないなら、num はそのマスにしか入らない
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
				if (hasHint(xx, yy, num)) {
					if (found) {
						// 二つめのマスが見つかってしまった。
						// num が入る可能性があるマスが複数あるのでダメ
						return false;
					}
					cx = xx; // num をヒントに持つマスを記録しておく
					cy = yy;
					found = true;
				}
			}
		}
		if (found) {
			// num をヒントに含むマスは一つしかなかった。
			// そのマスに入る数字は num で確定した
			set(cx, cy, num);
			setHow("Uniq in block");
			return true;
		}
		return false;
	}

	// 指定された行（横一列）にある9マスを調べる。
	// このうち、ヒントに num を含んでいるマスがただひとつしかないなら、num はそのマスにしか入らない
	bool step_row_uq(int y, int num) {
		assert(0 <= y && y < 9);
		assert(1 <= num && num <= 9);
		// この行に入る num は一か所しかない
		int cx=-1;
		bool found = false;
		for (int x=0; x<9; x++) {
			if (hasHint(x, y, num)) {
				if (found) {
					return false; // 重複
				}
				cx = x;
				found = true;
			}
		}
		if (found) {
			set(cx, y, num);
			setHow("Uniq in row");
			return true;
		}
		return false;
	}

	// 指定された列（縦一列）にある9マスを調べる。
	// このうち、ヒントに num を含んでいるマスがただひとつしかないなら、num はそのマスにしか入らない
	bool step_col_uq(int x, int num) {
		assert(0 <= x && x < 9);
		assert(1 <= num && num <= 9);
		// この列に入る num は一か所しかない
		int cy=-1;
		bool found = false;
		for (int y=0; y<9; y++) {
			if (hasHint(x, y, num)) {
				if (found) {
					return false; // 重複
				}
				cy = y;
				found = true;
			}
		}
		if (found) {
			set(x, cy, num);
			setHow("Uniq in column");
			return true;
		}
		return false;
	}
};

// 問題を作る（すでにパターンがあるものとする）
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
		printf("\n\n");
		for (char *c=s; *c; c++) {
			if (*c == '1') {
				if (grid.removeRandomOne()) {
					grid.print();
				} else {
					su_SetConsoleTextAttr(TEXTATTR_ERR);
					printf("これ以上数字を消せません\n");
					su_SetConsoleTextAttr(TEXTATTR_NONE);
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
				su_GetRandomIntPair(&a, &b);
				printf("Num %d <==> %d\n", a, b);
				grid.swapNum(a, b);
			//	grid.print();
			}
			if (*c == '2') {
				int a, b;
				su_GetRandomLinePair(&a, &b);
				printf("Col %d <==> %d\n", a, b);
				grid.swapCol(a, b);
			//	grid.print();
			}
			if (*c == '3') {
				int a, b;
				su_GetRandomLinePair(&a, &b);
				printf("Row %d <==> %d\n", a, b);
				grid.swapRow(a, b);
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
	char str[256] = {0};
	memset(str, 0, sizeof(str));
	printf("---------------------------------\n");
	printf("■数独の問題を解きます。\n");
	printf("　数字の配置を１行ごと（つまり９文字ずつ）入力していってください\n");
	printf("　1～9の数字以外は全て空白（数字が入っていないマス）になります\n");
	printf("　行末の空白は省略できます\n");
	printf("\n");
	printf("　例えば 1..3..5.. という入力と 1..3..5 は同じです（この場合は空白をドット . で表しています\n");
	printf("　　※ a と入力するとデモ問題 a を自動入力します。\n");
	printf("　　※ b と入力するとデモ問題 b を自動入力します。\n");
	printf("\n");
	for (int y=0; y<9; y++) {
		if (y % 3 == 0) {
			printf("------+--+--+--+\n");
		}
		char s[128] = {0};
		printf("[%d] > ", 1+y);
		fgets(s, 256, stdin);
		if (strcmp(s, "a\n") == 0) { // 改行に注意
			strcpy_s(str, sizeof(str), su_SampleGridA);
			break;
		}
		if (strcmp(s, "b\n") == 0) { // 改行に注意
			strcpy_s(str, sizeof(str), su_SampleGridB);
			break;
		}
		// 長すぎるなら切り落とす。ただし改行は残しておく
		if (strlen(s) > 9+1) { // 改行を含めた長さであることに注意
			s[9] = '\n';
			s[10] = '\0';
		}
		strcat_s(str, sizeof(str), s);
	}

	CSudokuGrid grid;
	grid.loadFromString(str);
	grid.print();

	if (grid.hasError()) {
		su_SetConsoleTextAttr(TEXTATTR_ERR);
		printf("[エラー] 数字が重複しています");
		su_SetConsoleTextAttr(TEXTATTR_NONE);
		printf("\n\n");
	}

	do {
		grid.stepSolve();
		grid.print();

		if (grid.isSolved()) {
			printf("SOLVED!!\n");
		//	break;
		} else {
			printf("★エンターキーを押してください。1段階づつ解いていきます\n");
		}
	} while (getchar());
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