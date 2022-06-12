/********************************************************
*	Strategy.h : 策略接口文件                           *
*	张永锋                                              *
*	zhangyf07@gmail.com                                 *
*	2014.5                                              *
*********************************************************/

#ifndef STRATEGY_H_
#define STRATEGY_H_

#include "Point.h"
#include <cstddef>

extern "C" Point *getPoint(const int M, const int N, const int *top, const int *_board,
						   const int lastX, const int lastY, const int noX, const int noY);

extern "C" void clearPoint(Point *p);

void clearArray(int M, int N, int **board);

/*
	添加你自己的辅助函数
*/



void printBoard();

void copyBoardInfo(const int M, const int N, const int* top, int** board,
	const int _lastX, const int _lastY, const int _noX, const int _noY);

void MCT(const int player, const int curr);

int getBestColumn();

#endif