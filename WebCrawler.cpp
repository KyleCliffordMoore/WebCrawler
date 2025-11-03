// HTMLParserTest.cpp
// Example program showing how to use the parser
// CSCE 463 sample code
//
#include "pch.h"

// Ima use these a lot...
using std::string;
using std::cout;

std::set<string> visitedHosts;
std::set<unsigned long> visitedIPAddresses;

std::atomic<size_t> Q{ 0 }; // URLs in queue
std::atomic<size_t> E{ 0 }; // URLs extracted
std::atomic<size_t> H{ 0 }; // URLs with unique hosts
std::atomic<size_t> D{ 0 }; // DNS lookups
std::atomic<size_t> I{ 0 }; // URLs with unique IP addresses
std::atomic<size_t> R{ 0 }; // Passed Robots
std::atomic<size_t> C{ 0 }; // successfully crawled URLs
std::atomic<size_t> L{ 0 }; // Links found
std::atomic<size_t> bits{ 0 };

std::atomic<size_t> _2xx{ 0 }; // 2xx responses
std::atomic<size_t> _3xx{ 0 }; // 3xx responses
std::atomic<size_t> _4xx{ 0 }; // 4xx responses
std::atomic<size_t> _5xx{ 0 }; // 5xx responses
std::atomic<size_t> _other{ 0 }; // other responses


std::mutex coutMutex;


class URLs {
	std::queue<string> urls;
	mutable std::mutex myMutex;
	std::condition_variable cond_var;
	bool finished = false;
	
public:
	void push(const string& url) {
		{
			std::lock_guard<std::mutex> lockguard(myMutex);
			urls.push(url);
		}
		Q.fetch_add(1);
		cond_var.notify_one();
	}
	
	bool pop(string& outUrl) {
		std::unique_lock<std::mutex> lockguard(myMutex);

		// 313 stuff
		cond_var.wait(lockguard, [this]() {return !this->urls.empty() || finished; });
		if (urls.empty())
			return false;

		outUrl = move(urls.front());
		urls.pop();
		Q.fetch_sub(1);
		return true;
	}

	void setFinished() {
		{
			std::lock_guard<std::mutex> lockguard(myMutex);
			finished = true;
		}
		cond_var.notify_all();
	}

	size_t getSize() const {
		std::lock_guard<std::mutex> lockguard(myMutex);
		return urls.size();
	}
};

std::mutex hostMutex;
bool isUniqueHost(const string& host) {
	std::lock_guard<std::mutex> lockguard(hostMutex);
	if (visitedHosts.insert(host).second) {
		H.fetch_add(1);
		return true;
	}
	return false;
}
std::mutex ipMutex;
bool isUniqueIPAddress(unsigned long ipAddress) {
	std::lock_guard<std::mutex> lockguard(ipMutex);
	if (visitedIPAddresses.insert(ipAddress).second) {
		I.fetch_add(1);
		return true;
	}
	return false;
}

string getUnsafeURLFromUser(int argc, char* argv[]) {

	if (argc < 2) {
		// cout << "Invalid, please enter a URL!";
		exit(1);
	}

	string url = argv[1];

	//// cout << "URL: " << url << '\n';
	return url;
}

string getFilePathFromUser(int argc, char* argv[]) {

	//if (argv[1][0] != '1') {
	//	// cout << "Invalid, hw1 part 2 only supports one thread, please enter 1\n";
	//	exit(1);
	//}

	return string(argv[2]);
}

