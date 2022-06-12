#include <unistd.h>
#include <cmath>
#include <cstdlib>
#include <tuple>
#include <time.h>

#include "Point.h"
#include "Strategy.h"
#include "Judge.h"

using namespace std;

const int MAX_NODE = 5000000;
const double TIME = 2.;  // time limit of MCTS
const int MAX_BOARD_SIZE = 12;
const double UCT_C = 1/1.414;  // param for score(curr)/visit(curr) + "C" * sqrt(2* ln(visit(parent)) / visit(curr))
const int GAME_WIN = 2;
const int GAME_TIE = 1;
const int GAME_CONTINUE = 0;

int num_node = 0; // number of total situations(nodes)
int tree_node[MAX_NODE][MAX_BOARD_SIZE]; // MCT nodes, N-ary tree
int score[MAX_NODE][MAX_BOARD_SIZE]; // scores of the nodes
int visit[MAX_NODE];
int feasible[MAX_BOARD_SIZE];
int unvisited_feasible[MAX_BOARD_SIZE];

int** board;
int top[MAX_BOARD_SIZE];
int M, N;
int noX, noY;

namespace stack
{
	int tree_stack[MAX_BOARD_SIZE * MAX_BOARD_SIZE];
	int board_stack[MAX_BOARD_SIZE * MAX_BOARD_SIZE];
	int stack_top = 0;

	inline void clearStack(){
		stack_top = 0;
	}

	inline void pushStack(const int &node, const int &col){
		tree_stack[stack_top++] = node;
		board_stack[stack_top - 1] = col;
	}

	inline int stackSize(){
		return stack_top;
	}

	inline std::pair<int, int> popStack(){
		if (--stack_top >= 0)
			return { tree_stack[stack_top], board_stack[stack_top] };
		else
			return { -1, -1 };
	}
}

/*
	策略函数接口,该函数被对抗平台调用,每次传入当前状态,要求输出你的落子点,该落子点必须是一个符合游戏规则的落子点,不然对抗平台会直接认为你的程序有误
	
	input:
		为了防止对对抗平台维护的数据造成更改，所有传入的参数均为const属性
		M, N : 棋盘大小 M - 行数 N - 列数 均从0开始计， 左上角为坐标原点，行用x标记，列用y标记
		top : 当前棋盘每一列列顶的实际位置. e.g. 第i列为空,则_top[i] == M, 第i列已满,则_top[i] == 0
		_board : 棋盘的一维数组表示, 为了方便使用，在该函数刚开始处，我们已经将其转化为了二维数组board
				你只需直接使用board即可，左上角为坐标原点，数组从[0][0]开始计(不是[1][1])
				board[x][y]表示第x行、第y列的点(从0开始计)
				board[x][y] == 0/1/2 分别对应(x,y)处 无落子/有用户的子/有程序的子,不可落子点处的值也为0
		lastX, lastY : 对方上一次落子的位置, 你可能不需要该参数，也可能需要的不仅仅是对方一步的
				落子位置，这时你可以在自己的程序中记录对方连续多步的落子位置，这完全取决于你自己的策略
		noX, noY : 棋盘上的不可落子点(注:涫嫡饫锔?龅膖op已经替你处理了不可落子点，也就是说如果某一步
				所落的子的上面恰是不可落子点，那么UI工程中的代码就已经将该列的top值又进行了一次减一操作，
				所以在你的代码中也可以根本不使用noX和noY这两个参数，完全认为top数组就是当前每列的顶部即可,
				当然如果你想使用lastX,lastY参数，有可能就要同时考虑noX和noY了)
		以上参数实际上包含了当前状态(M N _top _board)以及历史信息(lastX lastY),你要做的就是在这些信息下给出尽可能明智的落子点
	output:
		你的落子点Point
*/
extern "C" Point *getPoint(const int _M, const int _N, const int* _top, const int* _board,
						   const int _lastX, const int _lastY, const int _noX, const int _noY)
{
	stack::clearStack();
	for (int i = 0; i < num_node; ++i){
		visit[i] = 0;
		for (int j = 0; j < N; ++j) score[i][j] = 0, tree_node[i][j] = 0;
	}
	num_node = 1;
	M = _M, N = _N;
	noX = _noX, noY = _noY;
	for (int i = 0; i < _N; ++i) top[i] = _top[i];

	/*
		不要更改这段代码
	*/
	int x = -1, y = -1; //最终将你的落子点存到x,y中
	board = new int *[M];
	for (int i = 0; i < M; i++)
	{
		board[i] = new int[N];
		for (int j = 0; j < N; j++)
		{
			board[i][j] = _board[i * N + j];
		}
	}

	/*
		根据你自己的策略来返回落子点,也就是根据你的策略完成对x,y的赋值
		该部分对参数使用没有限制，为了方便实现，你可以定义自己新的类、.h文件、.cpp文件
	*/
	clock_t start = clock();
	while ((clock() - start) < (TIME * CLOCKS_PER_SEC) && num_node < MAX_NODE - 300) MCT(2, 0);
	y = getBestColumn();
	if (y == -1){
		for(y=0;y<N;y++)
			if(top[y]>0) break;
	}
	x=top[y] - 1;

	/*
		不要更改这段代码
	*/
	clearArray(M, N, board);
	return new Point(x, y);
}

