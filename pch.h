// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H


#define PRINTLN(thingToPrint) std::cout << thingToPrint << '\n'
#define MY_WEBCRAWLER_AGENT "myHWWebCrawler/1.0"

// is clock per second ever not 1k?
#define CALC_TIME(startTime, endTime) double(1000) * ((endTime - startTime) / double(CLOCKS_PER_SEC))

#define _WINSOCK_DEPRECATED_NO_WARNINGS

// add headers that you want to pre-compile here
#include <iostream>
#include <string>
#include <set>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <iomanip>

#include "HTMLParserBase.h"
#include "stdafx.h"

#include "WinSock2.h"
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")


#include <cstring>

#endif //PCH_H