bool parseURL(const string& url, string& host, int& port, string& path, string& query) {
	
	// cout << "\tParsing URL... ";

	string _host;
	int _port;
	string _path;
	string _query;
	
	string parseURL;


	// Check Scheme
	const string httpScheme = "http://";
	if (url.rfind(httpScheme, 0)) {
		cout << url;
		return false;
	}
	parseURL = url.substr(httpScheme.size(), url.size());



	// Remove Fragment
	size_t fragmentIndex = parseURL.find('#', 0);
	if (fragmentIndex != string::npos)
		parseURL = parseURL.substr(0, fragmentIndex);



	// Get Query
	size_t queryIndex = parseURL.find('?');
	if (queryIndex != string::npos) {
		
		_query = parseURL.substr(queryIndex);
		parseURL = parseURL.substr(0, queryIndex);
	}



	// Host / Port
	_path = "/";
	_port = 80;
	string hostAndPort;
	size_t rootIndex = parseURL.find('/');
	
	// Does it following [host:port] scheme?
	if (rootIndex != string::npos) {
		_path = parseURL.substr(rootIndex);
		hostAndPort = parseURL.substr(0, rootIndex);
	}
	else hostAndPort = parseURL;

	// Is ths hostAndPort missing?
	if (hostAndPort.size() == 0) {
		// cout << "failed with invalid host\n"; //I dont think this will happen - not in his examples
		return false;
	}

	size_t colonIndex = hostAndPort.find(':');
	if (colonIndex != string::npos) {
		_host = hostAndPort.substr(0, colonIndex);
		string portAsString = hostAndPort.substr(colonIndex + 1);
		if (portAsString.size() == 0) {
			// cout << "failed with invalid port\n";
			return false;
		}
		_port = std::stoi(portAsString);
		if (0 >= _port || _port > MAXSHORT) {
			// cout << "failed with invalid port\n";
			return false;
		}
	}
	else _host = hostAndPort;

	if (_host.size() > MAX_HOST_LEN) {
		// cout << "failed with invalid host\n";
		return false;
	}

	host  = _host;
	port =  _port;
	path =  _path;
	query = _query;

	// dont need request anymore
	// cout << "host " << host << ", port " << port << "\n";//, request " << path << query << '\n';
	if (path.back() == '\r')
		path.pop_back();

	return true;
}

// my atomic run counter
std::atomic<int> atomicRunCounter = 0;

//dns mutex
std::mutex dnsMutex;
bool resolveHost(const string& host, unsigned long& address) {
	
	clock_t startTime = clock();
	// host is something like tamu.edu

	address = inet_addr(host.c_str());
	if (address == INADDR_NONE) {
		
		struct hostent* remote = NULL;
		{
			//std::lock_guard<std::mutex> lockguard(dnsMutex);
			remote = gethostbyname(host.c_str());
		}

		if (remote == NULL) {
			return false;
		}
		memcpy(&address, remote->h_addr, remote->h_length);
		D.fetch_add(1);

		//cout << "addr as string: " << inet_ntoa(*(struct in_addr*)remote->h_addr) << '\n';
	}
	atomicRunCounter.fetch_add(1);

	clock_t endTime = clock();
	double deltaTime = CALC_TIME(startTime, endTime);

	struct in_addr addressTemp;
	addressTemp.s_addr = address;
	char* ipAsString = inet_ntoa(addressTemp);

	if (!isUniqueIPAddress(address))
		return false;

	
	return true;
}

string createHTTPRequest(string regType, const string& host, const string& path) {

	string req = regType + " " + path + " HTTP/1.0\r\n";
    req += "User-agent: myHWWebCrawler/1.0\r\n";
	req += "Host: " + host + "\r\n";
	req += "Connection: close\r\n\r\n";

	return req;
}

bool getHeaderAndStatusCode(const string& httpResponse, string& header, string& statusCode) {
	// Header
	// cout << "\tVerifying header... ";
	size_t endOfHeaderIndex = httpResponse.find("\r\n\r\n");
	header = "";
	header = httpResponse.substr(0, endOfHeaderIndex);


	// Status Code
	std::string firstLine = header.substr(0, header.find("\r\n"));
	size_t space1 = firstLine.find(' ');
	size_t space2 = firstLine.find(' ', space1 + 1);

	if (space1 == string::npos || space2 == string::npos)
		return false;

	statusCode = firstLine.substr(space1 + 1, space2 - (space1 + 1));
	// cout << "status code " << statusCode << '\n';

	return true;
}