/*
	getPoint函数返回的Point指针是在本so模块中声明的，为避免产生堆错误，应在外部调用本so中的
	函数来释放空间，而不应该在外部直接delete
*/
extern "C" void clearPoint(Point *p)
{
	delete p;
	return;
}

/*
	清除top和board数组
*/
void clearArray(int M, int N, int **board)
{
	for (int i = 0; i < M; i++)
	{
		delete[] board[i];
	}
	delete[] board;
}

/*
	添加你自己的辅助函数，你可以声明自己的类、函数，添加新的.h .cpp文件来辅助实现你的想法
*/





inline int relu(const int &val)
{
	return val >= 1 ? val : 1;
}



// int board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];

// int lastX, lastY;

/********** Local Helpers ***********
*************************************/

inline void placeChess(const int &y, const int &player)
{
	board[--top[y]][y] = player;
	if (noX == top[y] - 1 && noY == y)
		--top[y];
}

inline void removeChess(const int &y)
{
	if (noX == top[y] && noY == y)
		++top[y];
	board[top[y]++][y] = 0;
}

inline bool colFull(const int &y)
{
	return top[y] == 0;
}




inline int gameOver(const int &player, int x, int y)
{
	auto judge = player == 1 ? userWin : machineWin;
	if (judge(x, y, M, N, (int**)board)) return GAME_WIN;
	else
	{
		for (int i = 0; i < N; ++i)
			if (!colFull(i))
				return GAME_CONTINUE;
		return GAME_TIE;
	}
}

// find a point either machine or human wins
inline int evalBestPoint(const int &player)
{
	int best = -1;
	for (int y = 0; y < N; ++y){
		int x = top[y] - 1;
		if (x >= 0){
			placeChess(y, player);
			int code = gameOver(player, x, y);
			removeChess(y);
			if (code == GAME_WIN)
			{
				if (best == -1)
					best = y;
				else
				{
					best = best * N + y; break; // in this case, wherever 3 - player puts, he's doomed to fail
				}
			}
		}
	}
	return best;
}

inline int UCTBestChild(const int &curr, const size_t &feasible_cnt)
{
	// use UCT to evaluate best child
	double max_score = -INFINITY;
	int candidate = -1;
	for (size_t i = 0; i < feasible_cnt; ++i)
	{
		double val = static_cast<double>(score[curr][feasible[i]]) / visit[tree_node[curr][feasible[i]]] + UCT_C * std::sqrt(2 * std::log(visit[curr]) / visit[tree_node[curr][feasible[i]]]);
		if (val > max_score)
			max_score = val, candidate = feasible[i];
	}
	return candidate;
}