bool recvInfo(
	const SOCKET& mySocket, 
	char*& httpResponse, 
	int& bytesRead, 
	int& currBuffSize,
	int INITIAL_BUFF_SIZE,
	int MAX_BUFF_SIZE
) {
	int recvCount = 0;
	
	fd_set fileDescriptors;
	struct timeval timeVal;

	if (currBuffSize > INITIAL_BUFF_SIZE) {
		delete[] httpResponse;
		httpResponse = new char[INITIAL_BUFF_SIZE];
		currBuffSize = INITIAL_BUFF_SIZE;
	}

	while (true /* Scary */) {

		if (recvCount > MAX_BUFF_SIZE) {
			cout << recvCount << " > " << MAX_BUFF_SIZE << '\n';
			cout << "here\n";
			closesocket(mySocket);
			return false;
		}

		if (currBuffSize <= recvCount + 1024) {
			char* newBuffer = new char[currBuffSize * 2];
			memcpy(newBuffer, httpResponse, recvCount);
			delete[] httpResponse;
			currBuffSize *= 2;
			httpResponse = newBuffer;
		}

		FD_ZERO(&fileDescriptors);
		FD_SET(mySocket, &fileDescriptors);
		timeVal.tv_sec = 10;
		timeVal.tv_usec = 0;
		int returnVal = select(0, &fileDescriptors, NULL, NULL, &timeVal);
		// Happy case
		if (returnVal > 0) {

			// Happy normal code
			int bytesReadCount = recv(mySocket, httpResponse + recvCount, currBuffSize - recvCount - 1, 0);

			if (bytesReadCount > 0)
				recvCount += bytesReadCount;
			else if (bytesReadCount == 0)
				break;
			else {
				closesocket(mySocket);
				return false;
			}

		}
		else if (returnVal == 0) {
			closesocket(mySocket);
			return false;
		}
		else {
			closesocket(mySocket);
			return false;
		}

	}

	httpResponse[recvCount] = '\0';
	bytesRead = recvCount;
	bits.fetch_add(bytesRead * 8);

	// make sure it returned a HTTP header
	if (strncmp(httpResponse, "HTTP/", 5) != 0)
		return false;
	
	return true;
}