int treePolicy(int &player, int &curr)
{
	while (true)
	{
		size_t cnt = 0;
		int best = evalBestPoint(3 - player);
		if (best >= 0 && best < N)
			feasible[0] = best, cnt = 1;
		best = evalBestPoint(player);
		if (best >= 0)
		{
			best %= N;
			if (!tree_node[curr][best])
				tree_node[curr][best] = num_node++;
			placeChess(best, player);
			stack::pushStack(curr, best);
			return GAME_WIN;
		}
		if (!cnt)
		{
			for (int i = 0; i < N; ++i)
				if (!colFull(i))
					feasible[cnt++] = i;
		}
		if (cnt)
		{
			size_t unvisited_cnt = 0;
			for (size_t i = 0; i < cnt; ++i)
			{
				if (!tree_node[curr][feasible[i]])
					unvisited_feasible[unvisited_cnt++] = feasible[i];
			}
			if (!unvisited_cnt) // all visited, use UCT to select best
			{
				int child = UCTBestChild(curr, cnt);
				placeChess(child, player);
				stack::pushStack(curr, child);
				curr = tree_node[curr][child];
				player = 3 - player;
				continue; // continue to play
			}
			else // have unvisited child, randomly pick up one
			{
				int child = unvisited_feasible[rand() % unvisited_cnt];
				placeChess(child, player);
				stack::pushStack(curr, child);
				tree_node[curr][child] = num_node;
				curr = num_node++ - 1;
				return GAME_CONTINUE;
			}
		}
		else return GAME_TIE;
	}
}

void MCT(const int _player, const int _curr)
{
	int player = _player, curr = _curr;
	int winner = 0;
	int stat = treePolicy(player, curr);
	int bonus = 0;
	int player_tree_node = player;

	constexpr int REWARD = 5;
	if (stat == GAME_CONTINUE){
		int sim_cnt = 0;
		while (true)
		{
			player = 3 - player;
			++sim_cnt;
			int f_cnt = 0;
			int best = evalBestPoint(3 - player);
			if (best >= 0 && best < N)
				feasible[0] = best, f_cnt = 1;
			best = evalBestPoint(player);
			if (best >= 0)
				feasible[0] = best % N, f_cnt = 1;
			if (!f_cnt)
			{
				for (int i = 0; i < N; ++i)
					if (!colFull(i))
						feasible[f_cnt++] = i;
			}
			if (f_cnt)
			{
				int choose = feasible[rand() % f_cnt];
				int x = top[choose] - 1;
				placeChess(choose, player);
				stack::pushStack(-1, choose);
				stat = gameOver(player, x, choose);
			}
			else
			{
				winner = 0;
				break;
			}
			if (stat == GAME_TIE)
			{
				winner = 0;
				break;
			}
			else if (stat == GAME_WIN)
			{
				winner = player;
				bonus = sim_cnt == 1 ? REWARD : 1;
				break;
			}
		}
	}
	else if (stat == GAME_WIN)
		winner = player_tree_node, bonus = REWARD;
	player = player_tree_node;
	// backup
	while (stack::stackSize())
	{
		auto parent = stack::popStack();
		removeChess(parent.second);
		if (parent.first == -1)
			continue;
		if (winner == player)
		{
			score[parent.first][parent.second] += (1 + bonus);
		}
		else if (winner == 3 - player)
		{
			score[parent.first][parent.second] -= relu(bonus - 1);
		}
		if (bonus > 0)
			--bonus;
		player = 3 - player;
		++visit[parent.first], ++visit[tree_node[parent.first][parent.second]];
	}
}

int getBestColumn()
{
	double max_score = -1;
	int best_column = -1;
	for (int i = 0; i < N; ++i){
		if (!tree_node[0][i]) continue;
		double temp_score = double(score[0][i]) /visit[tree_node[0][i]];
		if (temp_score > max_score && top[i]!=0)
			max_score = temp_score, best_column = i;
	}
	return best_column;
}