bool connectAndRecvInfo(
	string reqType, 
	unsigned long address, 
	const string& host, 
	const int& port, 
	const string& path, 
	const string& query, 
	char*& httpResponse, 
	int& bytesRead,
	int& currBuffSize
) {

	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = address;
	server.sin_port = htons((u_short)port);
	SOCKET mySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	if (connect(mySocket, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
		closesocket(mySocket);
		return false;
	}

	//string test = path + query;
	
	
	string request = createHTTPRequest(reqType, host, path);

	if (request.size() > MAX_REQUEST_LEN) {
		closesocket(mySocket);
		return false;
	}

	if (send(mySocket, request.c_str(), (int)request.size(), 0) == SOCKET_ERROR) {
		closesocket(mySocket);
		return false;
	}

	//int maxSize = reqType == "GET" ? 2 * 1024 * 1024 : 16 * 1024;
	int maxSize = INT_MAX;

	if (!recvInfo(mySocket, httpResponse, bytesRead, currBuffSize, 4096, maxSize))
		return false;

	closesocket(mySocket);
	return true;
}

bool getPageWithSock(const string& host, const int& port, const string& path, const string& query, char*& httpResponse, int& bytesRead, int& currBuffSize) {

	// Resolve Host
	unsigned long address = INADDR_NONE;
	if (!resolveHost(host, address))
		return false;


	// Check Robots
	if (!connectAndRecvInfo("HEAD", address, host, port, "/robots.txt", query, httpResponse, bytesRead, currBuffSize))
		return false;


	string header = "";
	string statusCode = "";
	getHeaderAndStatusCode(httpResponse, header, statusCode);
	if (statusCode.size() != 3 || statusCode[0] != '4') 
		return false;
	R.fetch_add(1);


	//Continue to GET page
	if (!connectAndRecvInfo("GET", address, host, port, path, query, httpResponse, bytesRead, currBuffSize))
		return false;
	//cout << httpResponse << '\n';

	C.fetch_add(1);
	return true;
}

bool getInfoFromResponse(char* httpGetResponse, int& bytesRead, const string& baseURL) {
	
	std::string header = "";
	std::string statusCode = "";
	if (!getHeaderAndStatusCode(httpGetResponse, header, statusCode))
		return false;

	//cout << "Status code: " << statusCode << '\n';
	if (statusCode.size() != 3) {
		_other.fetch_add(1);
		return false;
	}
	else if (statusCode[0] == '2')
		_2xx.fetch_add(1);
	else if (statusCode[0] == '3')
		_3xx.fetch_add(1);
	else if (statusCode[0] == '4')
		_4xx.fetch_add(1);
	else if (statusCode[0] == '5')
		_5xx.fetch_add(1);
	else
		_other.fetch_add(1);

	if (statusCode[0] != '2')
		return false;

	// Parse page

	clock_t startTime = clock();

	HTMLParserBase* parser = new HTMLParserBase;
	//std::cout << parser << '\n';
	int nLinks = 0;

	char* linkBuffer = parser->Parse(
		httpGetResponse,
		bytesRead,
		(char*)(baseURL.c_str()), 
		(int) baseURL.size(), 
		&nLinks
	);
	if (nLinks < 0)
		nLinks = 0;

	L.fetch_add(nLinks);

	clock_t endTime = clock();

	double deltaTime = CALC_TIME(startTime, endTime);



	delete parser;		// this internally deletes linkBuffer

	return true;
}

std::atomic<int> actThreads = 0;

void statsThread() {

	int ticks = 0;
	int lastC = 0;
	size_t lastBits = 0;
	// TODO change this!!!
	std::this_thread::sleep_for(std::chrono::seconds(2));
	while (actThreads) {
		
		ticks += 2;

		int tempBits = bits.load();
		int tempC = C.load();
		std::cout 
			<< "[" << std::setw(3) << ticks << "] "
			<< " Q " << std::setw(6) << Q.load() 
			<< " E " << std::setw(7) << E.load() 
			<< " H " << std::setw(6) << H.load() 
			<< " D " << std::setw(6) << D.load() 
			<< " I " << std::setw(5) << I.load() 
			<< " R " << std::setw(5) << R.load() 
			<< " C " << std::setw(5) << C.load() 
			<< " L " << std::setw(4) << L.load() / 1000 << 'K'
			//<< " Active Threads: " << actThreads.load() << '\n'
			;

		
		int deltaC = tempC - lastC;
		int deltaB = tempBits - lastBits;

		std::cout
			<< "\n*** crawling "
			<< std::setw(6) << deltaC / 2 << " pps @ "
			<< std::setw(4) << deltaB / 1000.0 / 1000.0 << " Mbps\n";

		lastC = tempC;
		lastBits = tempBits;


		std::this_thread::sleep_for(std::chrono::seconds(2));
	/*	std::cout << "Atomic Run Counter: " << atomicRunCounter.load() << '\n';*/
	}
	std::cout << "Extracted " << E << " URLs @ " << E / double(ticks) << "/s\n";
	std::cout << "Looked up " << H << " DNS names @ " << H / double(ticks) << "/s\n";
	std::cout << "Attempted " << I << " robots @ " << I / double(ticks) << "/s\n";
	std::cout << "Crawled " << _2xx + _3xx + _4xx + _5xx + _other << " pages @ " << (_2xx + _3xx + _4xx + _5xx + _other) / double(ticks) << "/s (" << bits / 8.0 / 1000.0 / 1000.0 << "MB)\n";
	std::cout << "Parsed " << L.load() << " links @ " << L.load() / double(ticks) << "/s\n";
	std::cout << "HTTP codes: ";

	std::cout 
		<< "2xx = " << _2xx.load()
		<< ", 3xx = " << _3xx.load()
		<< ", 4xx = " << _4xx.load()
		<< ", 5xx = " << _5xx.load()
		<< ", other = " << _other.load() << '\n';

}


void worker(URLs &urls) {
	actThreads.fetch_add(1);
	 //cout << "Thread started\n";
	string url = "";

	int currBuffSize = 4096;
	char* httpResponse = new char[currBuffSize];

	while (urls.pop(url)) {
		E.fetch_add(1);
		if (url.empty())
			continue;
		// cout << "got here\n";

		string host;
		int port;
		string path;
		string query;

		if (!parseURL(url, host, port, path, query))
			continue;

		if (!isUniqueHost(host))
			continue;
		
		

		int bytesRead = 0;
		if (!getPageWithSock(host, port, path, query, httpResponse, bytesRead, currBuffSize))
			continue;


		if (!getInfoFromResponse(httpResponse, bytesRead, url /*Is this the base url?*/))
			continue;
		
	}

	delete[] httpResponse;
	//cout << "thread done\n";
	actThreads.fetch_sub(1);
}

int main(int argc, char** argv)
{

	// Start the Windows Socket API
	// TODO: Will have to move this logic out when running multiple times!
	WSADATA wsaData;
	WORD wVersionReq = MAKEWORD(2, 2); // I don't really get MAKEWORD?
	if (WSAStartup(wVersionReq, &wsaData) != 0) {
		return 1;
	}

	if (argc == 2) {
		
		string url = getUnsafeURLFromUser(argc, argv);
		 cout << "URL: " << url << '\n';
		string host;
		int port;
		string path;
		string query;

		if (!parseURL(url, host, port, path, query)) {
			WSACleanup();
			cout << "failed to parse url\n";
			return 2;
		}
		
		cout << host << '\n';

		 cout << "\tChecking host uniqueness... ";
		if (!isUniqueHost(host)) {
			 cout << "failed\n";
			WSACleanup();
			return 3;
		}
		 cout << "passed\n";

		int currBuffSize = 4096;
		int bytesRead = 0;
		char* httpResponse = new char[currBuffSize];
		if (!getPageWithSock(host, port, path, query, httpResponse, bytesRead, currBuffSize)) {
			WSACleanup();
			delete[] httpResponse;
			return 4;
		}

		if (!getInfoFromResponse(httpResponse, bytesRead, url /*Is this the base url?*/)) {
			WSACleanup();
			delete[] httpResponse;
			return 5;
		}
			
		delete[] httpResponse;
	}
	else if (argc == 3) {
		int numThread = std::stoi(argv[1]);
		string urlFileName = getFilePathFromUser(argc, argv);

		URLs urls;

		std::ifstream urlsFile(urlFileName, std::ios::binary);
		
		if (!urlsFile) {
			 cout << "Can not open " << urlFileName << '\n';
			exit(7);
		}

		urlsFile.seekg(0, std::ios::end);
		 cout << "Opened " << urlFileName << " with size " << urlsFile.tellg() << '\n';
		urlsFile.seekg(0, std::ios::beg);

		string url = "";
		while (std::getline(urlsFile, url)) {
			if (url.empty())
				break;
			urls.push(url);
		}
		urls.setFinished();

		// start stats thread
		std::thread stats(statsThread);

		std::vector<std::thread> workers;
		workers.reserve(numThread);
		for (int i = 0; i < numThread; i++)
			workers.emplace_back(worker, std::ref(urls));
		for (auto& workerThread : workers)
			workerThread.join();

		// end stats thread
		//stats.detach();
		stats.join();
	}
	else {
		// cout << "Invalid number of arguments\n Example: run.exe 1 <filepath>\n";
		WSACleanup();
		exit(1);
	}

	WSACleanup();
	return 0;
}

